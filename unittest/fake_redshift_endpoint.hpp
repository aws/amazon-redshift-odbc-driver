/*-------------------------------------------------------------------------
*
* Copyright(c) 2026, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
*-------------------------------------------------------------------------
*/

// In-process fake Redshift endpoint for the hostname-mismatch tests in tls_policy_test.cpp.
// Certificates are minted with the OpenSSL the driver already links, so no external tools.

#ifndef __FAKE_REDSHIFT_ENDPOINT_H__
#define __FAKE_REDSHIFT_ENDPOINT_H__

#ifdef WIN32
// winsock2.h has to come before windows.h, which otherwise pulls in winsock.h;
// NOCRYPT keeps out wincrypt.h, whose X509_NAME macro breaks the OpenSSL headers.
#ifndef NOCRYPT
#define NOCRYPT
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#endif

#include <sql.h>
#include <sqlext.h>
#include <sqlucode.h>

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include <atomic>
#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

// MSVC spells the POSIX file and process calls with a leading underscore.
#ifdef WIN32
#include <direct.h>
#include <io.h>
#include <process.h>
#define RS_MKDIR(p) _mkdir(p)
#define RS_RMDIR(p) _rmdir(p)
#define RS_UNLINK(p) _unlink(p)
#define RS_DUP(fd) _dup(fd)
#define RS_DUP2(a, b) _dup2((a), (b))
#define RS_FILENO(f) _fileno(f)
#define RS_CLOSE(fd) _close(fd)
#define RS_GETPID() _getpid()
typedef SOCKET RS_SOCKET;
#define RS_NO_SOCKET INVALID_SOCKET
#define RS_CLOSE_SOCKET(s) closesocket(s)
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#define RS_MKDIR(p) mkdir((p), 0700)
#define RS_RMDIR(p) rmdir(p)
#define RS_UNLINK(p) unlink(p)
#define RS_DUP(fd) dup(fd)
#define RS_DUP2(a, b) dup2((a), (b))
#define RS_FILENO(f) fileno(f)
#define RS_CLOSE(fd) close(fd)
#define RS_GETPID() getpid()
typedef int RS_SOCKET;
#define RS_NO_SOCKET (-1)
#define RS_CLOSE_SOCKET(s) close(s)
#endif

namespace rsfake
{

// Logged when a mismatch is tolerated, and the error when verify-full was asked for.
const char *const TOLERATED_WARNING = "does not name host";
const char *const REJECTED_ERROR = "does not match host name";
const char *const CHAIN_REJECTED_ERROR = "CERTIFICATE_VERIFY_FAILED";

// Distinctive so a leak of it is easy to spot.
const char *const PROBE_PASSWORD = "SECRET-MARKER-DO-NOT-LEAK";

// Caps each wait on the driver, so a test that fails before connecting can't hang the suite.
const int ENDPOINT_TIMEOUT_SECONDS = 30;

// The tests link the driver directly, so the Driver keyword only has to be present.
const char *const DIRECT_DRIVER_CLAUSE = "Driver=Amazon Redshift ODBC Driver;";

const char *const GOOD_NAME = "localhost";
const char *const WRONG_NAME = "attacker.example";

// Fixed name, so the log can be read without listing the directory. The ".1"
// sibling is libpq's connection trace, written at LogLevel 4 and above.
const char *const DRIVER_LOG_NAME = "redshift_odbc.log";

/**
 * @brief Braces a connection-string value so a ';' in it doesn't end the attribute,
 *        doubling any '}' as the driver's parser expects.
 * @param value The raw value.
 * @return The braced value.
 */
inline std::string connStrValue(const std::string &value)
{
    std::string braced = "{";
    for (char ch : value)
    {
        braced.push_back(ch);
        if (ch == '}')
        {
            braced.push_back('}');
        }
    }
    return braced + "}";
}

/**
 * @brief Finds the temp directory from TMPDIR, TEMP or TMP.
 * @return The first one set, or "/tmp".
 */
inline std::string tempRoot()
{
    const char *candidates[] = {"TMPDIR", "TEMP", "TMP"};
    for (const char *name : candidates)
    {
        const char *value = getenv(name);
        if (value != NULL && value[0] != '\0')
        {
            return std::string(value);
        }
    }
    return "/tmp";
}

/**
 * @brief Builds a temp path unique within this process; avoids mkdtemp, which
 *        Windows lacks.
 * @param prefix Start of the file or directory name.
 * @return The path; nothing is created.
 */
inline std::string uniquePath(const char *prefix)
{
    static int next = 0;
    return tempRoot() + "/" + prefix + std::to_string((long)RS_GETPID()) + "_" +
           std::to_string(next++);
}

/**
 * @brief Reads a whole file.
 * @param path The file to read.
 * @return Its contents, or "" if it can't be opened.
 */
inline std::string readFileIfPresent(const std::string &path)
{
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in)
    {
        return std::string();
    }
    return std::string(std::istreambuf_iterator<char>(in),
                       std::istreambuf_iterator<char>());
}

