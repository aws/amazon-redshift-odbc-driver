/*
 * Copyright (c) Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * tls_policy_test.cpp
 *
 * Unit tests for the TLS policy helpers in pq_tls_policy.h,
 * verification that the linked crypto library (AWS-LC) supports
 * the post-quantum key exchange groups the driver advertises, the
 * per-process count of open SSL connections, and the root certificates
 * and sockets each SSL connection owns.
 */

// Include first: it pulls in winsock2.h, which must precede windows.h on Windows.
#include "fake_redshift_endpoint.hpp"

#include "common.h"
#include "rsutil.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <arpa/inet.h>
#include <dirent.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

extern "C" {
#include "libpq-fe.h"
#include "pq_tls_policy.h"
long pqsecure_open_connection_count(void);
void pqsecure_destroy(void);
}

/*
 * pq_should_prefer_pq -- PQ advertising default + opt-out
 */

TEST(TlsPolicyPreferPQ, NullDefaultsToOn) {
    EXPECT_EQ(1, pq_should_prefer_pq(nullptr));
}

TEST(TlsPolicyPreferPQ, EmptyStringDefaultsToOn) {
    EXPECT_EQ(1, pq_should_prefer_pq(""));
}

TEST(TlsPolicyPreferPQ, ZeroDisables) {
    // Customer-facing opt-out for the middlebox-incompatibility case.
    EXPECT_EQ(0, pq_should_prefer_pq("0"));
}

TEST(TlsPolicyPreferPQ, LowercaseFalseDisables) {
    EXPECT_EQ(0, pq_should_prefer_pq("false"));
}

TEST(TlsPolicyPreferPQ, CapitalizedFalseDisables) {
    EXPECT_EQ(0, pq_should_prefer_pq("False"));
}

TEST(TlsPolicyPreferPQ, UppercaseFalseDisables) {
    EXPECT_EQ(0, pq_should_prefer_pq("FALSE"));
}

TEST(TlsPolicyPreferPQ, OneKeepsOn) {
    EXPECT_EQ(1, pq_should_prefer_pq("1"));
}

TEST(TlsPolicyPreferPQ, TrueKeepsOn) {
    EXPECT_EQ(1, pq_should_prefer_pq("true"));
    EXPECT_EQ(1, pq_should_prefer_pq("True"));
    EXPECT_EQ(1, pq_should_prefer_pq("TRUE"));
}

TEST(TlsPolicyPreferPQ, TypoIsTreatedAsOn) {
    EXPECT_EQ(1, pq_should_prefer_pq("fasle"));
    EXPECT_EQ(1, pq_should_prefer_pq("no"));
    EXPECT_EQ(1, pq_should_prefer_pq("off"));
}

/*
 * SNI policy -- enabled by default, with IP literals excluded
 */

TEST(TlsPolicySni, LeadingOneEnablesHostnameSni) {
    EXPECT_EQ(1, pq_should_send_sni("1", "relay.example.com"));
    EXPECT_EQ(1, pq_should_send_sni("10", "relay.example.com"));
}

TEST(TlsPolicySni, ValuesNotStartingWithOneDisableSni) {
    EXPECT_EQ(0, pq_should_send_sni(nullptr, "relay.example.com"));
    EXPECT_EQ(0, pq_should_send_sni("", "relay.example.com"));
    EXPECT_EQ(0, pq_should_send_sni("0", "relay.example.com"));
    EXPECT_EQ(0, pq_should_send_sni("false", "relay.example.com"));
    EXPECT_EQ(0, pq_should_send_sni("invalid", "relay.example.com"));
}

TEST(TlsPolicySni, DigitAndDotOnlyHostsDoNotUseSni) {
    EXPECT_EQ(0, pq_should_send_sni("1", "127.0.0.1"));
    EXPECT_EQ(0, pq_should_send_sni("1", "255.255.255.255"));
    EXPECT_EQ(0, pq_should_send_sni("1", "256.1.1.1"));
    EXPECT_EQ(0, pq_should_send_sni("1", "1234"));
}

TEST(TlsPolicySni, Ipv6LiteralDoesNotUseSni) {
    EXPECT_EQ(0, pq_should_send_sni("1", "2001:db8::1"));
    EXPECT_EQ(0, pq_should_send_sni("1", "::1"));
}

TEST(TlsPolicySni, BracketedIpv6DoesNotUseSni) {
    EXPECT_EQ(0, pq_should_send_sni("1", "[2001:db8::1]"));
}

TEST(TlsPolicySni, DigitLeadingHostnameUsesSni) {
    EXPECT_EQ(1, pq_should_send_sni("1", "123.relay.example.com"));
}

TEST(TlsPolicySni, MissingHostDoesNotUseSni) {
    EXPECT_EQ(0, pq_should_send_sni("1", nullptr));
    EXPECT_EQ(0, pq_should_send_sni("1", ""));
}

/*
 * pq_resolve_min_tls -- min_tls property mapping + fail-secure default
 */

TEST(TlsPolicyMinTls, NullDefaultsTo12) {
    EXPECT_EQ(RS_MIN_TLS_1_2, pq_resolve_min_tls(nullptr));
}

TEST(TlsPolicyMinTls, ValidVersionsMapDirectly) {
    EXPECT_EQ(RS_MIN_TLS_1_1, pq_resolve_min_tls("1.1"));
    EXPECT_EQ(RS_MIN_TLS_1_2, pq_resolve_min_tls("1.2"));
    EXPECT_EQ(RS_MIN_TLS_1_3, pq_resolve_min_tls("1.3"));
}

TEST(TlsPolicyMinTls, InvalidValueFailsSecureTo12) {
    // The behavior connection_string_tls_invalid_defaults_to_12 asserted:
    // any unrecognized value clamps to TLS 1.2 rather than weakening it.
    EXPECT_EQ(RS_MIN_TLS_1_2, pq_resolve_min_tls("garbage"));
    EXPECT_EQ(RS_MIN_TLS_1_2, pq_resolve_min_tls(""));
    EXPECT_EQ(RS_MIN_TLS_1_2, pq_resolve_min_tls("1.0"));
    EXPECT_EQ(RS_MIN_TLS_1_2, pq_resolve_min_tls("1"));
    EXPECT_EQ(RS_MIN_TLS_1_2, pq_resolve_min_tls("TLSv1.3"));
}

TEST(TlsPolicyMinTls, NeverReturnsBelow12ForUnknownInput) {
    // Fail-secure invariant: unknown input must never resolve below 1.2.
    const char *inputs[] = {"0.9", "2.0", "ssl3", "1.4", "abc", " 1.2"};
    for (const char *in : inputs) {
        rs_min_tls_version v = pq_resolve_min_tls(in);
        EXPECT_TRUE(v == RS_MIN_TLS_1_2 || v == RS_MIN_TLS_1_3)
            << "input \"" << in << "\" must not weaken the TLS floor below 1.2";
    }
}

/*
 * PQ_HYBRID_GROUPS_LIST invariants
 */

