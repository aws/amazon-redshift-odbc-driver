/*-------------------------------------------------------------------------
*
* Copyright(c) 2026, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
*-------------------------------------------------------------------------
*/

// In-process fake Redshift endpoint for TLS and logging regression tests.
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

#include <rslog.h>

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

// Defined in rsutil.c: the loaded driver's directory (this binary's own here).
// Declared here, not via rsutil.h, to keep this header cheap; C linkage like rsutil.h.
extern "C" char *getDriverPath();

namespace rsfake
{

// Logged when a mismatch is tolerated, and the error when verify-full was asked for.
const char *const TOLERATED_WARNING = "does not name host";
const char *const REJECTED_ERROR = "does not match host name";
const char *const CHAIN_REJECTED_ERROR = "CERTIFICATE_VERIFY_FAILED";

// Part of the driver's error when a named CA file cannot be read.
const char *const CA_FILE_UNREADABLE_ERROR = "could not be opened";

// Part of the driver's error for a CA file path that is not a file (a directory opens on glibc).
const char *const CA_FILE_NOT_A_FILE_ERROR = "is not a regular file";

// Part of the driver's error for a CA file with no readable certificate.
const char *const CA_FILE_NOT_LOADABLE_ERROR = "holds no certificate";

// Part of the driver's error when an identity-provider request's path was too long to keep.
const char *const IAM_CA_FILE_DROPPED_ERROR = "identity provider request reads";

// Distinctive so a leak of it is easy to spot.
const char *const PROBE_PASSWORD = "SECRET-MARKER-DO-NOT-LEAK";

// Caps each wait on the driver, so a test that fails before connecting can't hang the suite.
const int ENDPOINT_TIMEOUT_SECONDS = 30;

// Bounds the wait for the TLS retry after a refused plaintext startup.
const int RETRY_WAIT_SECONDS = 5;

// Upper bound on a plaintext StartupMessage drained before it is refused.
const int MAX_STARTUP_LENGTH = 10000;

// The tests link the driver directly, so the Driver keyword only has to be present.
const char *const DIRECT_DRIVER_CLAUSE = "Driver=Amazon Redshift ODBC Driver;";

const char *const GOOD_NAME = "localhost";
const char *const WRONG_NAME = "attacker.example";

// Fixed name, so the log can be read without listing the directory.
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
 * @brief Reads the driver log.
 * @param dir The LogPath directory.
 * @return The file contents, or an empty string when the file is absent.
 */
inline std::string readDriverLog(const std::string &dir)
{
    return readFileIfPresent(dir + "/" + DRIVER_LOG_NAME);
}

inline std::string secondaryDriverLogPath(const std::string &dir)
{
    return dir + "/" + DRIVER_LOG_NAME + ".1";
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
 * @brief Deletes a file when it exists.
 * @param path File path.
 * @return true when the file is absent after the call.
 */
inline bool removeFileIfPresent(const std::string &path) {
    errno = 0;
    return RS_UNLINK(path.c_str()) == 0 || errno == ENOENT;
}

/**
 * @brief Deletes the primary and legacy secondary logs, then their directory.
 * @param dir The LogPath directory.
 * @return true when the files and directory are absent after the call.
 */
inline bool removeDriverLog(const std::string &dir) {
    bool removed = removeFileIfPresent(dir + "/" + DRIVER_LOG_NAME);
    removed = removeFileIfPresent(secondaryDriverLogPath(dir)) && removed;
    errno = 0;
    if (RS_RMDIR(dir.c_str()) != 0 && errno != ENOENT) {
        removed = false;
    }
    return removed;
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
 * Builds a fatal ErrorResponse. An sslmode=allow client retries over TLS only if its
 * plaintext startup gets an error; a bare close looks like a broken server.
 */
inline std::string errorResponse(const char *sqlState, const char *text)
{
    std::string body("SFATAL");
    body.push_back('\0');
    body.push_back('C');
    body += sqlState;
    body.push_back('\0');
    body.push_back('M');
    body += text;
    body.push_back('\0');
    body.push_back('\0');  // end of the field list
    std::string message("E");
    putInt32(&message, 4 + (int)body.size());
    message += body;
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
 * Mints a self-signed CA that signs nothing and writes it to a temp file. Its subject is
 * unique, as libpq's process-global store makes equal subjects collide.
 * @return The PEM file path, or "" on failure.
 */
inline std::string writeUnrelatedCa()
{
    static int next = 0;
    const std::string name = "Unrelated Test CA " +
                             std::to_string((long)RS_GETPID()) + "_" +
                             std::to_string(next++);
    EVP_PKEY *key = newKey();
    X509 *ca = (key != NULL) ? newCert(key, name, NULL, NULL, NULL) : NULL;
    std::string path;
    if (ca != NULL) {
        path = uniquePath("rsodbc_unrelated_ca_") + ".crt";
        // BIO, not FILE *: on Windows a FILE * from another C runtime crashes OpenSSL.
        BIO *out = BIO_new_file(path.c_str(), "w");
        const bool ok = out != NULL && PEM_write_bio_X509(out, ca) == 1;
        BIO_free(out);
        if (!ok) {
            // Remove the partial file BIO_new_file created before the write failed.
            removeFileIfPresent(path);
            path.clear();
        }
    }
    X509_free(ca);
    EVP_PKEY_free(key);
    return path;
}

/** A CA that signs nothing, in a file removed when it goes out of scope. */
class UnrelatedCa
{
public:
    UnrelatedCa() : m_path(writeUnrelatedCa()) {}

    ~UnrelatedCa()
    {
        if (!m_path.empty()) {
            removeFileIfPresent(m_path);
        }
    }

    UnrelatedCa(const UnrelatedCa &) = delete;
    UnrelatedCa &operator=(const UnrelatedCa &) = delete;

    /** @return True when the certificate was minted and written. */
    bool ok() const { return !m_path.empty(); }

    /** @return The PEM file path, or "" when minting or writing failed. */
    const std::string &path() const { return m_path; }

private:
    std::string m_path;
};

// What libpqConnect appends to the driver directory for the built-in root.crt path.
const char *const BUILT_IN_ROOT_NAME = "root.crt";

// The separator libpqConnect joins them with; a diagnostic naming the built-in root.crt
// carries it.
#ifdef WIN32
const char BUILT_IN_ROOT_SEPARATOR = '\\';
#else
const char BUILT_IN_ROOT_SEPARATOR = '/';
#endif

/** The cause of the last failed socket call: WSAGetLastError() on Windows, errno elsewhere. */
inline int lastSocketError()
{
#ifdef WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

/** The cause of the last failed file replace: GetLastError() on Windows, errno elsewhere. */
inline std::string lastFileReplaceError()
{
#ifdef WIN32
    return "Windows error " + std::to_string((unsigned long)GetLastError());
#else
    return strerror(errno);
#endif
}

/** Replaces one file with another in a single step; true when to now holds what from held. */
inline bool replaceFileInPlace(const std::string &from, const std::string &to)
{
#ifdef WIN32
    // rename() refuses an existing target on Windows.
    return MoveFileExA(from.c_str(), to.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(from.c_str(), to.c_str()) == 0;
#endif
}

/** The path of the built-in root.crt beside this binary, or "" if the driver directory is unknown. */
inline std::string builtInRootPath()
{
    std::string path;
    char *driverPath = ::getDriverPath();
    if (driverPath != NULL && driverPath[0] != '\0') {
        path = std::string(driverPath) + BUILT_IN_ROOT_SEPARATOR + BUILT_IN_ROOT_NAME;
    }
    free(driverPath);
    return path;
}

/**
 * Appends a CA to the driver directory's root.crt while in scope, restoring the original
 * bytes on destruction. libpqConnect passes that file as ssldefaultrootcert, so this sets
 * what the built-in root.crt trusts. libpq's store is process-global, so the CA stays
 * trusted, but its unique subject and key mean it never signs another endpoint's leaf.
 */
class BuiltInRootExtension
{
public:
    /** Appends the certificate in a PEM file to the built-in root.crt. */
    explicit BuiltInRootExtension(const std::string &pemFile)
    {
        const std::string pem = readFileIfPresent(pemFile);
        if (pem.empty()) {
            m_why = "could not read " + pemFile;
            return;
        }
        m_path = builtInRootPath();
        if (m_path.empty()) {
            m_why = "the driver directory could not be determined";
            return;
        }
        m_original = readFileIfPresent(m_path);
        if (m_original.empty()) {
            m_why = m_path + " is missing or empty";
            return;
        }
        FILE *out = fopen(m_path.c_str(), "ab");
        if (out == NULL) {
            m_why = m_path + " could not be opened for writing";
            return;
        }
        const bool wrote =
            fwrite(pem.data(), 1, pem.size(), out) == pem.size() && fflush(out) == 0;
        const bool closed = fclose(out) == 0;
        if (!wrote || !closed) {
            m_why = "could not append to " + m_path;
            if (!restore()) {
                m_why += ", and the original bytes could not be put back";
            }
            return;
        }
        m_extended = true;
    }

    /**
     * Puts the built-in root.crt back. A destructor cannot fail the test, so a failure is
     * printed to stderr rather than left as silent residue.
     */
    ~BuiltInRootExtension()
    {
        if (m_extended && !restore()) {
            fprintf(stderr,
                    "ERROR: %s still carries the CA this test appended; restore it "
                    "before trusting another run of this suite\n",
                    m_path.c_str());
            fflush(stderr);
        }
    }

    BuiltInRootExtension(const BuiltInRootExtension &) = delete;
    BuiltInRootExtension &operator=(const BuiltInRootExtension &) = delete;

    /** True when the built-in root.crt now carries the extra CA. */
    bool ok() const { return m_extended; }

    /** Puts the built-in root.crt back now so a case can assert it is whole. */
    bool restoreNow()
    {
        if (!m_extended) {
            return true;
        }
        if (restore()) {
            return true;
        }
        m_why = "the original bytes of " + m_path + " could not be put back";
        return false;
    }

    /** Why the built-in root.crt could not be extended or restored, "" when both worked. */
    const std::string &why() const { return m_why; }

private:
    /**
     * Restores the original bytes atomically: through a temp file (named with the process id)
     * moved over root.crt, so a failed write leaves it whole. m_extended clears only on success.
     * @return true when the original bytes are back.
     */
    bool restore()
    {
        const std::string temp =
            m_path + ".restore." + std::to_string((long)RS_GETPID());
        FILE *out = fopen(temp.c_str(), "wb");
        if (out == NULL) {
            return false;
        }
        const bool wrote =
            fwrite(m_original.data(), 1, m_original.size(), out) == m_original.size() &&
            fflush(out) == 0;
        const bool closed = fclose(out) == 0;
        if (!wrote || !closed || !replaceFileInPlace(temp, m_path)) {
            RS_UNLINK(temp.c_str());
            return false;
        }
        m_extended = false;
        return true;
    }

    std::string m_path;
    std::string m_original;
    std::string m_why;
    bool m_extended = false;
};

/**
 * Replaces the built-in root.crt with test content, or removes it, and restores the
 * original bytes on destruction like BuiltInRootExtension. The cases that drive the
 * driver's check of that file need it unusable.
 */
class BuiltInRootReplacement
{
public:
    /** Takes the built-in root.crt over; content "" removes the file. */
    explicit BuiltInRootReplacement(const std::string &content)
    {
        m_path = builtInRootPath();
        if (m_path.empty()) {
            m_why = "the driver directory could not be determined";
            return;
        }
        m_original = readFileIfPresent(m_path);
        if (m_original.empty()) {
            m_why = m_path + " is missing or empty";
            return;
        }
        if (content.empty()) {
            if (RS_UNLINK(m_path.c_str()) != 0) {
                m_why = m_path + " could not be removed";
                return;
            }
            m_replaced = true;
            return;
        }
        const std::string why = writeOver(content);
        if (!why.empty()) {
            m_why = m_path + ": " + why;
            return;
        }
        m_replaced = true;
    }

    ~BuiltInRootReplacement()
    {
        if (m_replaced && !restore()) {
            fprintf(stderr,
                    "ERROR: %s no longer holds the built-in root.crt; restore it "
                    "before trusting another run of this suite\n",
                    m_path.c_str());
            fflush(stderr);
        }
    }

    BuiltInRootReplacement(const BuiltInRootReplacement &) = delete;
    BuiltInRootReplacement &operator=(const BuiltInRootReplacement &) = delete;

    /** True when the built-in root.crt now holds what this asked for. */
    bool ok() const { return m_replaced; }

    /** @return The path of the built-in root.crt, which a diagnostic must name. */
    const std::string &path() const { return m_path; }

    /** Puts the installed file back now so a case can assert it is whole. */
    bool restoreNow()
    {
        // restore() sets m_why with the step that failed.
        return !m_replaced || restore();
    }

    /** Why the built-in root.crt could not be replaced or restored, "" when both worked. */
    const std::string &why() const { return m_why; }

private:
    /**
     * @brief Writes content over the built-in root.crt in one step.
     * @return "" on success, otherwise the step that failed and its error.
     */
    std::string writeOver(const std::string &content)
    {
        const std::string temp =
            m_path + ".replace." + std::to_string((long)RS_GETPID());
        FILE *out = fopen(temp.c_str(), "wb");
        if (out == NULL) {
            return "the temporary file could not be created: " +
                   std::string(strerror(errno));
        }
        const bool wrote =
            fwrite(content.data(), 1, content.size(), out) == content.size() &&
            fflush(out) == 0;
        const int wroteErrno = errno;
        const bool closed = fclose(out) == 0;
        if (!wrote || !closed) {
            RS_UNLINK(temp.c_str());
            return "the temporary file could not be written: " +
                   std::string(strerror(wrote ? errno : wroteErrno));
        }
        if (!replaceFileInPlace(temp, m_path)) {
            // Usually something else still holds the file open.
            const std::string why = lastFileReplaceError();
            RS_UNLINK(temp.c_str());
            return "it could not be replaced: " + why;
        }
        return std::string();
    }

    /** @brief Puts the built-in root.crt back, atomically. */
    bool restore()
    {
        const std::string why = writeOver(m_original);
        if (!why.empty()) {
            m_why = "the original bytes of " + m_path + " could not be put back, " + why;
            return false;
        }
        m_replaced = false;
        return true;
    }

    std::string m_path;
    std::string m_original;
    std::string m_why;
    bool m_replaced = false;
};

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

/*
 * The message lengths this fake acts on. A length counts its own four bytes, so a payload
 * makes it at least five. Out-of-range lengths are rejected before any subtraction or read.
 */
const int MIN_MESSAGE_LENGTH = 5;
const int MAX_MESSAGE_LENGTH = 64 * 1024;

/** True when a peer-supplied message length is one this fake will read. */
inline bool messageLengthIsUsable(int length)
{
    return length >= MIN_MESSAGE_LENGTH && length <= MAX_MESSAGE_LENGTH;
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

// What one client exchange did, so the server knows whether to expect a retry.
enum ServeResult
{
    SERVE_DONE,
    SERVE_PLAINTEXT_REFUSED,  // sslmode=allow retries TLS after a plaintext refusal
    SERVE_TLS_REJECTED        // records that the client rejected the TLS certificate
};

// Serves one TLS connection on a background thread after refusing any plaintext startup.
// certMode: "matching", "mismatched" (valid chain, wrong name) or "untrusted" (unknown CA).
class FakeEndpoint
{
public:
    /**
     * @brief Mints the certificates, listens on loopback and starts serving.
     * @param certMode "matching", "mismatched" or "untrusted".
     * @param acceptPlaintextAfterTlsRejection Makes the endpoint accept a later plaintext
     *        startup. Tests use it to prove that a TLS certificate rejection does not
     *        disclose credentials in plaintext.
     * @param declineTls Respond with PostgreSQL's explicit TLS-unsupported response ('N')
     *        and accept the resulting plaintext startup.
     * @param sendInvalidTlsData Reply 'S' then send data that cannot be a TLS handshake.
     * @param rejectAuthentication Complete TLS then reject the startup before requesting a password.
     */
    explicit FakeEndpoint(const char *certMode, bool acceptPlaintextAfterTlsRejection = false,
                          bool declineTls = false, bool sendInvalidTlsData = false,
                          bool rejectAuthentication = false)
        : m_acceptPlaintextAfterTlsRejection(acceptPlaintextAfterTlsRejection),
          m_declineTls(declineTls),
          m_sendInvalidTlsData(sendInvalidTlsData),
          m_rejectAuthentication(rejectAuthentication)
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
     *        whether that client sent the password. It is read only inside the TLS
     *        session, so true also proves the connection was encrypted.
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

    /** @return True when the client completed the TLS handshake. */
    bool tlsHandshakeCompleted()
    {
        if (m_thread.joinable())
        {
            m_thread.join();
        }
        return m_tlsHandshakeCompleted;
    }

    /** @return True when a plaintext fallback sent PROBE_PASSWORD. */
    bool plaintextPasswordReceived()
    {
        if (m_thread.joinable())
        {
            m_thread.join();
        }
        return m_plaintextPasswordReceived;
    }

private:
    /**
     * @brief Mints the CA, writes it to m_caFile and sets up the TLS context.
     *        Each subject gets a unique tag so no two endpoints share a CA
     *        name.
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
            // Room for the retry of a refused plaintext client.
            listen(m_listener, 4) != 0 ||
            getsockname(m_listener, (struct sockaddr *)&addr, &length) != 0)
        {
            m_startupOutput = "could not listen on 127.0.0.1: socket error " +
                              std::to_string(lastSocketError());
            return false;
        }
        m_port = ntohs(addr.sin_port);
        return true;
    }

    /**
     * @brief Accepts and serves one TLS client. Polls the accept so the destructor can
     *        stop a wait the test abandoned. A plaintext client is refused and served
     *        again, since sslmode=allow upgrades to TLS only after that.
     */
    void serveOnce()
    {
        time_t deadline = time(NULL) + ENDPOINT_TIMEOUT_SECONDS;
        while (true) {
            RS_SOCKET client = RS_NO_SOCKET;
            while (!m_stop && time(NULL) < deadline) {
                fd_set readable;
                FD_ZERO(&readable);
                FD_SET(m_listener, &readable);
                struct timeval tick;
                tick.tv_sec = 0;
                tick.tv_usec = 100 * 1000;
                if (select((int)m_listener + 1, &readable, NULL, NULL, &tick) > 0) {
                    client = accept(m_listener, NULL, NULL);
                    break;
                }
            }
            if (client == RS_NO_SOCKET) {
                return;
            }
            setReceiveTimeout(client, ENDPOINT_TIMEOUT_SECONDS);
            const ServeResult result = serveClient(client);
            RS_CLOSE_SOCKET(client);
            if (result == SERVE_PLAINTEXT_REFUSED ||
                (result == SERVE_TLS_REJECTED && m_acceptPlaintextAfterTlsRejection) ||
                (result == SERVE_DONE && m_rejectAuthentication && m_acceptPlaintextAfterTlsRejection)) {
                // Only the libpq retry is still owed, so stop waiting for it sooner.
                deadline = time(NULL) + RETRY_WAIT_SECONDS;
                continue;
            }
            return;
        }
    }

    /** Completes an already-started plaintext connection for the prefer fallback case. */
    ServeResult servePlaintextStartup(RS_SOCKET client, int length)
    {
        if (length <= SSL_REQUEST_LENGTH || length >= MAX_STARTUP_LENGTH) {
            return SERVE_DONE;
        }
        std::string startup;
        startup.resize((size_t)(length - SSL_REQUEST_LENGTH));
        size_t drained = 0;
        while (drained < startup.size()) {
            int got = recv(client, &startup[drained], (int)(startup.size() - drained), 0);
            if (got <= 0) {
                return SERVE_DONE;
            }
            drained += (size_t)got;
        }
        const std::string request = authRequest(AUTH_CLEARTEXT_PASSWORD);
        if (send(client, request.data(), (int)request.size(), 0) <= 0) {
            return SERVE_DONE;
        }
        char kind = '\0';
        if (recv(client, &kind, 1, 0) != 1 || kind != 'p') {
            return SERVE_DONE;
        }
        unsigned char size[4];
        if (recv(client, (char *)size, sizeof(size), 0) != (int)sizeof(size)) {
            return SERVE_DONE;
        }
        const int passwordLength = getInt32(size);
        if (!messageLengthIsUsable(passwordLength)) {
            return SERVE_DONE;
        }
        std::string password;
        password.resize((size_t)(passwordLength - 4));
        if (recv(client, &password[0], (int)password.size(), 0) != (int)password.size()) {
            return SERVE_DONE;
        }
        m_plaintextPasswordReceived = password.find(PROBE_PASSWORD) != std::string::npos;
        const std::string ready = authRequest(AUTH_OK) + readyForQuery();
        send(client, ready.data(), (int)ready.size(), 0);
        return SERVE_DONE;
    }

    /**
     * @brief Answers SSLRequest, completes the TLS handshake, asks for a cleartext
     *        password and lets the client in. A plaintext startup is refused with a
     *        fatal error, which makes an sslmode=allow client retry on TLS.
     * @param client The accepted socket.
     * @return SERVE_PLAINTEXT_REFUSED for an allow retry, SERVE_TLS_REJECTED for a
     *         prefer fallback, otherwise SERVE_DONE.
     */
    ServeResult serveClient(RS_SOCKET client)
    {
        unsigned char opening[SSL_REQUEST_LENGTH];
        int have = 0;
        while (have < SSL_REQUEST_LENGTH)
        {
            int got = recv(client, (char *)opening + have, SSL_REQUEST_LENGTH - have, 0);
            if (got <= 0)
            {
                return SERVE_DONE;
            }
            have += got;
        }
        const int length = getInt32(opening);
        if (length != SSL_REQUEST_LENGTH || getInt32(opening + 4) != SSL_REQUEST_CODE) {
            // A StartupMessage, not an SSLRequest. Only the prefer-fallback endpoint accepts it.
            if (m_acceptPlaintextAfterTlsRejection) {
                return servePlaintextStartup(client, length);
            }
            if (length > SSL_REQUEST_LENGTH && length < MAX_STARTUP_LENGTH) {
                std::string rest;
                rest.resize((size_t)(length - SSL_REQUEST_LENGTH));
                size_t drained = 0;
                while (drained < rest.size()) {
                    int got = recv(client, &rest[drained], (int)(rest.size() - drained), 0);
                    if (got <= 0) {
                        break;
                    }
                    drained += (size_t)got;
                }
            }
            const std::string refusal =
                errorResponse("28000", "fake endpoint requires TLS");
            send(client, refusal.data(), (int)refusal.size(), 0);
            return SERVE_PLAINTEXT_REFUSED;
        }
        if (m_declineTls) {
            if (send(client, "N", 1, 0) != 1) {
                return SERVE_DONE;
            }
            unsigned char startup[SSL_REQUEST_LENGTH];
            if (recv(client, (char *)startup, sizeof(startup), 0) != (int)sizeof(startup)) {
                return SERVE_DONE;
            }
            return servePlaintextStartup(client, getInt32(startup));
        }
        if (send(client, "S", 1, 0) != 1) {
            return SERVE_DONE;
        }
        if (m_sendInvalidTlsData) {
#ifdef WIN32
            send(client, "not TLS", 7, 0);
#else
            send(client, "not TLS", 7, MSG_NOSIGNAL);
#endif
            return SERVE_TLS_REJECTED;
        }

        SSL *ssl = SSL_new(m_ctx);
        if (ssl == NULL)
        {
            return SERVE_DONE;
        }
        SSL_set_fd(ssl, (int)client);
        // A client that rejects the certificate ends the exchange here.
        const bool tlsAccepted = SSL_accept(ssl) == 1;
        if (tlsAccepted)
        {
            m_tlsHandshakeCompleted = true;
            std::string length = sslRead(ssl, 4);
            // Check the length before subtracting from it, so a bad one is refused, not read.
            if (length.size() == 4 &&
                messageLengthIsUsable(
                    getInt32((const unsigned char *)length.data()))) {
                sslRead(ssl, getInt32((const unsigned char *)length.data()) - 4);
                if (m_rejectAuthentication) {
                    const std::string rejection = errorResponse("28000", "fake authentication failure");
                    sslWrite(ssl, rejection);
                }
                // Ask for a cleartext password, as a rogue server would, then let the client in.
                else if (sslWrite(ssl, authRequest(AUTH_CLEARTEXT_PASSWORD)) &&
                    sslRead(ssl, 1) == "p")
                {
                    std::string size = sslRead(ssl, 4);
                    if (size.size() == 4 &&
                        messageLengthIsUsable(
                            getInt32((const unsigned char *)size.data()))) {
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
        return tlsAccepted ? SERVE_DONE : SERVE_TLS_REJECTED;
    }

    std::thread m_thread;
    std::atomic<bool> m_stop{false};
    const bool m_acceptPlaintextAfterTlsRejection = false;
    const bool m_declineTls = false;
    const bool m_sendInvalidTlsData = false;
    const bool m_rejectAuthentication = false;
    RS_SOCKET m_listener = RS_NO_SOCKET;
    SSL_CTX *m_ctx = NULL;
    std::string m_caFile;
    std::string m_startupOutput;
    int m_port = 0;
    // Written by the server thread; read only after it is joined.
    bool m_tlsHandshakeCompleted = false;
    bool m_passwordReceived = false;
    bool m_plaintextPasswordReceived = false;
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
    bool secondaryLogCreated = false;
    bool logCleanupSucceeded = true;
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
 * @param supplyCaFile When true (default) the endpoint's CA is passed as CaFile.
 * @return What the attempt returned and logged.
 */
inline Attempt connectTo(const std::string &driverClause, const std::string &host,
                         const FakeEndpoint &endpoint, const std::string &extra,
                         EntryPoint entryPoint = ENTRY_POINT_NATIVE,
                         bool supplyCaFile = true)
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
                          ";Database=dev;UID=victim;PWD=" + PROBE_PASSWORD;
    if (supplyCaFile) {
        connStr += ";CaFile=" + connStrValue(endpoint.caFile());
    }
    connStr += extra;
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
        attempt.secondaryLogCreated =
            std::ifstream(secondaryDriverLogPath(logDir)).good();
    }

    if (SQL_SUCCEEDED(attempt.rc))
    {
        SQLGetConnectAttr(dbc, SQL_ATTR_CONNECTION_DEAD, &attempt.connectionDead,
                          0, NULL);
    }

    SQLDisconnect(dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
    if (!logDir.empty())
    {
        shutdownLogging();
        attempt.logCleanupSucceeded = removeDriverLog(logDir);
    }
    return attempt;
}


} // namespace rsfake

#endif // __FAKE_REDSHIFT_ENDPOINT_H__