/**
 * @brief Reads libpq's connection trace followed by the driver log.
 * @param dir The LogPath directory.
 * @return Both files' contents; a missing file reads as "".
 */
inline std::string readDriverLog(const std::string &dir)
{
    return readFileIfPresent(dir + "/" + DRIVER_LOG_NAME + ".1") +
           readFileIfPresent(dir + "/" + DRIVER_LOG_NAME);
}

/**
 * @brief Checks for the connect call's return trace, written after everything
 *        that call logged.
 * @param text Driver log text.
 * @return true if SQLDriverConnect or SQLDriverConnectW logged its return.
 */
inline bool hasConnectReturn(const std::string &text)
{
    return text.find("SQLDriverConnect() return SQL_") != std::string::npos ||
           text.find("SQLDriverConnectW() return SQL_") != std::string::npos;
}

/**
 * @brief Reads the driver log once the connect call has returned. The driver logs
 *        from a background thread, so the file lags the call; waits up to 10 seconds.
 * @param dir The LogPath directory.
 * @return The log text, complete or as far as it got.
 */
inline std::string readDriverLogWhenComplete(const std::string &dir)
{
    const time_t deadline = time(NULL) + 10;
    std::string text = readDriverLog(dir);
    while (!hasConnectReturn(text) && time(NULL) < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        text = readDriverLog(dir);
    }
    return text;
}

/**
 * @brief Deletes the driver log, the libpq trace and their directory.
 * @param dir The LogPath directory.
 */
inline void removeDriverLog(const std::string &dir)
{
    RS_UNLINK((dir + "/" + DRIVER_LOG_NAME).c_str());
    RS_UNLINK((dir + "/" + DRIVER_LOG_NAME + ".1").c_str());
    RS_RMDIR(dir.c_str());
}

/**
 * @brief Checks whether SQLDriverConnect is a macro for SQLDriverConnectW, as
 *        iODBC's sqlucode.h makes it; unixODBC doesn't.
 * @return true if the ANSI call goes to the wide entry point.
 */
inline bool ansiEntryPointIsRemapped()
{
#ifdef SQLDriverConnect
    return true;
#else
    return false;
#endif
}


// PostgreSQL v3 protocol constants.
const int SSL_REQUEST_LENGTH = 8;
const int SSL_REQUEST_CODE = 80877103;
const int AUTH_OK = 0;
const int AUTH_CLEARTEXT_PASSWORD = 3;

/**
 * @brief Appends a 32-bit integer in network byte order.
 * @param out The message being built.
 * @param value The integer to append.
 */
inline void putInt32(std::string *out, int value)
{
    out->push_back((char)((value >> 24) & 0xff));
    out->push_back((char)((value >> 16) & 0xff));
    out->push_back((char)((value >> 8) & 0xff));
    out->push_back((char)(value & 0xff));
}

/**
 * @brief Reads a 32-bit integer in network byte order.
 * @param in At least 4 bytes.
 * @return The integer.
 */
inline int getInt32(const unsigned char *in)
{
    return (int)(((unsigned)in[0] << 24) | ((unsigned)in[1] << 16) |
                 ((unsigned)in[2] << 8) | (unsigned)in[3]);
}

/**
 * @brief Builds an Authentication message.
 * @param code AUTH_OK or AUTH_CLEARTEXT_PASSWORD.
 * @return The message bytes.
 */
inline std::string authRequest(int code)
{
    std::string message("R");
    putInt32(&message, 8);
    putInt32(&message, code);
    return message;
}

/**
 * @brief Builds ParameterStatus, BackendKeyData and ReadyForQuery: the minimum to
 *        pass as a server after authentication.
 * @return The message bytes.
 */