TEST(TlsPolicyPqGroups, ListHasPqHybridsBeforeClassical) {
    const char *list = PQ_HYBRID_GROUPS_LIST;
    const char *firstMlkem = strstr(list, "MLKEM");
    const char *firstX25519Std = strstr(list, ":X25519");  // classical X25519
    ASSERT_NE(nullptr, firstMlkem)
        << "PQ_HYBRID_GROUPS_LIST must contain at least one MLKEM hybrid: "
        << list;
    ASSERT_NE(nullptr, firstX25519Std)
        << "PQ_HYBRID_GROUPS_LIST must contain classical fallback X25519: "
        << list;
    EXPECT_LT(firstMlkem, firstX25519Std)
        << "MLKEM hybrid must appear before classical X25519: " << list;
}

TEST(TlsPolicyPqGroups, ListContainsX25519Mlkem768) {
    EXPECT_NE(nullptr, strstr(PQ_HYBRID_GROUPS_LIST, "X25519MLKEM768"))
        << "PQ_HYBRID_GROUPS_LIST must include X25519MLKEM768: "
        << PQ_HYBRID_GROUPS_LIST;
}

/*
 * Verify that the linked crypto library (AWS-LC) actually supports
 * the PQ groups we advertise. This catches regressions where the
 * crypto backend is swapped to a library without PQ support.
 */

#include <openssl/ssl.h>

TEST(TlsPolicyCrypto, SslAcceptsPqGroupsList) {
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    ASSERT_NE(nullptr, ctx);
    SSL *ssl = SSL_new(ctx);
    ASSERT_NE(nullptr, ssl);

    int rc = SSL_set1_groups_list(ssl, PQ_HYBRID_GROUPS_LIST);
    EXPECT_EQ(1, rc)
        << "SSL_set1_groups_list must accept PQ_HYBRID_GROUPS_LIST ("
        << PQ_HYBRID_GROUPS_LIST << "). "
        << "This fails if the linked crypto library does not support "
        << "post-quantum key exchange groups.";

    SSL_free(ssl);
    SSL_CTX_free(ctx);
}

TEST(TlsPolicyCrypto, SslAcceptsClassicalGroupsList) {
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    ASSERT_NE(nullptr, ctx);
    SSL *ssl = SSL_new(ctx);
    ASSERT_NE(nullptr, ssl);

    int rc = SSL_set1_groups_list(ssl, PQ_CLASSICAL_GROUPS_LIST);
    EXPECT_EQ(1, rc)
        << "SSL_set1_groups_list must accept PQ_CLASSICAL_GROUPS_LIST ("
        << PQ_CLASSICAL_GROUPS_LIST << "). "
        << "The Prefer_PQ=0 path depends on this list being valid.";

    SSL_free(ssl);
    SSL_CTX_free(ctx);
}

TEST(TlsPolicyCrypto, SslRejectsInvalidGroupName) {
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    ASSERT_NE(nullptr, ctx);
    SSL *ssl = SSL_new(ctx);
    ASSERT_NE(nullptr, ssl);

    int rc = SSL_set1_groups_list(ssl, "NotARealGroup");
    EXPECT_NE(1, rc);

    SSL_free(ssl);
    SSL_CTX_free(ctx);
}

/*====================================================================================================================================================*/

// No mode named: the mismatch is logged and the connection still goes through.
TEST(SslModeHostnameMismatch, defaulted_mode_warns_about_a_wrong_name)
{
    rsfake::FakeEndpoint endpoint("mismatched");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint, "");

    EXPECT_NE(attempt.logText.find(rsfake::TOLERATED_WARNING), std::string::npos)
        << "A defaulted mode must record the mismatch in the driver log. log was:\n"
        << attempt.logText;
    EXPECT_EQ(attempt.diagnostics.find(rsfake::REJECTED_ERROR), std::string::npos)
        << "A defaulted mode must not reject the name outright. diagnostics were:\n"
        << attempt.diagnostics;

    // The warning goes to the log only; the application's user sees nothing.
    ASSERT_TRUE(attempt.captureError.empty())
        << "could not capture stderr: " << attempt.captureError;
    EXPECT_EQ(attempt.stderrText.find(rsfake::TOLERATED_WARNING), std::string::npos)
        << "The mismatch must not reach stderr. stderr was:\n"
        << attempt.stderrText;
    EXPECT_EQ(attempt.rc, SQL_SUCCESS)
        << "A tolerated mismatch must connect plainly, with nothing for the "
        << "application to surface. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_EQ(attempt.connectionDead, SQL_CD_FALSE)
        << "The connection must still be usable";
    // Release 1 still sends the password after a tolerated mismatch, by design.
    EXPECT_TRUE(endpoint.passwordReceived())
        << "A tolerated mismatch completes the login, so the password must reach the server";
}

/*====================================================================================================================================================*/

// A caller who named the mode gets no allowance: the same certificate fails.
TEST(SslModeHostnameMismatch, explicit_verify_full_rejects_a_wrong_name)
{
    rsfake::FakeEndpoint endpoint("mismatched");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint, ";sslmode=verify-full");

    EXPECT_FALSE(endpoint.passwordReceived())
        << "The password must not be sent to a server whose certificate names another host";
    ASSERT_TRUE(attempt.captureError.empty())
        << "could not capture stderr: " << attempt.captureError;
    EXPECT_FALSE(SQL_SUCCEEDED(attempt.rc))
        << "An explicit verify-full must reject a certificate naming another host. "
        << "diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.diagnostics.find(rsfake::REJECTED_ERROR), std::string::npos)
        << "The failure must name the mismatch. diagnostics were:\n"
        << attempt.diagnostics;
    ASSERT_TRUE(rsfake::hasConnectReturn(attempt.logText))
        << "the driver log was not captured, so the warning check would prove nothing";
    EXPECT_EQ(attempt.logText.find(rsfake::TOLERATED_WARNING), std::string::npos)
        << "A caller who chose the mode must not get the transitional warning. "
        << "log was:\n"
        << attempt.logText;
}

/*====================================================================================================================================================*/

// EncryptionMethod=2 also names verify-full and must be just as strict.
TEST(SslModeHostnameMismatch, explicit_encryption_method_rejects_a_wrong_name)
{
    rsfake::FakeEndpoint endpoint("mismatched");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint, ";EncryptionMethod=2");

    EXPECT_FALSE(endpoint.passwordReceived())
        << "The password must not be sent to a server whose certificate names another host";
    EXPECT_FALSE(SQL_SUCCEEDED(attempt.rc))
        << "EncryptionMethod=2 names verify-full, so a certificate for another host "
        << "must be rejected. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.diagnostics.find(rsfake::REJECTED_ERROR), std::string::npos)
        << "The failure must name the mismatch. diagnostics were:\n"
        << attempt.diagnostics;
    ASSERT_TRUE(rsfake::hasConnectReturn(attempt.logText))
        << "the driver log was not captured, so the warning check would prove nothing";
    EXPECT_EQ(attempt.logText.find(rsfake::TOLERATED_WARNING), std::string::npos)
        << "A caller who chose the mode must not get the transitional warning. "
        << "log was:\n"
        << attempt.logText;
}

/*====================================================================================================================================================*/

// The allowance skips the name check only. No mode is named, and the leaf is
// signed by a CA the driver is never given.
TEST(SslModeHostnameMismatch, the_allowance_never_relaxes_chain_validation)
{
    rsfake::FakeEndpoint endpoint("untrusted");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint, "");

    EXPECT_FALSE(SQL_SUCCEEDED(attempt.rc))
        << "A chain that does not validate must fail even when the mode was "
        << "defaulted. diagnostics were:\n"
        << attempt.diagnostics;
    // Any failure would satisfy the check above, so require the chain's.
    EXPECT_NE(attempt.diagnostics.find(rsfake::CHAIN_REJECTED_ERROR), std::string::npos)
        << "The connection must fail on the certificate chain. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_FALSE(endpoint.passwordReceived())
        << "The password must not be sent to a server whose chain does not validate";
    EXPECT_EQ(attempt.logText.find(rsfake::TOLERATED_WARNING), std::string::npos)
        << "A chain failure is not a name mismatch and must not be recorded as a "
        << "tolerated one. log was:\n"
        << attempt.logText;
}

/*====================================================================================================================================================*/

// A caller who named verify-full gets SQL_ERROR when CaFile did not sign the
// chain.
TEST(SslModeHostnameMismatch, explicit_verify_full_rejects_an_untrusted_chain)
{
    rsfake::FakeEndpoint endpoint{"untrusted"};
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::Attempt const attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint,
        ";sslmode=verify-full");

    EXPECT_FALSE(endpoint.passwordReceived())
        << "The password must not be sent to a server whose chain does not "
        << "validate";
    EXPECT_EQ(attempt.rc, SQL_ERROR)
        << "verify-full must fail when CaFile did not sign the server's chain. "
        << "diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.diagnostics.find(rsfake::CHAIN_REJECTED_ERROR),
              std::string::npos)
        << "The connection must fail on the certificate chain. diagnostics "
        << "were:\n"
        << attempt.diagnostics;
}

/*====================================================================================================================================================*/

namespace {

/**
 * @brief Connects with sslmode=verify-full to an endpoint whose certificate names
 *        the host, and checks the completed connection string still names the mode.
 * @param entryPoint ANSI or wide SQLDriverConnect.
 * @param entryPointName Name of the entry point, for failure messages.
 */
void expectCompletedStringCarriesMode(rsfake::EntryPoint entryPoint,
                                      const char *entryPointName)
{
    rsfake::FakeEndpoint endpoint("matching");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::Attempt attempt =
        rsfake::connectTo(rsfake::DIRECT_DRIVER_CLAUSE, "localhost", endpoint,
                          ";sslmode=verify-full", entryPoint);

    ASSERT_TRUE(SQL_SUCCEEDED(attempt.rc))
        << entryPointName << ": a certificate that does name the host must be "
        << "accepted under verify-full. diagnostics were:\n"
        << attempt.diagnostics;

    // Without the mode, reconnecting from this string would quietly get the
    // allowance. The completed string echoes the caller's own spelling of the
    // keyword, so compare case insensitively.
    std::string completed = attempt.outConnStr;
    std::transform(completed.begin(), completed.end(), completed.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    EXPECT_NE(completed.find("sslmode=verify-full"), std::string::npos)
        << entryPointName << ": the completed string must carry the mode the "
        << "caller named, or reconnecting from it is weaker than the original "
        << "connection. it was:\n"
        << attempt.outConnStr;
}

}  // namespace

// ANSI and wide entry points build the output string separately, so test both.
TEST(SslModeRoundTrip, ansi_completed_string_carries_an_explicit_mode)
{
    // If the driver manager maps ANSI onto wide, the wide test covers it.
    if (rsfake::ansiEntryPointIsRemapped())
    {
        GTEST_SKIP() << "this driver manager maps SQLDriverConnect onto its wide "
                        "form, so the wide case covers this platform";
    }

    expectCompletedStringCarriesMode(rsfake::ENTRY_POINT_ANSI, "ANSI");
}

/*====================================================================================================================================================*/

TEST(SslModeRoundTrip, wide_completed_string_carries_an_explicit_mode)
{
    expectCompletedStringCarriesMode(rsfake::ENTRY_POINT_WIDE, "wide");
}

/*====================================================================================================================================================*/

// Writing the default into the string would take the allowance from callers
// who never named a mode.
TEST(SslModeRoundTrip, completed_string_does_not_pin_a_defaulted_mode)
{
    rsfake::FakeEndpoint endpoint("matching");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::Attempt attempt = rsfake::connectTo(rsfake::DIRECT_DRIVER_CLAUSE,
                                                "localhost", endpoint, "");

    ASSERT_TRUE(SQL_SUCCEEDED(attempt.rc))
        << "a defaulted mode must still connect to a matching certificate. "
        << "diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_EQ(attempt.outConnStr.find("SSLMode="), std::string::npos)
        << "a caller who named no mode must not have one written into their "
        << "completed string. it was:\n"
        << attempt.outConnStr;
}

/*====================================================================================================================================================*/

// connStrValue must survive the driver's parser: ';' stays inside, '}' is doubled.
TEST(SslModeHostnameMismatch, conn_str_value_round_trips_through_the_parser)
{
    for (const std::string value : {"C:\\Temp\\a;b", "C:\\Users\\A}B\\Temp", "/tmp/x;y}z}"})
    {
        StringMap parsed = parseConnectionString(
            "Driver=x;CaFile=" + rsfake::connStrValue(value) + ";LogLevel=6");
        EXPECT_EQ(parsed["CaFile"], value) << "value did not round-trip";
        EXPECT_EQ(parsed["LogLevel"], "6") << "the value leaked into the next attribute";
    }
}

/*
 * ssl_open_connections -- every SSL connect is counted once and undone once.
 * The libcrypto lock callbacks are removed only when the count returns to zero.
 */

namespace {

#ifdef _WIN32
using SocketHandle = SOCKET;
SocketHandle const kInvalidSocket{INVALID_SOCKET};
#else
using SocketHandle = int;
SocketHandle const kInvalidSocket{-1};
#endif

// How long a loopback client or server waits before giving up.
constexpr int kLoopbackTimeoutSeconds{5};
constexpr int kLoopbackTimeoutMs{kLoopbackTimeoutSeconds * 1000};

/**
 * @brief Closes a socket; does nothing for kInvalidSocket.
 * @param socket The socket to close.
 */
void closeSocket(SocketHandle socket) {
    if (socket == kInvalidSocket) {
        return;
    }
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

/**
 * @brief Waits for a socket to become readable.
 * @param socket The socket.
 * @param timeoutMs How long to wait.
 * @return true if it became readable in time.
 */
bool waitForRead(SocketHandle socket, int timeoutMs) {
#ifdef _WIN32
    WSAPOLLFD descriptor{socket, POLLRDNORM, 0};
    return WSAPoll(&descriptor, 1, timeoutMs) > 0;
#else
    pollfd descriptor{socket, POLLIN, 0};
    return poll(&descriptor, 1, timeoutMs) > 0;
#endif
}

/**
 * @brief Switches a socket between blocking and non-blocking mode.
 * @param socket The socket.
 * @param blocking true for blocking mode.
 */
void setBlocking(SocketHandle socket, bool blocking) {
#ifdef _WIN32
    u_long nonBlocking{blocking ? 0UL : 1UL};
    ioctlsocket(socket, FIONBIO, &nonBlocking);
#else
    int const flags{fcntl(socket, F_GETFL, 0)};
    fcntl(socket, F_SETFL, blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK));
#endif
}

/**
 * @brief Bounds every blocking read and write on a socket.
 * @param socket The socket.
 * @param seconds The limit for each call.
 */
void setIoTimeouts(SocketHandle socket, int seconds) {
#ifdef _WIN32
    DWORD const ms{static_cast<DWORD>(seconds) * 1000};
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<char const *>(&ms), sizeof(ms));
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<char const *>(&ms), sizeof(ms));
#else
    timeval const limit{seconds, 0};
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &limit, sizeof(limit));
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &limit, sizeof(limit));
#endif
}