inline std::string readyForQuery()
{
    static const char param[] = "server_version\0" "99";
    std::string message("S");
    putInt32(&message, 4 + (int)sizeof(param));
    message.append(param, sizeof(param));
    message.push_back('K');
    putInt32(&message, 12);
    putInt32(&message, 1);
    putInt32(&message, 2);
    message.push_back('Z');
    putInt32(&message, 5);
    message.push_back('I');
    return message;
}

/**
 * @brief Describes the latest OpenSSL error.
 * @param what What was being attempted.
 * @return what, followed by OpenSSL's error text.
 */
inline std::string opensslError(const char *what)
{
    char buffer[256] = {0};
    ERR_error_string_n(ERR_get_error(), buffer, sizeof(buffer));
    return std::string(what) + ": " + buffer;
}

/**
 * @brief Generates a 2048-bit RSA key.
 * @return The key, which the caller frees, or NULL on failure.
 */
inline EVP_PKEY *newKey()
{
    EVP_PKEY *key = EVP_PKEY_new();
    RSA *rsa = RSA_new();
    BIGNUM *exponent = BN_new();
    bool ok = key != NULL && rsa != NULL && exponent != NULL &&
              BN_set_word(exponent, RSA_F4) == 1 &&
              RSA_generate_key_ex(rsa, 2048, exponent, NULL) == 1 &&
              EVP_PKEY_assign_RSA(key, rsa) == 1;
    BN_free(exponent);
    if (!ok)
    {
        RSA_free(rsa);
        EVP_PKEY_free(key);
        return NULL;
    }
    return key;
}

/**
 * @brief Adds an X.509v3 extension to a certificate.
 * @param cert The certificate.
 * @param ctx Issuer and subject context for the extension.
 * @param nid Which extension.
 * @param value The extension value, in OpenSSL config syntax.
 * @return true if the extension was added.
 */
inline bool addExtension(X509 *cert, X509V3_CTX *ctx, int nid, const char *value)
{
    X509_EXTENSION *ext = X509V3_EXT_conf_nid(NULL, ctx, nid, value);
    if (ext == NULL)
    {
        return false;
    }
    bool ok = X509_add_ext(cert, ext, -1) == 1;
    X509_EXTENSION_free(ext);
    return ok;
}

/**
 * @brief Mints a certificate valid for two days.
 * @param key The certificate's key.
 * @param commonName The subject CN.
 * @param issuer The signing CA, or NULL for a self-signed certificate.
 * @param issuerKey The issuer's key, or NULL to sign with key.
 * @param subjectAltName NULL makes a CA; otherwise the leaf's subjectAltName.
 * @return The certificate, which the caller frees, or NULL on failure.
 */
inline X509 *newCert(EVP_PKEY *key, const std::string &commonName, X509 *issuer,
                     EVP_PKEY *issuerKey, const char *subjectAltName)
{
    static long serial = 1;
    X509 *cert = X509_new();
    if (cert == NULL)
    {
        return NULL;
    }
    X509_NAME *subject = X509_get_subject_name(cert);
    bool ok = X509_set_version(cert, 2) == 1 &&
              ASN1_INTEGER_set(X509_get_serialNumber(cert), serial++) == 1 &&
              X509_gmtime_adj(X509_getm_notBefore(cert), -3600) != NULL &&
              X509_gmtime_adj(X509_getm_notAfter(cert), 2 * 24 * 3600) != NULL &&
              X509_set_pubkey(cert, key) == 1 &&
              X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC,
                                         (const unsigned char *)commonName.c_str(),
                                         -1, -1, 0) == 1 &&
              X509_set_issuer_name(cert, issuer != NULL
                                             ? X509_get_subject_name(issuer)
                                             : subject) == 1;
    if (ok)
    {
        X509V3_CTX ctx;
        X509V3_set_ctx(&ctx, issuer != NULL ? issuer : cert, cert, NULL, NULL, 0);
        if (subjectAltName == NULL)
        {
            ok = addExtension(cert, &ctx, NID_basic_constraints, "critical,CA:TRUE") &&
                 addExtension(cert, &ctx, NID_key_usage, "critical,keyCertSign,cRLSign");
        }
        else
        {
            ok = addExtension(cert, &ctx, NID_subject_alt_name, subjectAltName);
        }
    }
    if (!ok || X509_sign(cert, issuerKey != NULL ? issuerKey : key, EVP_sha256()) <= 0)
    {
        X509_free(cert);
        return NULL;
    }
    return cert;
}

/**
 * @brief Reads count bytes over TLS.
 * @param ssl The TLS connection.
 * @param count How many bytes to read.
 * @return The bytes read; short if the client stops or the read times out.
 */
inline std::string sslRead(SSL *ssl, int count)
{
    std::string data;
    while ((int)data.size() < count)
    {
        char buffer[512];
        int want = count - (int)data.size();
        int got = SSL_read(ssl, buffer, want < (int)sizeof(buffer) ? want : (int)sizeof(buffer));
        if (got <= 0)
        {
            break;
        }
        data.append(buffer, (size_t)got);
    }
    return data;
}

/**
 * @brief Writes data over TLS.
 * @param ssl The TLS connection.
 * @param data The bytes to write.
 * @return true if all of data was written.
 */
inline bool sslWrite(SSL *ssl, const std::string &data)
{
    return SSL_write(ssl, data.data(), (int)data.size()) == (int)data.size();
}

/**
 * @brief Sets how long a receive on sock may block.
 * @param sock The socket.
 * @param seconds The timeout.
 */
inline void setReceiveTimeout(RS_SOCKET sock, int seconds)
{
#ifdef WIN32
    DWORD timeout = (DWORD)seconds * 1000;
#else
    struct timeval timeout;
    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;
#endif
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
}

// Serves one connection on a background thread. certMode is "matching", "mismatched"
// (valid chain, wrong name) or "untrusted" (signed by a CA the client never gets).
class FakeEndpoint
{
public:
    /**
     * @brief Mints the certificates, listens on loopback and starts serving.
     * @param certMode "matching", "mismatched" or "untrusted".
     */
    explicit FakeEndpoint(const char *certMode)
    {
#ifdef WIN32
        static const bool winsockReady = []() {
            WSADATA data;
            return WSAStartup(MAKEWORD(2, 2), &data) == 0;
        }();
        if (!winsockReady)
        {
            m_startupOutput = "WSAStartup failed";
            return;
        }
#endif
        if (!makeCertificates(certMode) || !listenOnLoopback())
        {
            return;
        }
        m_thread = std::thread([this]() { serveOnce(); });
    }

    /**
     * @brief Stops the server thread and removes the CA file.
     */
    ~FakeEndpoint()
    {
        m_stop = true;
        if (m_thread.joinable())
        {
            m_thread.join();
        }
        if (m_listener != RS_NO_SOCKET)
        {
            RS_CLOSE_SOCKET(m_listener);
        }
        SSL_CTX_free(m_ctx);
        if (!m_caFile.empty())
        {
            RS_UNLINK(m_caFile.c_str());
        }
    }

    FakeEndpoint(const FakeEndpoint &) = delete;
    FakeEndpoint &operator=(const FakeEndpoint &) = delete;

    /**
     * @brief Checks whether the endpoint is serving.
     * @return true if the server thread started.
     */
    bool started() const { return m_thread.joinable(); }

    /**
     * @brief Gets the loopback port the endpoint listens on.
     * @return The port, or 0 if listening failed.
     */
    int port() const { return m_port; }

    /**
     * @brief Gets the CA file a client must trust for the endpoint's chain.
     * @return The PEM file path, or "" if minting failed.
     */
    std::string caFile() const { return m_caFile; }

    /**
     * @brief Explains why the endpoint didn't start.
     * @return The reason, or "" if it started.
     */
    const std::string &startupOutput() const { return m_startupOutput; }