/**
 * @brief Waits until the socket is ready the way PQconnectPoll asked.
 * @param socket The connection's socket.
 * @param status PQconnectPoll's last result: readable for
 * PGRES_POLLING_READING, writable otherwise.
 * @param timeoutMs How long to wait.
 */
void waitForPoll(SocketHandle socket, PostgresPollingStatusType status,
                 int timeoutMs) {
#ifdef _WIN32
    WSAPOLLFD descriptor{socket,
                         static_cast<SHORT>(status == PGRES_POLLING_READING
                                                ? POLLRDNORM
                                                : POLLWRNORM),
                         0};
    WSAPoll(&descriptor, 1, timeoutMs);
#else
    pollfd descriptor{
        socket,
        static_cast<short>(status == PGRES_POLLING_READING ? POLLIN : POLLOUT),
        0};
    poll(&descriptor, 1, timeoutMs);
#endif
}

/*
 * Loopback endpoint that answers each SSLRequest with 'S' and never completes
 * the handshake, so libpq counts the connection and then has to undo it.
 */
class SslRequestAcceptor {
  public:
    /**
     * @brief Opens a loopback listener on a free port; port() is 0 if that
     *        fails.
     */
    SslRequestAcceptor() {
#ifdef _WIN32
        WSADATA wsaData;
        m_wsaStarted = WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
#endif
        m_listen = socket(AF_INET, SOCK_STREAM, 0);
        if (m_listen == kInvalidSocket) {
            return;
        }
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        socklen_t len{sizeof(addr)};
        if (bind(m_listen, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) !=
                0 ||
            listen(m_listen, 16) != 0 ||
            getsockname(m_listen, reinterpret_cast<sockaddr *>(&addr), &len) !=
                0) {
            return;
        }
        m_port = ntohs(addr.sin_port);
        // Several threads may wait on this listener; non-blocking, so the
        // ones that lose the race for a connection return to their wait.
        setBlocking(m_listen, false);
    }

    /** @brief Closes the listener. */
    ~SslRequestAcceptor() {
        closeSocket(m_listen);
#ifdef _WIN32
        if (m_wsaStarted) {
            WSACleanup();
        }
#endif
    }

    /** @return The listening port, or 0 if the listener did not open. */
    int port() const { return m_port; }

    /**
     * @brief Accepts one connection, answers its SSLRequest with 'S', and
     * closes once release is ready.
     * @param release Signalled when the test is done with the connection.
     */
    void serveOne(std::shared_future<void> release) {
        SocketHandle const client{acceptSslRequest()};
        if (client != kInvalidSocket) {
            release.wait_for(std::chrono::seconds{2 * kLoopbackTimeoutSeconds});
        }
        closeSocket(client);
    }

    /**
     * @brief Accepts one connection, answers its SSLRequest with 'S', and runs
     *        the server side of the TLS handshake.
     * @param serverCtx Context holding the server's certificate and key.
     * @return true if the client completed the handshake.
     */
    bool serveHandshake(SSL_CTX *serverCtx) {
        SocketHandle const client{acceptSslRequest()};
        if (client == kInvalidSocket) {
            return false;
        }
        SSL *const ssl{SSL_new(serverCtx)};
        bool const completed{ssl != nullptr &&
                             SSL_set_fd(ssl, static_cast<int>(client)) == 1 &&
                             SSL_accept(ssl) == 1};
        if (completed) {
            SSL_shutdown(ssl);
        }
        SSL_free(ssl);
        closeSocket(client);
        return completed;
    }

  private:
    /**
     * @brief Accepts one connection and answers its SSLRequest with 'S'.
     * @return The client socket, or kInvalidSocket if no SSLRequest arrived.
     */
    SocketHandle acceptSslRequest() {
        auto const deadline{std::chrono::steady_clock::now() +
                            std::chrono::milliseconds{kLoopbackTimeoutMs}};
        SocketHandle client{kInvalidSocket};
        while (client == kInvalidSocket) {
            auto const left{std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now())};
            if (left.count() <= 0 ||
                !waitForRead(m_listen, static_cast<int>(left.count()))) {
                return kInvalidSocket;
            }
            // Fails at once if another thread took the connection.
            client = accept(m_listen, nullptr, nullptr);
        }
        // The client socket may inherit non-blocking mode (Windows); the
        // TLS server side expects blocking calls, each bounded.
        setBlocking(client, true);
        setIoTimeouts(client, kLoopbackTimeoutSeconds);
        char request[8];
        size_t got{0};
        while (got < sizeof(request) &&
               waitForRead(client, kLoopbackTimeoutMs)) {
            int const n = recv(client, request + got,
                               static_cast<int>(sizeof(request) - got), 0);
            if (n <= 0) {
                break;
            }
            got += static_cast<size_t>(n);
        }
        if (got != sizeof(request)) {
            closeSocket(client);
            return kInvalidSocket;
        }
        send(client, "S", 1, 0);
        return client;
    }

    SocketHandle m_listen{kInvalidSocket};
    int m_port{0};
#ifdef _WIN32
    bool m_wsaStarted{false};
#endif
};

/**
 * @brief Builds a libpq connection string for the loopback acceptor.
 * @param port The acceptor's port.
 * @return A conninfo string with sslmode=require.
 */
std::string sslConnInfo(int port) {
    return "host=127.0.0.1 port=" + std::to_string(port) +
           " sslmode=require connect_timeout=" +
           std::to_string(kLoopbackTimeoutSeconds);
}

} // namespace