    /**
     * @brief Waits for the server thread to finish its one client, then reports
     *        whether that client sent the password.
     * @return true if a PasswordMessage carrying PROBE_PASSWORD arrived.
     */
    bool passwordReceived()
    {
        if (m_thread.joinable())
        {
            m_thread.join();
        }
        return m_passwordReceived;
    }

private:
    /**
     * @brief Mints the CA, writes it to m_caFile and sets up the TLS context. libpq
     *        keeps every CA in one process-global store, where two CAs with the same
     *        subject collide, so each subject gets a unique tag.
     * @param certMode "matching", "mismatched" or "untrusted".
     * @return true on success; otherwise m_startupOutput says why.
     */
    bool makeCertificates(const char *certMode)
    {
        static int next = 0;
        const std::string tag = std::to_string((long)RS_GETPID()) + "_" + std::to_string(next++);
        const bool untrusted = strcmp(certMode, "untrusted") == 0;
        const bool mismatched = strcmp(certMode, "mismatched") == 0;

        EVP_PKEY *caKey = newKey();
        EVP_PKEY *signerKey = untrusted ? newKey() : NULL;
        EVP_PKEY *leafKey = newKey();
        X509 *ca = caKey ? newCert(caKey, "Fake Redshift Test CA " + tag, NULL, NULL, NULL) : NULL;
        X509 *signer = ca;
        if (untrusted)
        {
            signer = signerKey ? newCert(signerKey, "Rogue CA " + tag, NULL, NULL, NULL) : NULL;
        }
        const std::string san = mismatched ? std::string("DNS:") + WRONG_NAME
                                           : std::string("DNS:") + GOOD_NAME + ",IP:127.0.0.1";
        X509 *leaf = (leafKey && signer)
                         ? newCert(leafKey, mismatched ? WRONG_NAME : GOOD_NAME, signer,
                                   untrusted ? signerKey : caKey, san.c_str())
                         : NULL;

        bool ok = ca != NULL && leaf != NULL;
        if (!ok)
        {
            m_startupOutput = opensslError("could not mint the certificates");
        }
        if (ok)
        {
            // BIO, not FILE *: on Windows a FILE * from another C runtime crashes OpenSSL.
            m_caFile = uniquePath("rsodbc_fake_ca_") + ".crt";
            BIO *out = BIO_new_file(m_caFile.c_str(), "w");
            ok = out != NULL && PEM_write_bio_X509(out, ca) == 1;
            BIO_free(out);
            if (!ok)
            {
                m_startupOutput = "could not write " + m_caFile;
            }
        }
        if (ok)
        {
            m_ctx = SSL_CTX_new(TLS_server_method());
            ok = m_ctx != NULL && SSL_CTX_use_certificate(m_ctx, leaf) == 1 &&
                 SSL_CTX_use_PrivateKey(m_ctx, leafKey) == 1;
            if (!ok)
            {
                m_startupOutput = opensslError("could not set up the TLS context");
            }
        }

        X509_free(leaf);
        if (signer != ca)
        {
            X509_free(signer);
        }
        X509_free(ca);
        EVP_PKEY_free(leafKey);
        EVP_PKEY_free(signerKey);
        EVP_PKEY_free(caKey);
        return ok;
    }

    /**
     * @brief Listens on an ephemeral 127.0.0.1 port. Loopback only, since this
     *        endpoint collects a cleartext password.
     * @return true on success; otherwise m_startupOutput says why.
     */
    bool listenOnLoopback()
    {
        m_listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_listener == RS_NO_SOCKET)
        {
            m_startupOutput = "could not create a socket";
            return false;
        }
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        socklen_t length = sizeof(addr);
        if (bind(m_listener, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
            listen(m_listener, 1) != 0 ||
            getsockname(m_listener, (struct sockaddr *)&addr, &length) != 0)
        {
            m_startupOutput = "could not listen on 127.0.0.1: errno " + std::to_string(errno);
            return false;
        }
        m_port = ntohs(addr.sin_port);
        return true;
    }

    /**
     * @brief Accepts and serves one client. Polls the accept so the destructor can
     *        stop a wait the test abandoned.
     */
    void serveOnce()
    {
        const time_t deadline = time(NULL) + ENDPOINT_TIMEOUT_SECONDS;
        RS_SOCKET client = RS_NO_SOCKET;
        while (!m_stop && time(NULL) < deadline)
        {
            fd_set readable;
            FD_ZERO(&readable);
            FD_SET(m_listener, &readable);
            struct timeval tick;
            tick.tv_sec = 0;
            tick.tv_usec = 100 * 1000;
            if (select((int)m_listener + 1, &readable, NULL, NULL, &tick) > 0)
            {
                client = accept(m_listener, NULL, NULL);
                break;
            }
        }
        if (client == RS_NO_SOCKET)
        {
            return;
        }
        setReceiveTimeout(client, ENDPOINT_TIMEOUT_SECONDS);
        serveClient(client);
        RS_CLOSE_SOCKET(client);
    }

    /**
     * @brief Answers SSLRequest, completes the TLS handshake, asks for a cleartext
     *        password and lets the client in.
     * @param client The accepted socket.
     */
    void serveClient(RS_SOCKET client)
    {
        unsigned char opening[SSL_REQUEST_LENGTH];
        int have = 0;
        while (have < SSL_REQUEST_LENGTH)
        {
            int got = recv(client, (char *)opening + have, SSL_REQUEST_LENGTH - have, 0);
            if (got <= 0)
            {
                return;
            }
            have += got;
        }
        if (getInt32(opening) != SSL_REQUEST_LENGTH ||
            getInt32(opening + 4) != SSL_REQUEST_CODE ||
            send(client, "S", 1, 0) != 1)
        {
            return;
        }

        SSL *ssl = SSL_new(m_ctx);
        if (ssl == NULL)
        {
            return;
        }
        SSL_set_fd(ssl, (int)client);
        // A client that rejects the certificate ends the exchange here.
        if (SSL_accept(ssl) == 1)
        {
            std::string length = sslRead(ssl, 4);
            if (length.size() == 4)
            {
                sslRead(ssl, getInt32((const unsigned char *)length.data()) - 4);
                // Ask for a cleartext password, as a rogue server would, then let the client in.
                if (sslWrite(ssl, authRequest(AUTH_CLEARTEXT_PASSWORD)) &&
                    sslRead(ssl, 1) == "p")
                {
                    std::string size = sslRead(ssl, 4);
                    if (size.size() == 4)
                    {
                        std::string password =
                            sslRead(ssl, getInt32((const unsigned char *)size.data()) - 4);
                        m_passwordReceived = password.find(PROBE_PASSWORD) != std::string::npos;
                        sslWrite(ssl, authRequest(AUTH_OK) + readyForQuery());
                    }
                }
            }
            SSL_shutdown(ssl);
        }
        SSL_free(ssl);
    }

    std::thread m_thread;
    std::atomic<bool> m_stop{false};
    RS_SOCKET m_listener = RS_NO_SOCKET;
    SSL_CTX *m_ctx = NULL;
    std::string m_caFile;
    std::string m_startupOutput;
    int m_port = 0;
    // Written by the server thread; read only after it is joined.
    bool m_passwordReceived = false;
};

// Redirects stderr to a temp file while in scope; the tests check the warning stays off it.
class CapturedStderr
{
public:
    /**
     * @brief Starts sending stderr to a temp file.
     */
    CapturedStderr()
    {
        m_path = uniquePath("rsodbc_stderr_");
        m_file = fopen(m_path.c_str(), "w+");
        if (m_file == NULL)
        {
            m_error = "could not open " + m_path;
            return;
        }
        m_saved = RS_DUP(RS_FILENO(stderr));
        if (m_saved < 0)
        {
            m_error = "could not duplicate stderr";
            return;
        }
        fflush(stderr);
        if (RS_DUP2(RS_FILENO(m_file), RS_FILENO(stderr)) < 0)
        {
            m_error = "could not redirect stderr";
        }
    }

    /**
     * @brief Restores stderr and deletes the temp file.
     */
    ~CapturedStderr()
    {
        fflush(stderr);
        if (m_saved >= 0)
        {
            RS_DUP2(m_saved, RS_FILENO(stderr));
            RS_CLOSE(m_saved);
        }
        if (m_file != NULL)
        {
            fclose(m_file);
            RS_UNLINK(m_path.c_str());
        }
    }

    /**
     * @brief Explains why the redirect failed, in which case an empty text() proves nothing.
     * @return The reason, or "" if stderr is redirected.
     */
    const std::string &error() const { return m_error; }

    /**
     * @brief Reads what has been written to stderr so far.
     * @return The captured text.
     */
    std::string text() const
    {
        fflush(stderr);
        return readFileIfPresent(m_path);
    }

private:
    std::string m_path;
    std::string m_error;
    FILE *m_file = NULL;
    int m_saved = -1;
};

// The ANSI and wide entry points are separate implementations, so tests can pick one.
enum EntryPoint
{
    ENTRY_POINT_NATIVE, // wide under iODBC, ANSI otherwise
    ENTRY_POINT_ANSI,
    ENTRY_POINT_WIDE
};

struct Attempt
{
    SQLRETURN rc = SQL_ERROR;
    std::string diagnostics;
    std::string stderrText;
    std::string captureError; // set if the stderr redirect failed
    std::string logText;
    // Callers may reconnect from this, so it must carry the SSL mode to stay as strict.
    std::string outConnStr;
    // Only queried when the connect succeeded.
    SQLINTEGER connectionDead = SQL_CD_TRUE;
};