// The count is process-wide, so this starts from a count of zero rather than
// draining whatever is there: a context leaked by an earlier test fails here
// instead of being absorbed by extra destroy calls.
TEST(SslConnectionCount, ExtraDestroyCallsStopAtZero) {
    long const base{pqsecure_open_connection_count()};
    ASSERT_EQ(base, 0L) << "an earlier test left " << base
                        << " SSL contexts counted";
    pqsecure_destroy();
    EXPECT_EQ(pqsecure_open_connection_count(), 0L) << "count went below zero";
}

TEST(SslConnectionCount, RisesDuringHandshakeAndFallsOnClose) {
    SslRequestAcceptor server;
    ASSERT_NE(server.port(), 0) << "could not open a loopback listener";
    std::promise<void> release;
    std::thread peer{&SslRequestAcceptor::serveOne, &server,
                     release.get_future().share()};
    long const base{pqsecure_open_connection_count()};

    PGconn *const conn{PQconnectStart(sslConnInfo(server.port()).c_str())};
    if (conn == nullptr) {
        // Join first: returning with the peer thread running aborts the binary.
        release.set_value();
        peer.join();
        FAIL() << "PQconnectStart returned no connection";
    }
    // Drive the connect until libpq has sent its ClientHello and is waiting. As
    // libpq documents, start as if PQconnectPoll asked to write.
    static constexpr int kPollIntervalMs{50};
    static constexpr int kMaxPolls{kLoopbackTimeoutMs / kPollIntervalMs};
    PostgresPollingStatusType status{PGRES_POLLING_WRITING};
    for (int pollCount = 0;
         pollCount < kMaxPolls && pqsecure_open_connection_count() == base;
         ++pollCount) {
        waitForPoll(PQsocket(conn), status, kPollIntervalMs);
        status = PQconnectPoll(conn);
        if (status == PGRES_POLLING_FAILED) {
            break;
        }
    }
    long const during{pqsecure_open_connection_count()};
    std::string const connectError{PQerrorMessage(conn)};

    PQfinish(conn);
    long const after{pqsecure_open_connection_count()};
    release.set_value();
    peer.join();

    // A failed connect undoes its count before the loop sees it, so report
    // that as such rather than as a missing count.
    ASSERT_NE(status, PGRES_POLLING_FAILED)
        << "the connect failed before it was counted: " << connectError;
    EXPECT_EQ(during, base + 1)
        << "the SSL connect was not counted within " << kLoopbackTimeoutMs
        << " ms";
    EXPECT_EQ(after, base) << "closing the connection did not undo its count";
}

TEST(SslConnectionCount, FailedHandshakesReturnToBaseline) {
    SslRequestAcceptor server;
    ASSERT_NE(server.port(), 0) << "could not open a loopback listener";
    std::promise<void> now;
    now.set_value();
    std::shared_future<void> const closeAtOnce{now.get_future().share()};
    long const base{pqsecure_open_connection_count()};

    static constexpr int kAttempts{20};
    for (int attempt = 0; attempt < kAttempts; ++attempt) {
        std::thread peer{&SslRequestAcceptor::serveOne, &server, closeAtOnce};
        PGconn *const conn{PQconnectdb(sslConnInfo(server.port()).c_str())};
        peer.join();
        ASSERT_NE(conn, nullptr)
            << "PQconnectdb returned no connection on attempt " << attempt;
        EXPECT_EQ(PQstatus(conn), CONNECTION_BAD)
            << "the handshake should fail";
        PQfinish(conn);
        ASSERT_EQ(pqsecure_open_connection_count(), base)
            << "count not undone after failed SSL connect " << attempt;
    }
}

/*
 * verify-ca against a loopback TLS server, trusting only the root each
 * connection names.
 */

namespace {

class TestServerCertificate {
  public:
    /**
     * @brief Mints a CA root and a server certificate for 127.0.0.1 that it
     *        signs, so the root is a real trust anchor (CA:TRUE, keyCertSign).
     * @param commonName Subject CN of the server certificate; the root's CN is
     *        this with " Root" added, so roots stay unique.
     */
    explicit TestServerCertificate(std::string const &commonName) {
        m_path = rsfake::uniquePath("rsodbc_root_") + ".crt";
        m_rootKey = rsfake::newKey();
        m_key = rsfake::newKey();
        m_root = m_rootKey != nullptr
                     ? rsfake::newCert(m_rootKey, commonName + " Root", nullptr,
                                       nullptr, nullptr)
                     : nullptr;
        m_cert = m_key != nullptr && m_root != nullptr
                     ? rsfake::newCert(m_key, commonName, m_root, m_rootKey,
                                       "IP:127.0.0.1")
                     : nullptr;
        // BIO: a FILE * from another C runtime crashes OpenSSL on Windows.
        BIO *const file{
            m_cert != nullptr ? BIO_new_file(m_path.c_str(), "w") : nullptr};
        m_ready = file != nullptr && PEM_write_bio_X509(file, m_root) == 1;
        BIO_free(file);
    }

    /** @brief Frees the certificates and keys and removes the PEM file. */
    ~TestServerCertificate() {
        X509_free(m_cert);
        X509_free(m_root);
        EVP_PKEY_free(m_key);
        EVP_PKEY_free(m_rootKey);
        RS_UNLINK(m_path.c_str());
    }

    /** @return true if the certificates were minted and the root written. */
    bool ready() const { return m_ready; }

    /** @return The PEM file holding the root, for sslrootcert. */
    std::string const &path() const { return m_path; }

    /**
     * @brief Builds a server context that presents this certificate.
     * @return The context, or nullptr on failure; the caller frees it.
     */
    SSL_CTX *newServerContext() const {
        SSL_CTX *ctx{SSL_CTX_new(TLS_server_method())};
        if (ctx != nullptr && (SSL_CTX_use_certificate(ctx, m_cert) != 1 ||
                               SSL_CTX_use_PrivateKey(ctx, m_key) != 1)) {
            SSL_CTX_free(ctx);
            ctx = nullptr;
        }
        return ctx;
    }

  private:
    std::string m_path;
    EVP_PKEY *m_rootKey{nullptr};
    EVP_PKEY *m_key{nullptr};
    X509 *m_root{nullptr};
    X509 *m_cert{nullptr};
    bool m_ready{false};
};

/* Outcome of one connect to the loopback TLS server. */
struct HandshakeResult {
    bool serverCompleted;    // the server finished its side of the handshake
    std::string clientError; // libpq's error message
};

/**
 * @brief Quotes a file path for a libpq connection string.
 * @param path The path.
 * @return The path in single quotes, with quotes and backslashes escaped.
 */
std::string conninfoPath(std::string const &path) {
    std::string quoted{"'"};
    for (char const ch : path) {
        if (ch == '\\' || ch == '\'') {
            quoted += '\\';
        }
        quoted += ch;
    }
    return quoted + "'";
}

/**
 * @brief Connects once to the loopback server.
 * @param port The server's port.
 * @param options libpq options, such as sslmode and sslrootcert.
 * @return libpq's error message for the attempt.
 */
std::string connectWithOptions(int port, std::string const &options) {
    std::string const connInfo{"host=127.0.0.1 port=" + std::to_string(port) +
                               " connect_timeout=" +
                               std::to_string(kLoopbackTimeoutSeconds) + " " +
                               options};
    PGconn *const conn{PQconnectdb(connInfo.c_str())};
    std::string error{conn != nullptr ? PQerrorMessage(conn)
                                      : "no connection"};
    PQfinish(conn);
    return error;
}

/**
 * @brief Connects once to the loopback server, trusting only rootPath.
 * @param port The server's port.
 * @param sslMode verify-ca or verify-full.
 * @param rootPath PEM file passed as sslrootcert.
 * @return libpq's error message for the attempt.
 */
std::string connectWithRoot(int port, char const *sslMode,
                            std::string const &rootPath) {
    return connectWithOptions(port,
                              std::string{"sslmode="} + sslMode +
                                  " sslrootcert=" + conninfoPath(rootPath));
}

/**
 * @brief Serves one handshake and connects to it once.
 * @param server Loopback listener.
 * @param serverCtx Context the server handshakes with.
 * @param options libpq options for the client.
 * @return Whether the handshake completed, and libpq's error message.
 */
HandshakeResult serveAndConnect(SslRequestAcceptor &server, SSL_CTX *serverCtx,
                                std::string const &options) {
    std::future<bool> serverDone{
        std::async(std::launch::async, &SslRequestAcceptor::serveHandshake,
                   &server, serverCtx)};
    HandshakeResult result{false /* serverCompleted */,
                           connectWithOptions(server.port(), options)};
    result.serverCompleted = serverDone.get();
    return result;
}

/**
 * @brief Serves one handshake and connects to it once, trusting only rootPath.
 * @param server Loopback listener.
 * @param serverCtx Context with the certificate the server presents.
 * @param sslMode verify-ca or verify-full.
 * @param rootPath PEM file passed as sslrootcert.
 * @return Whether the handshake completed, and libpq's error message.
 */
HandshakeResult connectTrustingRoot(SslRequestAcceptor &server,
                                    SSL_CTX *serverCtx, char const *sslMode,
                                    std::string const &rootPath) {
    return serveAndConnect(server, serverCtx,
                           std::string{"sslmode="} + sslMode +
                               " sslrootcert=" + conninfoPath(rootPath));
}

/* Deletes a file when it goes out of scope. */
struct ScopedFile {
    std::string path;
    ~ScopedFile() { RS_UNLINK(path.c_str()); }
};

/**
 * @brief Writes a PEM file holding the given certificates, then the key if
 *        any.
 * @param path The file to write.
 * @param certs Certificates, leaf first.
 * @param key Private key to append, or nullptr.
 * @return true if every item was written.
 */
bool writePem(std::string const &path, std::vector<X509 *> const &certs,
              EVP_PKEY *key) {
    // BIO: a FILE * from another C runtime crashes OpenSSL on Windows.
    BIO *const out{BIO_new_file(path.c_str(), "w")};
    bool ok{out != nullptr};
    for (X509 *const cert : certs) {
        ok = ok && PEM_write_bio_X509(out, cert) == 1;
    }
    if (key != nullptr) {
        ok = ok && PEM_write_bio_PrivateKey(out, key, nullptr, nullptr, 0,
                                            nullptr, nullptr) == 1;
    }
    BIO_free(out);
#ifndef _WIN32
    // libpq refuses a key file that group or others can read.
    ok = ok && chmod(path.c_str(), 0600) == 0;
#endif
    return ok;
}

/**
 * @brief Tells whether libpq refused the server's certificate chain.
 * @param clientError libpq's error message.
 * @return true if verification of the chain failed.
 */
bool chainWasRejected(std::string const &clientError) {
    return clientError.find("CERTIFICATE_VERIFY_FAILED") != std::string::npos;
}

} // namespace

class SslRootCertificate : public ::testing::Test {
  protected:
    /**
     * @brief Mints two unrelated roots and a server that presents the first.
     */
    void SetUp() override {
        ASSERT_NE(m_server.port(), 0) << "could not open a loopback listener";
        ASSERT_TRUE(m_serverCert.ready())
            << "could not mint " << m_serverCert.path();
        ASSERT_TRUE(m_otherCert.ready())
            << "could not mint " << m_otherCert.path();
        m_serverCtx = m_serverCert.newServerContext();
        ASSERT_NE(m_serverCtx, nullptr) << "could not build the server context";
    }

    /** @brief Frees the server context. */
    void TearDown() override { SSL_CTX_free(m_serverCtx); }

    SslRequestAcceptor m_server;
    TestServerCertificate m_serverCert{"rsodbc-test-server-root"};
    TestServerCertificate m_otherCert{"rsodbc-test-unrelated-root"};
    SSL_CTX *m_serverCtx{nullptr};
};

TEST_F(SslRootCertificate, MatchingRootCompletesHandshake) {
    HandshakeResult const result = connectTrustingRoot(
        m_server, m_serverCtx, "verify-ca", m_serverCert.path());
    EXPECT_TRUE(result.serverCompleted)
        << "handshake failed while trusting the server's own root: "
        << result.clientError;
}

TEST_F(SslRootCertificate, UnrelatedRootFailsVerification) {
    for (char const *const sslMode : {"verify-ca", "verify-full"}) {
        SCOPED_TRACE(sslMode);
        HandshakeResult const result = connectTrustingRoot(
            m_server, m_serverCtx, sslMode, m_otherCert.path());
        EXPECT_FALSE(result.serverCompleted)
            << sslMode << " accepted a server its root did not sign";
        EXPECT_TRUE(chainWasRejected(result.clientError))
            << "wrong error for an untrusted server: " << result.clientError;
    }
}

TEST_F(SslRootCertificate, RootLoadedByAnotherConnectionIsNotTrusted) {
    // A root trusted by one connection must not carry over to the next.
    HandshakeResult const first = connectTrustingRoot(
        m_server, m_serverCtx, "verify-ca", m_serverCert.path());
    ASSERT_TRUE(first.serverCompleted)
        << "control connect failed: " << first.clientError;

    HandshakeResult const second = connectTrustingRoot(
        m_server, m_serverCtx, "verify-ca", m_otherCert.path());
    EXPECT_FALSE(second.serverCompleted)
        << "a root loaded by an earlier connection was trusted by a later one";
}

TEST_F(SslRootCertificate, RepeatedHandshakesKeepCountBalanced) {
    static constexpr int kCycles{50};
    long const base{pqsecure_open_connection_count()};

    for (int cycle = 0; cycle < kCycles; ++cycle) {
        HandshakeResult const accepted = connectTrustingRoot(
            m_server, m_serverCtx, "verify-ca", m_serverCert.path());
        ASSERT_TRUE(accepted.serverCompleted)
            << "cycle " << cycle << ": " << accepted.clientError;
        ASSERT_EQ(pqsecure_open_connection_count(), base)
            << "count grew after a completed handshake on cycle " << cycle;

        HandshakeResult const refused = connectTrustingRoot(
            m_server, m_serverCtx, "verify-ca", m_otherCert.path());
        ASSERT_FALSE(refused.serverCompleted) << "cycle " << cycle;
        ASSERT_EQ(pqsecure_open_connection_count(), base)
            << "count grew after a refused handshake on cycle " << cycle;
    }
}