/**
 * @brief Connects once to the endpoint with TRACE logging, capturing stderr, the
 *        diagnostics and the driver log.
 * @param driverClause The Driver= part of the connection string.
 * @param host The host name to dial.
 * @param endpoint The endpoint to connect to.
 * @param extra More attributes, each starting with ';'.
 * @param entryPoint Which entry point to call; by default the one a driver manager would.
 * @return What the attempt returned and logged.
 */
inline Attempt connectTo(const std::string &driverClause, const std::string &host,
                         const FakeEndpoint &endpoint, const std::string &extra,
                         EntryPoint entryPoint = ENTRY_POINT_NATIVE)
{
    Attempt attempt;

    SQLHENV env = SQL_NULL_HENV;
    SQLHDBC dbc = SQL_NULL_HDBC;
    if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env) != SQL_SUCCESS)
    {
        return attempt;
    }
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    if (SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc) != SQL_SUCCESS)
    {
        SQLFreeHandle(SQL_HANDLE_ENV, env);
        return attempt;
    }

    // A tolerated mismatch is reported only in the driver log. 6 is TRACE, the most verbose.
    std::string logDir = uniquePath("rsodbc_log_");
    if (RS_MKDIR(logDir.c_str()) != 0)
    {
        logDir.clear();
    }

    std::string connStr = driverClause + "Server=" + host +
                          ";Port=" + std::to_string(endpoint.port()) +
                          ";Database=dev;UID=victim;PWD=" + PROBE_PASSWORD +
                          ";CaFile=" + connStrValue(endpoint.caFile()) + extra;
    if (!logDir.empty())
    {
        connStr += ";LogLevel=6;LogPath=" + connStrValue(logDir);
    }

    SQLCHAR outConnStr[1024] = {0};
    SQLSMALLINT outLen = 0;
    {
        CapturedStderr captured;
        // Under iODBC the ANSI call reaches the wide driver with a byte count and overruns.
        const bool goWide = (entryPoint == ENTRY_POINT_WIDE) ||
                            (entryPoint == ENTRY_POINT_NATIVE &&
                             ansiEntryPointIsRemapped());
        if (goWide)
        {
            std::vector<SQLWCHAR> wide(connStr.size() + 1, 0);
            for (size_t i = 0; i < connStr.size(); i++)
            {
                wide[i] = (SQLWCHAR)(unsigned char)connStr[i];
            }
            std::vector<SQLWCHAR> wideOut(1024, 0);
            attempt.rc = SQLDriverConnectW(dbc, NULL, wide.data(), SQL_NTS,
                                           wideOut.data(),
                                           (SQLSMALLINT)wideOut.size(), &outLen,
                                           SQL_DRIVER_NOPROMPT);
            for (SQLSMALLINT i = 0; i < outLen && i < (SQLSMALLINT)sizeof(outConnStr) - 1; i++)
            {
                outConnStr[i] = (SQLCHAR)wideOut[i];
            }
        }
        else
        {
            attempt.rc = SQLDriverConnect(dbc, NULL, (SQLCHAR *)connStr.c_str(),
                                          SQL_NTS, outConnStr, sizeof(outConnStr),
                                          &outLen, SQL_DRIVER_NOPROMPT);
        }
        attempt.stderrText = captured.text();
        attempt.captureError = captured.error();
    }
    attempt.outConnStr = std::string((const char *)outConnStr);

    SQLCHAR state[6];
    SQLCHAR message[SQL_MAX_MESSAGE_LENGTH];
    SQLINTEGER native = 0;
    SQLSMALLINT length = 0;
    for (SQLSMALLINT rec = 1;
         SQLGetDiagRec(SQL_HANDLE_DBC, dbc, rec, state, &native, message,
                       sizeof(message), &length) == SQL_SUCCESS;
         rec++)
    {
        attempt.diagnostics += std::string((char *)state) + ": " +
                               std::string((char *)message) + "\n";
    }

    if (!logDir.empty())
    {
        attempt.logText = readDriverLogWhenComplete(logDir);
        removeDriverLog(logDir);
    }

    if (SQL_SUCCEEDED(attempt.rc))
    {
        SQLGetConnectAttr(dbc, SQL_ATTR_CONNECTION_DEAD, &attempt.connectionDead,
                          0, NULL);
    }

    SQLDisconnect(dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
    return attempt;
}


} // namespace rsfake

#endif // __FAKE_REDSHIFT_ENDPOINT_H__