TEST_F(SslRootCertificate, ConcurrentConnectionsUseOnlyTheirOwnRoot) {
    static constexpr int kConnections{8};
    long const base{pqsecure_open_connection_count()};
    std::vector<std::future<bool>> servers;
    for (int index = 0; index < kConnections; ++index) {
        servers.push_back(std::async(std::launch::async,
                                     &SslRequestAcceptor::serveHandshake,
                                     &m_server, m_serverCtx));
    }

    // Even-numbered connections trust the server's root, odd ones an
    // unrelated root; all start together.
    std::promise<void> start;
    std::shared_future<void> const released{start.get_future().share()};
    std::vector<std::future<std::string>> clients;
    for (int index = 0; index < kConnections; ++index) {
        std::string const root{
            index % 2 == 0 ? m_serverCert.path() : m_otherCert.path()};
        clients.push_back(std::async(std::launch::async, [&, root]() {
            released.wait();
            return connectWithRoot(m_server.port(), "verify-ca", root);
        }));
    }
    start.set_value();

    for (int index = 0; index < kConnections; ++index) {
        std::string const error{clients[index].get()};
        if (index % 2 == 0) {
            EXPECT_FALSE(chainWasRejected(error))
                << "connection " << index << " trusted the server's root but "
                << "was refused: " << error;
        } else {
            EXPECT_TRUE(chainWasRejected(error))
                << "connection " << index << " trusted only an unrelated root "
                << "but was not refused: " << error;
        }
    }
    int completed{0};
    for (auto &server : servers) {
        completed += server.get() ? 1 : 0;
    }
    EXPECT_EQ(completed, kConnections / 2)
        << "handshakes completed by the server";
    EXPECT_EQ(pqsecure_open_connection_count(), base)
        << "count not back to its start after concurrent connects";
}

TEST_F(SslRootCertificate, ClientCertificateChainIsPresented) {
    // The intermediate must reach the server along with the leaf.
    using KeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
    using CertPtr = std::unique_ptr<X509, decltype(&X509_free)>;
    KeyPtr const rootKey{rsfake::newKey(), EVP_PKEY_free};
    KeyPtr const intermediateKey{rsfake::newKey(), EVP_PKEY_free};
    KeyPtr const leafKey{rsfake::newKey(), EVP_PKEY_free};
    ASSERT_TRUE(rootKey && intermediateKey && leafKey)
        << "could not generate keys";
    CertPtr const root{rsfake::newCert(rootKey.get(),
                                       "rsodbc-test-client-root", nullptr,
                                       nullptr, nullptr),
                       X509_free};
    ASSERT_NE(root, nullptr) << "could not mint the client root";
    CertPtr const intermediate{
        rsfake::newCert(intermediateKey.get(),
                        "rsodbc-test-client-intermediate", root.get(),
                        rootKey.get(), nullptr),
        X509_free};
    ASSERT_NE(intermediate, nullptr) << "could not mint the intermediate";
    CertPtr const leaf{rsfake::newCert(leafKey.get(), "rsodbc-test-client",
                                       intermediate.get(),
                                       intermediateKey.get(),
                                       "DNS:client.example"),
                       X509_free};
    ASSERT_NE(leaf, nullptr) << "could not mint the client certificate";

    ScopedFile const certFile{rsfake::uniquePath("rsodbc_client_") + ".crt"};
    ScopedFile const keyFile{rsfake::uniquePath("rsodbc_client_") + ".key"};
    ASSERT_TRUE(
        writePem(certFile.path, {leaf.get(), intermediate.get()}, nullptr))
        << "could not write " << certFile.path;
    ASSERT_TRUE(writePem(keyFile.path, {}, leafKey.get()))
        << "could not write " << keyFile.path;

    // The server trusts only the root, so it accepts the client only if the
    // intermediate arrives with the leaf.
    SSL_CTX_set_verify(m_serverCtx,
                       SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                       nullptr);
    ASSERT_EQ(
        X509_STORE_add_cert(SSL_CTX_get_cert_store(m_serverCtx), root.get()), 1)
        << "could not trust the client root on the server";

    HandshakeResult const result = serveAndConnect(
        m_server, m_serverCtx,
        "sslmode=verify-ca sslrootcert=" + conninfoPath(m_serverCert.path()) +
            " sslcert=" + conninfoPath(certFile.path) +
            " sslkey=" + conninfoPath(keyFile.path));
    EXPECT_TRUE(result.serverCompleted)
        << "the server could not verify the client's chain: "
        << result.clientError;
}

TEST_F(SslRootCertificate, DefaultRootsDoNotPickUpAnotherConnectionsCaFile) {
    // A CaFile one connection adds must not reach another connection's
    // default roots.
    HandshakeResult const withCaFile =
        serveAndConnect(m_server, m_serverCtx,
                        "sslmode=verify-ca ssldefaultrootcert=" +
                            conninfoPath(m_otherCert.path()) + " sslrootcert=" +
                            conninfoPath(m_serverCert.path()));
    ASSERT_TRUE(withCaFile.serverCompleted)
        << "control connect failed: " << withCaFile.clientError;

    HandshakeResult const bundleOnly =
        serveAndConnect(m_server, m_serverCtx,
                        "sslmode=verify-ca ssldefaultrootcert=" +
                            conninfoPath(m_otherCert.path()));
    EXPECT_FALSE(bundleOnly.serverCompleted)
        << "a CaFile loaded by one connection was trusted through the default "
           "roots";
    EXPECT_TRUE(chainWasRejected(bundleOnly.clientError))
        << "wrong error for an untrusted server: " << bundleOnly.clientError;
}

TEST_F(SslRootCertificate, ConcurrentConnectionsLoadTheDefaultRoots) {
    static constexpr int kConnections{8};
    std::vector<std::future<bool>> servers;
    for (int index = 0; index < kConnections; ++index) {
        servers.push_back(std::async(std::launch::async,
                                     &SslRequestAcceptor::serveHandshake,
                                     &m_server, m_serverCtx));
    }
    std::promise<void> start;
    std::shared_future<void> const released{start.get_future().share()};
    std::string const options{"sslmode=verify-ca ssldefaultrootcert=" +
                              conninfoPath(m_serverCert.path())};
    std::vector<std::future<std::string>> clients;
    for (int index = 0; index < kConnections; ++index) {
        clients.push_back(std::async(std::launch::async, [&]() {
            released.wait();
            return connectWithOptions(m_server.port(), options);
        }));
    }
    start.set_value();

    for (int index = 0; index < kConnections; ++index) {
        std::string const error{clients[index].get()};
        EXPECT_FALSE(chainWasRejected(error))
            << "connection " << index
            << " was refused by the default roots: " << error;
    }
    int completed{0};
    for (auto &server : servers) {
        completed += server.get() ? 1 : 0;
    }
    EXPECT_EQ(completed, kConnections) << "handshakes completed by the server";
}

TEST_F(SslRootCertificate, UnreadableDefaultRootsStillVerifyTheServer) {
    // With an unreadable bundle and a missing sslrootcert, the server is still
    // verified, so the connect fails instead of trusting any certificate.
    ScopedFile const bundle{rsfake::uniquePath("rsodbc_bad_bundle_") + ".crt"};
    {
        std::ofstream out{bundle.path.c_str()};
        out << "not a certificate\n";
    }
    std::string const missing{rsfake::uniquePath("rsodbc_missing_root_") +
                              ".crt"};
    HandshakeResult const result = serveAndConnect(
        m_server, m_serverCtx,
        "sslmode=require ssldefaultrootcert=" + conninfoPath(bundle.path) +
            " sslrootcert=" + conninfoPath(missing));
    EXPECT_FALSE(result.serverCompleted)
        << "connected without verifying the server";
    EXPECT_TRUE(chainWasRejected(result.clientError))
        << "wrong error for an unverified server: " << result.clientError;
}

TEST_F(SslRootCertificate, DefaultRootsAreReadOnEveryConnect) {
    // A bundle updated in place, such as one adding a rotated CA, is used by
    // the next connection without restarting the process.
    ScopedFile const bundle{rsfake::uniquePath("rsodbc_bundle_") + ".crt"};
    auto const copyRoot{[&bundle](std::string const &from) {
        std::ifstream in{from.c_str(), std::ios::binary};
        std::ofstream out{bundle.path.c_str(),
                          std::ios::binary | std::ios::trunc};
        out << in.rdbuf();
        return static_cast<bool>(out);
    }};
    std::string const options{"sslmode=verify-ca ssldefaultrootcert=" +
                              conninfoPath(bundle.path)};

    ASSERT_TRUE(copyRoot(m_otherCert.path()))
        << "could not write " << bundle.path;
    HandshakeResult const before =
        serveAndConnect(m_server, m_serverCtx, options);
    ASSERT_FALSE(before.serverCompleted)
        << "control connect trusted a bundle without the server's root";

    ASSERT_TRUE(copyRoot(m_serverCert.path()))
        << "could not write " << bundle.path;
    HandshakeResult const after =
        serveAndConnect(m_server, m_serverCtx, options);
    EXPECT_TRUE(after.serverCompleted)
        << "the updated bundle was not read: " << after.clientError;
}

TEST_F(SslRootCertificate, MinTlsAppliesPerConnection) {
    // The server stops at TLS 1.2, so only a connection that allows 1.2
    // completes.
    ASSERT_EQ(SSL_CTX_set_max_proto_version(m_serverCtx, TLS1_2_VERSION), 1)
        << "could not cap the server at TLS 1.2";
    std::string const trust{"sslmode=verify-ca sslrootcert=" +
                            conninfoPath(m_serverCert.path())};

    HandshakeResult const allows12 =
        serveAndConnect(m_server, m_serverCtx, trust + " min_tls=1.2");
    EXPECT_TRUE(allows12.serverCompleted)
        << "min_tls=1.2 was refused by a TLS 1.2 server: "
        << allows12.clientError;

    HandshakeResult const needs13 =
        serveAndConnect(m_server, m_serverCtx, trust + " min_tls=1.3");
    EXPECT_FALSE(needs13.serverCompleted)
        << "min_tls=1.3 connected to a TLS 1.2 server, so an earlier "
           "connection's minimum was used";

    HandshakeResult const allowsAgain =
        serveAndConnect(m_server, m_serverCtx, trust + " min_tls=1.2");
    EXPECT_TRUE(allowsAgain.serverCompleted)
        << "min_tls=1.2 was refused after a min_tls=1.3 connection: "
        << allowsAgain.clientError;
}

TEST_F(SslRootCertificate, UnreadableDefaultRootsFallBackToRootCert) {
    // A readable sslrootcert still verifies the server when the bundle cannot
    // be read.
    ScopedFile const bundle{rsfake::uniquePath("rsodbc_bad_bundle_") + ".crt"};
    {
        std::ofstream out{bundle.path.c_str()};
        out << "not a certificate\n";
    }
    HandshakeResult const result = serveAndConnect(
        m_server, m_serverCtx,
        "sslmode=verify-ca ssldefaultrootcert=" + conninfoPath(bundle.path) +
            " sslrootcert=" + conninfoPath(m_serverCert.path()));
    EXPECT_TRUE(result.serverCompleted)
        << "an unreadable bundle blocked a connection whose sslrootcert signs "
           "the server: "
        << result.clientError;
}

namespace {

/**
 * @brief Counts this process's open descriptors (handles on Windows).
 * @return The count, or -1 if it cannot be read.
 */
long openDescriptorCount() {
#if defined(_WIN32)
    DWORD handles{0};
    return GetProcessHandleCount(GetCurrentProcess(), &handles) ? (long)handles
                                                                : -1;
#else
#if defined(__APPLE__)
    DIR *const fdDir{opendir("/dev/fd")};
#else
    DIR *const fdDir{opendir("/proc/self/fd")};
#endif
    if (fdDir == nullptr) {
        return -1;
    }
    long count{0};
    while (readdir(fdDir) != nullptr) {
        ++count;
    }
    closedir(fdDir);
    return count;
#endif
}

/**
 * @brief Connects through the driver to a server whose chain verify-full
 *        refuses, then frees the connection.
 */
void refuseOneConnect() {
    rsfake::FakeEndpoint endpoint{"untrusted"};
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();
    rsfake::Attempt const attempt =
        rsfake::connectTo(rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint,
                          ";sslmode=verify-full");
    ASSERT_FALSE(SQL_SUCCEEDED(attempt.rc))
        << "connected to an untrusted chain:\n"
        << attempt.diagnostics;
}

} // namespace

TEST(SslConnectionResources, RefusedConnectsReleaseTheirSockets) {
    // Freeing a refused connection must also close its socket.
    static constexpr int kAttempts{20};
    static constexpr long kDescriptorSlack{5};
    refuseOneConnect();
    ASSERT_FALSE(HasFatalFailure());
    long const before{openDescriptorCount()};
    ASSERT_GT(before, 0) << "cannot count this process's descriptors";

    for (int attempt = 0; attempt < kAttempts; ++attempt) {
        refuseOneConnect();
        ASSERT_FALSE(HasFatalFailure()) << "attempt " << attempt;
    }
    long const after{openDescriptorCount()};
    ASSERT_GT(after, 0) << "cannot count this process's descriptors";
    long const growth{after - before};
    EXPECT_LT(growth, kDescriptorSlack)
        << "descriptors grew by " << growth << " across " << kAttempts
        << " refused connects";
}
