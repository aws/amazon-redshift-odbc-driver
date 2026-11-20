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
#include <rsdesc.h>
#include <rsodbc.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <future>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

#ifndef WIN32
#include <pwd.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

extern "C" {
#include "libpq-fe.h"
#include "pq_tls_policy.h"
long pqsecure_open_connection_count(void);
void pqsecure_destroy(void);
#include "rs_ssl_common.h"
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

/* rs_ssl_common.h: which modes load the built-in root.crt. */

TEST(SslCaFilePolicy, only_the_verify_modes_promise_validation) {
    EXPECT_NE(0, rs_ssl_mode_verifies_cert("verify-ca"));
    EXPECT_NE(0, rs_ssl_mode_verifies_cert("verify-full"));
    EXPECT_EQ(0, rs_ssl_mode_verifies_cert("require"));
    EXPECT_EQ(0, rs_ssl_mode_verifies_cert("prefer"));
    EXPECT_EQ(0, rs_ssl_mode_verifies_cert("allow"));
    EXPECT_EQ(0, rs_ssl_mode_verifies_cert("disable"));
    EXPECT_EQ(0, rs_ssl_mode_verifies_cert(nullptr));
    EXPECT_EQ(0, rs_ssl_mode_verifies_cert(""));
}

TEST(SslCaFilePolicy, mode_names_are_matched_without_regard_to_case) {
    // A connection string carries whatever spelling the caller typed.
    EXPECT_NE(0, rs_ssl_mode_verifies_cert("Verify-CA"));
    EXPECT_NE(0, rs_ssl_mode_verifies_cert("VERIFY-FULL"));
    EXPECT_EQ(0, rs_ssl_mode_verifies_cert("PREFER"));
    EXPECT_EQ(0, rs_ssl_mode_verifies_cert("Require"));
    EXPECT_EQ(0, rs_ssl_mode_verifies_cert("Allow"));
    EXPECT_EQ(0, rs_ssl_mode_verifies_cert("DISABLE"));
}

TEST(SslCaFilePolicy, an_unknown_mode_name_validates_nothing) {
    // It cannot arrive: resolveSslPolicy() maps every mode to one of libpq's six names.
    EXPECT_EQ(0, rs_ssl_mode_verifies_cert("preferred"));
}

// A path that opens is not always usable: a directory opens on glibc, and a file whose mode
// denies reading stats clean. Each reason is reported separately.
TEST(SslCaFilePolicy, the_ca_file_state_helper_separates_missing_from_unreadable) {
    int savedErrno = -1;

    EXPECT_EQ(TrustedCaFileState::NotFound, rs_ssl_ca_file_state(nullptr, &savedErrno));
    EXPECT_EQ(TrustedCaFileState::NotFound, rs_ssl_ca_file_state("", &savedErrno));

    const std::string missing = rsfake::uniquePath("rsodbc_state_missing_") + ".crt";
    savedErrno = 0;
    EXPECT_EQ(TrustedCaFileState::NotFound,
              rs_ssl_ca_file_state(missing.c_str(), &savedErrno));
    EXPECT_NE(0, savedErrno) << "the reason stat() failed must be reported";

    const std::string dir = rsfake::uniquePath("rsodbc_state_dir_");
    ASSERT_EQ(0, RS_MKDIR(dir.c_str())) << "could not create " << dir;
    EXPECT_EQ(TrustedCaFileState::NotRegular, rs_ssl_ca_file_state(dir.c_str(), nullptr))
        << dir << " is a directory, which libpq cannot read a certificate out of";
    RS_RMDIR(dir.c_str());

    const std::string file = rsfake::uniquePath("rsodbc_state_file_") + ".crt";
    FILE *out = fopen(file.c_str(), "wb");
    ASSERT_NE(out, (FILE *)nullptr) << "could not create " << file;
    fputs("x\n", out);
    fclose(out);
    EXPECT_EQ(TrustedCaFileState::Ok, rs_ssl_ca_file_state(file.c_str(), nullptr));

#ifndef WIN32
    // A root process reads a mode-000 file, so there is no unreadable case to make.
    if (geteuid() != 0) {
        ASSERT_EQ(0, chmod(file.c_str(), 0)) << "could not take read permission away";
        savedErrno = 0;
        EXPECT_EQ(TrustedCaFileState::NotReadable,
                  rs_ssl_ca_file_state(file.c_str(), &savedErrno))
            << "a file the process cannot read must not pass as a usable CA file";
        EXPECT_EQ(EACCES, savedErrno);
        chmod(file.c_str(), 0600);
    }
#endif
    RS_UNLINK(file.c_str());
}

// The cheap screen for the built-in root.crt: a 4 KB read instead of parsing its 144 certificates.
TEST(SslCaFilePolicy, the_certificate_header_probe_accepts_a_pem_bundle_and_refuses_text) {
    const std::string builtInRoot = rsfake::builtInRootPath();
    if (builtInRoot.empty()) {
        GTEST_SKIP() << "the driver directory could not be determined, so there is "
                        "no installed bundle to read";
    }
    EXPECT_NE(0, rs_ssl_ca_file_has_certificate_header(builtInRoot.c_str()))
        << builtInRoot << " is the built-in root.crt libpq parses and must pass the probe";

    const std::string text = rsfake::uniquePath("rsodbc_probe_text_") + ".crt";
    FILE *out = fopen(text.c_str(), "wb");
    ASSERT_NE(out, (FILE *)nullptr) << "could not create " << text;
    fputs("not a certificate\n", out);
    fclose(out);
    EXPECT_EQ(0, rs_ssl_ca_file_has_certificate_header(text.c_str()))
        << "a file with no PEM block must not pass as a certificate bundle";
    RS_UNLINK(text.c_str());

    const std::string empty = rsfake::uniquePath("rsodbc_probe_empty_") + ".crt";
    out = fopen(empty.c_str(), "wb");
    ASSERT_NE(out, (FILE *)nullptr) << "could not create " << empty;
    fclose(out);
    EXPECT_EQ(0, rs_ssl_ca_file_has_certificate_header(empty.c_str()))
        << "an empty file holds no certificate";
    RS_UNLINK(empty.c_str());

    EXPECT_EQ(0, rs_ssl_ca_file_has_certificate_header(
                     (rsfake::uniquePath("rsodbc_probe_missing_") + ".crt").c_str()));
    EXPECT_EQ(0, rs_ssl_ca_file_has_certificate_header(nullptr));
    EXPECT_EQ(0, rs_ssl_ca_file_has_certificate_header(""));
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

namespace {

/**
 * Builds the path of the default CA file libpq loads when the connection names none. The home
 * directory cannot be redirected by tests; APPDATA stands in on Windows.
 * @return The path, or "" when it cannot be determined.
 */
std::string defaultHomeDirectory()
{
#ifdef WIN32
    const char *appdata = getenv("APPDATA");
    return (appdata != NULL && appdata[0] != '\0') ? std::string(appdata) + "/redshift"
                                                   : std::string();
#else
    struct passwd *pw = getpwuid(geteuid());
    return (pw != NULL && pw->pw_dir != NULL) ? std::string(pw->pw_dir) : std::string();
#endif
}

std::string defaultUserCertPath()
{
    const std::string home = defaultHomeDirectory();
    if (home.empty()) {
        return home;
    }
    // Forward slashes on both platforms, as libpq composes them.
#ifdef WIN32
    return home + "/root.crt";
#else
    return home + "/.redshift/root.crt";
#endif
}

/**
 * Sets an environment variable while in scope and restores the previous value or absence.
 * On Windows a variable set here may not reach the driver (a separate module with its own
 * environment table), so such cases run in a child process; see runCaseInChild().
 */
class ScopedEnv
{
public:
    /** Sets name to value. */
    ScopedEnv(const char *name, const std::string &value) : m_name(name)
    {
        const char *previous = getenv(name);
        m_had = previous != NULL;
        if (m_had) {
            m_previous = previous;
        }
        m_ok = set(value.c_str());
    }

    ~ScopedEnv()
    {
        if (m_had) {
            set(m_previous.c_str());
        } else {
#ifdef WIN32
            set("");  // an empty value removes the variable
#else
            unsetenv(m_name.c_str());
#endif
        }
    }

    ScopedEnv(const ScopedEnv &) = delete;
    ScopedEnv &operator=(const ScopedEnv &) = delete;

    /** @return True when the variable was set. */
    bool ok() const { return m_ok; }

private:
    bool set(const char *value)
    {
#ifdef WIN32
        return _putenv_s(m_name.c_str(), value) == 0;
#else
        return setenv(m_name.c_str(), value, 1) == 0;
#endif
    }

    std::string m_name;
    std::string m_previous;
    bool m_had = false;
    bool m_ok = false;
};

// The driver logs the channel a CA file came from (rslibpq.c, "SSLMode=%s, CA file channel=%s,
// built-in root.crt=%s"). The guard below reads it, so on Windows it stops guarding by itself
// if the module or CRT layout changes.
const char CA_FILE_CHANNEL_MARKER[] = "CA file channel=";
const char CA_FILE_CHANNEL_ENV[] = "PGSSLROOTCERT";

// How the driver names the service-file channel in that log line and in its diagnostics.
const char CA_FILE_CHANNEL_SERVICE_FILE[] = "service file sslrootcert";

// How the driver's warning names this channel when it drops an ambient CA file. The channel
// line then names the replacement, so the warning is the only record the variable was read.
const char CA_FILE_DROPPED_ENV_WARNING[] = "The PGSSLROOTCERT CA file";

/** @brief What the driver logged about the channel that supplied the CA file. */
enum EnvCaFileObservation
{
    ENV_CA_FILE_OBSERVED,     // the driver read PGSSLROOTCERT
    ENV_CA_FILE_NOT_OBSERVED, // the driver decided the channel, and it was not that one
    ENV_CA_FILE_UNKNOWN       // the log does not say, so the probe cannot tell
};

/** Reads the CA file channel out of a driver log. */
EnvCaFileObservation envCaFileObservation(const std::string &logText)
{
    if (logText.find(std::string(CA_FILE_CHANNEL_MARKER) + CA_FILE_CHANNEL_ENV) !=
        std::string::npos) {
        return ENV_CA_FILE_OBSERVED;
    }
    // A dropped CA file is replaced in the channel line, so the warning says the variable was read.
    if (logText.find(CA_FILE_DROPPED_ENV_WARNING) != std::string::npos) {
        return ENV_CA_FILE_OBSERVED;
    }
    if (logText.find(CA_FILE_CHANNEL_MARKER) != std::string::npos) {
        return ENV_CA_FILE_NOT_OBSERVED;
    }
    return ENV_CA_FILE_UNKNOWN;
}

/** The channel name the driver logged, "" when the log names none. */
std::string loggedCaFileChannel(const std::string &logText)
{
    const size_t at = logText.find(CA_FILE_CHANNEL_MARKER);
    if (at == std::string::npos) {
        return std::string();
    }
    const size_t from = at + sizeof(CA_FILE_CHANNEL_MARKER) - 1;
    const size_t to = logText.find(',', from);
    return logText.substr(from, to == std::string::npos ? to : to - from);
}

/**
 * Fails a case when the driver's log says the CA file came from somewhere other than
 * PGSSLROOTCERT. Used only in the child, where the variable was set before the driver loaded.
 * A log naming no channel fails too, since the probe could not tell.
 */
#define ASSERT_DRIVER_OBSERVED_ENV_CA_FILE(attempt)                                    \
    do {                                                                              \
        const EnvCaFileObservation observation =                                       \
            envCaFileObservation((attempt).logText);                                  \
        if (observation == ENV_CA_FILE_UNKNOWN) {                                      \
            FAIL() << "the driver log does not name the CA file channel, so "         \
                   << "this case cannot tell whether the driver observed "            \
                   << CA_FILE_CHANNEL_ENV << ". Expected a line containing \""         \
                   << CA_FILE_CHANNEL_MARKER << "\". The log was:\n"                   \
                   << (attempt).logText;                                              \
        }                                                                             \
        ASSERT_EQ(observation, ENV_CA_FILE_OBSERVED)                                   \
            << "the driver did not read " << CA_FILE_CHANNEL_ENV                       \
            << " even though it was set before the driver loaded, so the channel is " \
            << "no longer being read. The log named channel \""                       \
            << loggedCaFileChannel((attempt).logText) << "\".";                       \
    } while (0)

/*
 * Running a case in a child process, so an environment variable is in place before the driver
 * loads. In the parent the case prepares files and variables, re-executes this binary filtered
 * to that case and asserts the child passed. The child inherits the variables and runs the body.
 */

// How long a re-executed case may run before it is killed.
const int CHILD_TIMEOUT_SECONDS = 120;

// Set by the parent, so the child runs the case body instead of re-executing again.
const char ENV_CHILD_MARKER[] = "RSODBC_TEST_ENV_CA_FILE_CHILD";

// The service-file case's CA file path, passed to the child separately.
const char ENV_CHILD_SERVICE_CA_FILE[] = "RSODBC_TEST_SERVICE_CA_FILE";

/** @brief Tells whether this process is the re-executed child. */
bool inEnvCaFileChild()
{
    const char *marker = getenv(ENV_CHILD_MARKER);
    return marker != NULL && marker[0] == '1';
}

/** @brief Reads a variable the parent set, as "" when it is not there. */
std::string inheritedValue(const char *name)
{
    const char *value = getenv(name);
    return value != NULL ? std::string(value) : std::string();
}

/** The path of this test binary, "" when unknown. */
std::string testBinaryPath()
{
#ifdef WIN32
    char buf[MAX_PATH];
    const DWORD len = GetModuleFileNameA(NULL, buf, (DWORD)sizeof(buf));
    return (len > 0 && len < sizeof(buf)) ? std::string(buf, len) : std::string();
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t size = (uint32_t)sizeof(buf);
    return _NSGetExecutablePath(buf, &size) == 0 ? std::string(buf) : std::string();
#else
    char buf[4096];
    const ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0) {
        return std::string();
    }
    buf[len] = '\0';
    return std::string(buf);
#endif
}

/** @brief What came of re-executing this binary for one case. */
struct ChildRun
{
    bool launched = false;
    bool timedOut = false;
    int exitCode = -1;
    std::string output;
};

/**
 * Re-executes this binary for one case with this process's environment, killing it after
 * CHILD_TIMEOUT_SECONDS.
 * @return The child's exit code and output; launched is false if it could not start.
 */
ChildRun runCaseInChild(const std::string &fullTestName)
{
    ChildRun run;
    const std::string exe = testBinaryPath();

    if (exe.empty()) {
        run.output = "the path of this test binary is unknown";
        return run;
    }
    // Inherited by the child, which therefore runs the body rather than recursing.
    ScopedEnv marker(ENV_CHILD_MARKER, "1");
    if (!marker.ok() || !inEnvCaFileChild()) {
        run.output = std::string("could not set ") + ENV_CHILD_MARKER;
        return run;
    }
    const std::string filter = "--gtest_filter=" + fullTestName;
#ifdef WIN32
    SECURITY_ATTRIBUTES inherit = {sizeof(inherit), NULL, TRUE};
    HANDLE readEnd = NULL;
    HANDLE writeEnd = NULL;
    if (!CreatePipe(&readEnd, &writeEnd, &inherit, 0)) {
        return run;
    }
    SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    ZeroMemory(&startup, sizeof(startup));
    ZeroMemory(&process, sizeof(process));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writeEnd;
    startup.hStdError = writeEnd;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    std::string command = "\"" + exe + "\" " + filter + " --gtest_color=no";
    std::vector<char> commandLine(command.begin(), command.end());
    commandLine.push_back('\0');
    const BOOL created = CreateProcessA(NULL, commandLine.data(), NULL, NULL, TRUE, 0, NULL,
                                        NULL, &startup, &process);
    CloseHandle(writeEnd);
    if (!created) {
        CloseHandle(readEnd);
        return run;
    }
    run.launched = true;

    // Read on another thread, so a full pipe cannot stall the child past the timeout.
    std::thread reader([&run, readEnd]() {
        char buffer[512];
        DWORD got = 0;
        while (ReadFile(readEnd, buffer, (DWORD)sizeof(buffer), &got, NULL) && got > 0) {
            run.output.append(buffer, got);
        }
    });
    if (WaitForSingleObject(process.hProcess, CHILD_TIMEOUT_SECONDS * 1000) != WAIT_OBJECT_0) {
        TerminateProcess(process.hProcess, 1);
        WaitForSingleObject(process.hProcess, INFINITE);
        run.timedOut = true;
    }
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    run.exitCode = (int)code;
    reader.join();
    CloseHandle(readEnd);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
#else
    int fds[2];
    if (pipe(fds) != 0) {
        return run;
    }
    const pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return run;
    }
    if (pid == 0) {
        // Only async-signal-safe calls between fork and exec.
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        close(fds[0]);
        close(fds[1]);
        execl(exe.c_str(), exe.c_str(), filter.c_str(), "--gtest_color=no", (char *)NULL);
        _exit(127);
    }
    close(fds[1]);
    run.launched = true;

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(CHILD_TIMEOUT_SECONDS);
    for (;;) {
        const long long left = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   deadline - std::chrono::steady_clock::now())
                                   .count();
        if (left <= 0) {
            kill(pid, SIGKILL);
            run.timedOut = true;
            break;
        }
        struct pollfd readable = {fds[0], POLLIN, 0};
        const int ready = poll(&readable, 1, (int)std::min(left, 1000LL));
        if (ready < 0 && errno != EINTR) {
            break;
        }
        if (ready > 0) {
            char buffer[512];
            const ssize_t got = read(fds[0], buffer, sizeof(buffer));
            if (got <= 0) {
                break;
            }
            run.output.append(buffer, (size_t)got);
        }
    }
    close(fds[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    run.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
    return run;
}

/**
 * In the parent, runs this case in a child and leaves; in the child, falls through to the body.
 * The child must report exactly one passing case, so a skipped or unmatched case is not a pass.
 */
#define RUN_IN_CHILD_OR_CONTINUE(fullTestName)                                        \
    do {                                                                              \
        if (!inEnvCaFileChild()) {                                                    \
            const ChildRun run = runCaseInChild(fullTestName);                        \
            ASSERT_TRUE(run.launched)                                                 \
                << "could not re-execute this test binary to run " << fullTestName    \
                << " with the environment set before the driver loaded: "             \
                << run.output;                                                        \
            EXPECT_FALSE(run.timedOut) << fullTestName << " was killed after "        \
                << CHILD_TIMEOUT_SECONDS << " seconds. Child output:\n" << run.output;\
            EXPECT_EQ(0, run.exitCode) << fullTestName                                \
                << " failed in the child process. Child output:\n" << run.output;     \
            EXPECT_NE(run.output.find("[  PASSED  ] 1 test"), std::string::npos)      \
                << fullTestName << " did not run as one passing case in the child, "  \
                << "so this case proved nothing. Child output:\n" << run.output;      \
            return;                                                                   \
        }                                                                             \
    } while (0)

/**
 * The CA file an environment-channel case names through PGSSLROOTCERT. The parent settles the
 * path (minting the file if needed) and sets the variable; the child reads the path back.
 */
class EnvCaFile
{
public:
    enum Kind
    {
        READABLE,  // a real certificate, which signs nothing the endpoint presents
        MISSING,   // a path deliberately left absent
        DIRECTORY  // a directory, which fopen opens on glibc and OpenSSL cannot parse
    };

    explicit EnvCaFile(Kind kind)
    {
        if (inEnvCaFileChild()) {
            m_path = inheritedValue(CA_FILE_CHANNEL_ENV);
            return;
        }
        if (kind == READABLE) {
            m_ca.reset(new rsfake::UnrelatedCa());
            m_path = m_ca->path();
            m_ready = m_ca->ok();
        } else if (kind == DIRECTORY) {
            m_path = rsfake::uniquePath("rsodbc_dir_env_ca_file_");
            m_ownsDir = RS_MKDIR(m_path.c_str()) == 0;
            m_ready = m_ownsDir;
        } else {
            m_path = rsfake::uniquePath("rsodbc_missing_env_ca_file_") + ".crt";
        }
        m_env.reset(new ScopedEnv(CA_FILE_CHANNEL_ENV, m_path));
    }

    ~EnvCaFile()
    {
        if (m_ownsDir) {
            RS_RMDIR(m_path.c_str());
        }
    }

    EnvCaFile(const EnvCaFile &) = delete;
    EnvCaFile &operator=(const EnvCaFile &) = delete;

    /** @return True when there is a path, and whatever it names was really created. */
    bool ok() const { return !m_path.empty() && m_ready && (!m_env || m_env->ok()); }

    /** @return The path both halves of the case name. */
    const std::string &path() const { return m_path; }

private:
    std::unique_ptr<rsfake::UnrelatedCa> m_ca;
    std::unique_ptr<ScopedEnv> m_env;
    std::string m_path;
    bool m_ready = true;
    bool m_ownsDir = false;
};

/**
 * Tells whether this host has a default CA file, which libpq would load for any connection
 * and so leaves no mode without a CA file.
 * @return "" when the host is usable, otherwise the path to name in the skip.
 */
std::string blockingUserDefaultCert()
{
    const std::string path = defaultUserCertPath();
    if (path.empty()) {
        return std::string();
    }
    FILE *caFile = fopen(path.c_str(), "rb");
    if (caFile == NULL) {
        return std::string();
    }
    fclose(caFile);
    return path;
}

/**
 * A directory standing in for a home directory, with the user's default CA file absent or
 * present, removed on scope exit. The real home directory cannot be redirected by a test.
 */
class ScopedHomeDir
{
public:
    ScopedHomeDir() : m_path(rsfake::uniquePath("rsodbc_fake_home_"))
    {
        m_made = RS_MKDIR(m_path.c_str()) == 0;
    }

    ~ScopedHomeDir()
    {
        rsfake::removeFileIfPresent(caFilePath());
        if (!m_subdir.empty()) {
            RS_RMDIR(m_subdir.c_str());
        }
        if (m_made) {
            RS_RMDIR(m_path.c_str());
        }
    }

    ScopedHomeDir(const ScopedHomeDir &) = delete;
    ScopedHomeDir &operator=(const ScopedHomeDir &) = delete;

    /** @return Nonzero when the directory was created. */
    bool ok() const { return m_made; }

    /** @return The directory to pass where pqGetHomeDirectory() would report one. */
    const std::string &path() const { return m_path; }

    /** @return Where libpq would look for the CA file under this directory. */
    std::string caFilePath() const
    {
#ifdef WIN32
        return m_path + "/root.crt";
#else
        return m_path + "/.redshift/root.crt";
#endif
    }

    /** Creates the CA file, and its subdirectory on POSIX; true when the file is there. */
    bool createCaFile()
    {
#ifndef WIN32
        const std::string subdir = m_path + "/.redshift";
        if (RS_MKDIR(subdir.c_str()) != 0) {
            return false;
        }
        m_subdir = subdir;
#endif
        FILE *caFile = fopen(caFilePath().c_str(), "wb");
        if (caFile == NULL) {
            return false;
        }
        // The contents do not matter: the look-up asks only whether the file opens.
        const bool wrote = fwrite("x", 1, 1, caFile) == 1;
        return fclose(caFile) == 0 && wrote;
    }

private:
    std::string m_path;
    std::string m_subdir;
    bool m_made = false;
};

/**
 * The user's default root.crt, where libpq looks for it. A test cannot redirect
 * pqGetHomeDirectory(), so this writes the real file on a host that has none and removes it
 * again; a file already there is used as it is and left alone.
 */
class ScopedUserDefaultCert
{
public:
    /** Copies pemFile to where libpq looks, unless a file is already there. */
    explicit ScopedUserDefaultCert(const std::string &pemFile)
        : m_path(defaultUserCertPath())
    {
        if (m_path.empty()) {
            return;
        }
        if (rs_ssl_ca_file_state(m_path.c_str(), NULL) != TrustedCaFileState::NotFound) {
            // The host has one of its own, which serves this case just as well.
            m_available = true;
            return;
        }
        const size_t slash = m_path.rfind('/');
        if (slash != std::string::npos) {
            const std::string dir = m_path.substr(0, slash);
            if (RS_MKDIR(dir.c_str()) == 0) {
                m_createdDir = dir;
            } else if (errno != EEXIST) {
                return;
            }
        }
        const std::string pem = rsfake::readFileIfPresent(pemFile);
        if (pem.empty()) {
            return;
        }
        FILE *out = fopen(m_path.c_str(), "wb");
        if (out == NULL) {
            return;
        }
        const bool wrote = fwrite(pem.data(), 1, pem.size(), out) == pem.size();
        m_created = fclose(out) == 0 && wrote;
        m_available = m_created;
    }

    ~ScopedUserDefaultCert()
    {
        if (m_created) {
            rsfake::removeFileIfPresent(m_path);
        }
        if (!m_createdDir.empty()) {
            RS_RMDIR(m_createdDir.c_str());
        }
    }

    ScopedUserDefaultCert(const ScopedUserDefaultCert &) = delete;
    ScopedUserDefaultCert &operator=(const ScopedUserDefaultCert &) = delete;

    /** @return True when a CA file is there for libpq to load. */
    bool available() const { return m_available; }

    /** @return Where libpq looks, "" when that cannot be determined. */
    const std::string &path() const { return m_path; }

private:
    std::string m_path;
    std::string m_createdDir;
    bool m_created = false;
    bool m_available = false;
};

/**
 * A libpq service file naming one group with one keyword, removed on scope exit. libpq fills
 * options a connection left unset from it; sslrootcert and ssldefaultrootcert carry a CA file.
 */
class ScopedServiceFile
{
public:
    /** Writes the file: group goes in PGSERVICE, keyword gets value. */
    ScopedServiceFile(const std::string &group, const std::string &keyword,
                      const std::string &value)
        : m_path(rsfake::uniquePath("rsodbc_service_") + ".conf")
    {
        FILE *file = fopen(m_path.c_str(), "wb");
        if (file == NULL) {
            return;
        }
        const std::string text =
            "[" + group + "]\n" + keyword + "=" + value + "\n";
        const bool wrote = fwrite(text.data(), 1, text.size(), file) == text.size();
        m_ok = fclose(file) == 0 && wrote;
    }

    ~ScopedServiceFile() { rsfake::removeFileIfPresent(m_path); }

    ScopedServiceFile(const ScopedServiceFile &) = delete;
    ScopedServiceFile &operator=(const ScopedServiceFile &) = delete;

    /** @return Nonzero when the file was written. */
    bool ok() const { return m_ok; }

    /** @return The path to put in PGSERVICEFILE. */
    const std::string &path() const { return m_path; }

private:
    std::string m_path;
    bool m_ok = false;
};

/**
 * The service file a case points libpq at, and the CA file path it names. Split like EnvCaFile:
 * the parent writes the files and variables, the child reads the path back.
 */
class ServiceFileCaFile
{
public:
    ServiceFileCaFile(const std::string &keyword, const std::string &groupPrefix,
                      EnvCaFile::Kind kind)
    {
        if (inEnvCaFileChild()) {
            m_path = inheritedValue(ENV_CHILD_SERVICE_CA_FILE);
            return;
        }
        if (kind == EnvCaFile::READABLE) {
            m_ca.reset(new rsfake::UnrelatedCa());
            m_path = m_ca->path();
        } else if (kind == EnvCaFile::DIRECTORY) {
            m_path = rsfake::uniquePath("rsodbc_dir_service_ca_file_");
            m_ownsDir = RS_MKDIR(m_path.c_str()) == 0;
        } else {
            m_path = rsfake::uniquePath("rsodbc_missing_service_ca_file_") + ".crt";
        }
        // Unique per case, so a leftover service file cannot be the group read.
        const std::string group = groupPrefix + std::to_string((long)RS_GETPID());
        m_service.reset(new ScopedServiceFile(group, keyword, m_path));
        if (!m_service->ok()) {
            return;
        }
        m_serviceFileEnv.reset(new ScopedEnv("PGSERVICEFILE", m_service->path()));
        m_serviceNameEnv.reset(new ScopedEnv("PGSERVICE", group));
        m_caFileEnv.reset(new ScopedEnv(ENV_CHILD_SERVICE_CA_FILE, m_path));
    }

    ~ServiceFileCaFile()
    {
        if (m_ownsDir) {
            RS_RMDIR(m_path.c_str());
        }
    }

    ServiceFileCaFile(const ServiceFileCaFile &) = delete;
    ServiceFileCaFile &operator=(const ServiceFileCaFile &) = delete;

    /** @return True when both halves have a CA file path and the file was written. */
    bool ok() const
    {
        if (m_path.empty() || (m_ca && !m_ca->ok())) {
            return false;
        }
        return inEnvCaFileChild() ||
               (m_service && m_service->ok() && m_serviceFileEnv->ok() &&
                m_serviceNameEnv->ok() && m_caFileEnv->ok());
    }

    /** @brief Explains a setup that did not complete. */
    std::string why() const
    {
        if (m_service && !m_service->ok()) {
            return "could not write " + m_service->path();
        }
        return m_path.empty() ? std::string("no CA file path was settled")
                              : std::string();
    }

    /** @return The CA file path the service file names. */
    const std::string &path() const { return m_path; }

private:
    std::unique_ptr<rsfake::UnrelatedCa> m_ca;
    std::unique_ptr<ScopedServiceFile> m_service;
    std::unique_ptr<ScopedEnv> m_serviceFileEnv;
    std::unique_ptr<ScopedEnv> m_serviceNameEnv;
    std::unique_ptr<ScopedEnv> m_caFileEnv;
    std::string m_path;
    bool m_ownsDir = false;
};

/**
 * A service group with one keyword of the case's choosing, for groups ServiceFileCaFile
 * cannot express: one naming no CA file, and one with a line too long for libpq.
 * Split like ServiceFileCaFile.
 */
class ServiceFileSetting
{
public:
    ServiceFileSetting(const std::string &keyword, const std::string &value,
                       const std::string &groupPrefix)
    {
        if (inEnvCaFileChild()) {
            return;
        }
        const std::string group = groupPrefix + std::to_string((long)RS_GETPID());
        m_service.reset(new ScopedServiceFile(group, keyword, value));
        if (!m_service->ok()) {
            return;
        }
        m_serviceFileEnv.reset(new ScopedEnv("PGSERVICEFILE", m_service->path()));
        m_serviceNameEnv.reset(new ScopedEnv("PGSERVICE", group));
    }

    ServiceFileSetting(const ServiceFileSetting &) = delete;
    ServiceFileSetting &operator=(const ServiceFileSetting &) = delete;

    /** @return True in the child, or in the parent once the file was written. */
    bool ok() const
    {
        return inEnvCaFileChild() || (m_service && m_service->ok() && m_serviceFileEnv->ok() &&
                                      m_serviceNameEnv->ok());
    }

    /** @brief Explains a setup that did not complete. */
    std::string why() const
    {
        return (m_service && !m_service->ok())
                   ? "could not write " + m_service->path()
                   : std::string();
    }

private:
    std::unique_ptr<ScopedServiceFile> m_service;
    std::unique_ptr<ScopedEnv> m_serviceFileEnv;
    std::unique_ptr<ScopedEnv> m_serviceNameEnv;
};

}  // namespace

// Skips a test that needs no CA file at all on a host where libpq would supply one.
#define SKIP_WITHOUT_A_CA_FILE_FREE_HOST()                                             \
    do {                                                                               \
        const std::string userCert = blockingUserDefaultCert();                        \
        if (!userCert.empty()) {                                                       \
            GTEST_SKIP() << "this host has " << userCert << ", which libpq loads as"   \
                         << " a default CA file, so no mode here is CA-file-free";     \
        }                                                                              \
    } while (0)

/*
 * rs_ssl_compose_user_default_cert_path(): the CA file libpq takes from the home directory.
 * These cases sit here because the drift guard needs the helpers above.
 */

TEST(SslCaFilePolicy, user_default_cert_path_is_composed_like_libpq)
{
#ifdef WIN32
    // pqGetHomeDirectory() already reports a Redshift-specific directory on Windows.
    const char *home = "C:/Users/u/AppData/Roaming/redshift";
    const char *expected = "C:/Users/u/AppData/Roaming/redshift/root.crt";
#else
    const char *home = "/home/u";
    const char *expected = "/home/u/.redshift/root.crt";
#endif
    char buf[512];

    ASSERT_NE(0, rs_ssl_compose_user_default_cert_path(home, buf, sizeof(buf)));
    EXPECT_STREQ(expected, buf);

    // No home directory means there is no file to look for.
    EXPECT_EQ(0, rs_ssl_compose_user_default_cert_path(nullptr, buf, sizeof(buf)));
    EXPECT_EQ(0, rs_ssl_compose_user_default_cert_path("", buf, sizeof(buf)));
    EXPECT_STREQ(expected, buf) << "a refused composition must leave the buffer alone";

    // Exactly enough room fits. One byte less is refused, not truncated.
    const size_t needed = strlen(expected) + 1;
    char tight[512];
    memset(tight, 'x', sizeof(tight));
    EXPECT_NE(0, rs_ssl_compose_user_default_cert_path(home, tight, needed));
    EXPECT_STREQ(expected, tight);

    memset(tight, 'x', sizeof(tight));
    EXPECT_EQ(0, rs_ssl_compose_user_default_cert_path(home, tight, needed - 1));
    EXPECT_EQ('x', tight[0]) << "a path that does not fit must not be written at all";
}

// libpq composes this path in a MAXPGPATH (1024) buffer and would cut a longer one short, so the
// driver must refuse what libpq cannot hold, or the two would look at different files.
TEST(SslCaFilePolicy, a_home_directory_too_long_for_libpq_is_not_composed)
{
    const size_t suffixLen = strlen(RS_SSL_USER_DEFAULT_CERT_SUFFIX);
    char buf[RS_SSL_USER_DEFAULT_CERT_PATH_MAX];

    // Step 1: home + '/' + suffix + '\0' is 1030 bytes, about 1010 of them the home directory.
    const std::string tooLong = "/" + std::string(1028 - suffixLen - 1, 'h');
    EXPECT_EQ(0, rs_ssl_compose_user_default_cert_path(tooLong.c_str(), buf, sizeof(buf)))
        << "a " << tooLong.size() << "-byte home directory fits the driver's buffer but "
        << "not libpq's";

    // Step 2: exactly 1024 bytes with the terminator still fits.
    const std::string fits = "/" + std::string(1022 - suffixLen - 1, 'h');
    EXPECT_NE(0, rs_ssl_compose_user_default_cert_path(fits.c_str(), buf, sizeof(buf)));
    EXPECT_EQ(strlen(buf), (size_t)1023);
}

// The driver's composed path and the one these tests call libpq's must name the same file;
// if they drift, the look-up misses and SKIP_WITHOUT_A_CA_FILE_FREE_HOST() guards the wrong file.
TEST(SslCaFilePolicy, user_default_cert_path_matches_the_path_libpq_would_use)
{
    const std::string home = defaultHomeDirectory();
    if (home.empty()) {
        GTEST_SKIP() << "this host reports no home directory, so there is no path to "
                        "compare";
    }

    char buf[1024];
    ASSERT_NE(0, rs_ssl_compose_user_default_cert_path(home.c_str(), buf, sizeof(buf)));
    EXPECT_EQ(defaultUserCertPath(), std::string(buf))
        << "the driver looks for the user's default certificate file somewhere "
        << "other than where these tests expect libpq to, so one of the two has "
        << "drifted";
}

// With no such file, no CA file was named by any channel, so there is nothing for the
// look-up to find.
TEST(SslCaFilePolicy, an_absent_user_default_cert_is_not_found)
{
    ScopedHomeDir home;
    ASSERT_TRUE(home.ok()) << "could not create " << home.path();

    EXPECT_EQ(0, rs_ssl_user_default_cert_present(home.path().c_str()))
        << "nothing was created under " << home.path();

    // No home directory to search is the same answer: nothing was named.
    EXPECT_EQ(0, rs_ssl_user_default_cert_present(nullptr));
    EXPECT_EQ(0, rs_ssl_user_default_cert_present(""));
}

// A usable user-default CA is an ambient trust anchor, so every encrypting mode validates
// and adds the driver's built-in root.crt.
TEST(SslCaFilePolicy, a_present_user_default_cert_enables_validation_for_encrypting_modes)
{
    ScopedHomeDir home;
    ASSERT_TRUE(home.ok()) << "could not create " << home.path();
    ASSERT_TRUE(home.createCaFile()) << "could not create " << home.caFilePath();

    EXPECT_NE(0, rs_ssl_user_default_cert_present(home.path().c_str()))
        << home.caFilePath() << " is there and must be found";

    for (const char *mode : {"allow", "prefer", "require"}) {
        EXPECT_EQ(0, rs_ssl_mode_verifies_cert(mode))
            << mode << " is not intrinsically a verifying mode; CA resolution enables validation";
    }
}

/*====================================================================================================================================================*/

namespace {

class SslModeWithoutCaFile : public ::testing::TestWithParam<const char *> {};

}  // namespace

// With no CA file anywhere, require, prefer and allow encrypt without validating.
// allow opens in plaintext first, so its case covers the upgrade too.
TEST_P(SslModeWithoutCaFile, encrypts_without_validating)
{
    SKIP_WITHOUT_A_CA_FILE_FREE_HOST();
    const std::string mode = GetParam();

    rsfake::FakeEndpoint endpoint("untrusted");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint, ";sslmode=" + mode,
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    // The outcome, not just the absence of a chain error: the mode could fail another way.
    EXPECT_TRUE(SQL_SUCCEEDED(attempt.rc))
        << mode << " with no CA file must connect without validating. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_EQ(attempt.diagnostics.find(rsfake::CHAIN_REJECTED_ERROR), std::string::npos)
        << mode << " with no CA file must not fail on the chain. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.logText.find("built-in root.crt=not loaded"), std::string::npos)
        << mode << " must not load the built-in root.crt. the log was:\n"
        << attempt.logText;
    // The endpoint reads the password only inside TLS, so receiving it proves encryption.
    EXPECT_TRUE(endpoint.passwordReceived()) << mode << " must log in over TLS";
}

INSTANTIATE_TEST_SUITE_P(SslModeChainValidation, SslModeWithoutCaFile,
                         ::testing::Values("require", "prefer", "allow"),
                         [](const ::testing::TestParamInfo<const char *> &info) {
                             return std::string(info.param);
                         });

// prefer may use plaintext only when the server explicitly declines TLS with PostgreSQL's 'N'
// response. A TLS handshake, protocol, certificate, timeout, or authentication failure is not an
// explicit decline and must remain a connection failure.
TEST(SslModeChainValidation, prefer_uses_plaintext_after_an_explicit_tls_unsupported_response)
{
    SKIP_WITHOUT_A_CA_FILE_FREE_HOST();
    rsfake::FakeEndpoint endpoint("matching", /*acceptPlaintextAfterTlsRejection=*/false,
                                  /*declineTls=*/true);
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint, ";sslmode=prefer",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    EXPECT_TRUE(SQL_SUCCEEDED(attempt.rc))
        << "prefer must continue after an explicit TLS-unsupported response. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_TRUE(endpoint.plaintextPasswordReceived())
        << "the explicit TLS-unsupported response must permit exactly one plaintext login";
    EXPECT_FALSE(endpoint.passwordReceived())
        << "the endpoint declined TLS, so the password must not arrive over TLS";
}

// A protocol failure after accepting TLS is not PostgreSQL's explicit 'N' decline.
TEST(SslModeChainValidation, prefer_does_not_fall_back_to_plaintext_after_a_tls_protocol_failure)
{
    SKIP_WITHOUT_A_CA_FILE_FREE_HOST();
    rsfake::FakeEndpoint endpoint("matching", /*acceptPlaintextAfterTlsRejection=*/true,
                                  /*declineTls=*/false, /*sendInvalidTlsData=*/true);
    ASSERT_TRUE(endpoint.started()) << endpoint.startupOutput();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint, ";sslmode=prefer",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    EXPECT_FALSE(SQL_SUCCEEDED(attempt.rc)) << attempt.diagnostics;
    EXPECT_FALSE(endpoint.tlsHandshakeCompleted())
        << "the endpoint deliberately sent invalid TLS bytes after 'S'";
    EXPECT_FALSE(endpoint.passwordReceived())
        << "a failed handshake cannot send a TLS password";
    EXPECT_FALSE(endpoint.plaintextPasswordReceived())
        << "a TLS protocol failure must not expose credentials through a plaintext retry";
}

// Authentication rejection after a successful handshake remains an encrypted connection failure.
TEST(SslModeChainValidation, prefer_does_not_fall_back_to_plaintext_after_tls_authentication_failure)
{
    SKIP_WITHOUT_A_CA_FILE_FREE_HOST();
    rsfake::FakeEndpoint endpoint("matching", /*acceptPlaintextAfterTlsRejection=*/true,
                                  /*declineTls=*/false, /*sendInvalidTlsData=*/false,
                                  /*rejectAuthentication=*/true);
    ASSERT_TRUE(endpoint.started()) << endpoint.startupOutput();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint, ";sslmode=prefer",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    EXPECT_FALSE(SQL_SUCCEEDED(attempt.rc)) << attempt.diagnostics;
    EXPECT_TRUE(endpoint.tlsHandshakeCompleted())
        << "the authentication failure must occur after TLS was established";
    EXPECT_FALSE(endpoint.passwordReceived())
        << "the fake rejects authentication before requesting a TLS password";
    EXPECT_FALSE(endpoint.plaintextPasswordReceived())
        << "an authentication failure over TLS must not retry credentials in plaintext";
}

namespace {

class SslModeWithMismatchingCaFile : public ::testing::TestWithParam<const char *> {};

}  // namespace

// require and allow reject an untrusted TLS chain when a usable CA is named. The unrelated
// caller CA and the built-in root are additive, so neither trusts this chain.
TEST_P(SslModeWithMismatchingCaFile, rejects_untrusted_chain)
{
    const std::string mode = GetParam();
    rsfake::FakeEndpoint endpoint("untrusted");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    // supplyCaFile=true names a CA file that does not sign the leaf.
    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint, ";sslmode=" + mode,
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/true);

    EXPECT_FALSE(SQL_SUCCEEDED(attempt.rc))
        << mode << " must validate when a usable CA file was named. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.diagnostics.find(rsfake::CHAIN_REJECTED_ERROR), std::string::npos)
        << mode << " must reject the untrusted certificate chain. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.logText.find("built-in root.crt=loaded"), std::string::npos)
        << mode << " must add the built-in root.crt beside the CA file. the log was:\n"
        << attempt.logText;
    EXPECT_FALSE(endpoint.passwordReceived()) << mode << " must reject before login";
}

INSTANTIATE_TEST_SUITE_P(SslModeChainValidation, SslModeWithMismatchingCaFile,
                         ::testing::Values("require", "allow"),
                         [](const ::testing::TestParamInfo<const char *> &info) {
                             return std::string(info.param);
                         });

// prefer must surface certificate rejection and must never retry credentials in plaintext.
TEST(SslModeChainValidation, prefer_does_not_fall_back_to_plaintext_after_rejecting_tls_with_a_usable_ca)
{
    rsfake::FakeEndpoint endpoint("untrusted", /*acceptPlaintextAfterTlsRejection=*/true);
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint, ";sslmode=prefer",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/true);

    EXPECT_FALSE(SQL_SUCCEEDED(attempt.rc))
        << "prefer must surface an untrusted TLS chain. diagnostics were:\n" << attempt.diagnostics;
    EXPECT_NE(attempt.diagnostics.find(rsfake::CHAIN_REJECTED_ERROR), std::string::npos)
        << "the TLS chain rejection must remain visible. diagnostics were:\n" << attempt.diagnostics;
    EXPECT_FALSE(endpoint.passwordReceived())
        << "the rejected TLS attempt must not send the password";
    EXPECT_FALSE(endpoint.plaintextPasswordReceived())
        << "certificate rejection must not retry credentials in plaintext";
}

/*====================================================================================================================================================*/

// A user-default CA is an ambient trust anchor. require therefore validates it and adds the
// built-in root.crt.
TEST(SslModeChainValidation, require_with_only_a_user_default_ca_file_validates)
{
    rsfake::UnrelatedCa unrelatedCa;
    ASSERT_TRUE(unrelatedCa.ok()) << "could not mint a CA";
    ScopedUserDefaultCert userCert(unrelatedCa.path());
    if (!userCert.available()) {
        GTEST_SKIP() << "the user's default certificate file could not be arranged at "
                     << (userCert.path().empty() ? std::string("an unknown path")
                                                 : userCert.path());
    }

    rsfake::FakeEndpoint endpoint("untrusted");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint, ";sslmode=require",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    // Else the case would pass on a host where the file was never found.
    ASSERT_EQ(loggedCaFileChannel(attempt.logText), "user default certificate file")
        << "the driver did not resolve " << userCert.path() << ", so this case proves "
        << "nothing. the log was:\n"
        << attempt.logText;
    EXPECT_FALSE(SQL_SUCCEEDED(attempt.rc))
        << "require must validate when the user has a default certificate file. "
        << "diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.diagnostics.find(rsfake::CHAIN_REJECTED_ERROR), std::string::npos)
        << "require must reject the untrusted certificate chain. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.logText.find("built-in root.crt=loaded"), std::string::npos)
        << "the user's default certificate file must add the built-in root.crt. "
        << "the log was:\n"
        << attempt.logText;
    EXPECT_FALSE(endpoint.passwordReceived()) << "require must reject before login";
}

// The only test that reaches the built-in root.crt with a verify mode and no caller CA file.
// Without it, dropping the built-in root.crt for verify modes would go unnoticed, as every
// other test supplies a CaFile. A stray user default CA file cannot turn rejection into success.
TEST(SslModeChainValidation, verify_ca_without_a_ca_file_uses_the_built_in_root)
{
    rsfake::FakeEndpoint endpoint("untrusted");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint, ";sslmode=verify-ca",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    EXPECT_FALSE(SQL_SUCCEEDED(attempt.rc))
        << "verify-ca must reject a certificate that chains to nothing it trusts. "
        << "diagnostics were:\n"
        << attempt.diagnostics;
    // Any failure would pass the check above, so require the chain's; a missing root.crt fails differently.
    EXPECT_NE(attempt.diagnostics.find(rsfake::CHAIN_REJECTED_ERROR), std::string::npos)
        << "verify-ca with no caller CA file must validate against the built-in root.crt. "
        << "diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_FALSE(endpoint.passwordReceived())
        << "the password must not reach a server whose chain does not validate";
}

/*====================================================================================================================================================*/

namespace {

class SslModeKeepsBuiltInRoot : public ::testing::TestWithParam<const char *> {};

}  // namespace

// The endpoint's CA is added to the built-in root.crt and the connection names an unrelated
// CA file. The server chains only to the built-in root.crt, so connecting shows it was loaded
// beside the CA file. Only the verify modes load it, so only they are driven here. The shared
// root.crt is restored, and that is asserted too.
TEST_P(SslModeKeepsBuiltInRoot, beside_a_mismatching_ca_file)
{
    const std::string mode = GetParam();
    rsfake::FakeEndpoint endpoint("matching");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::BuiltInRootExtension builtInRoot(endpoint.caFile());
    ASSERT_TRUE(builtInRoot.ok())
        << "the built-in root.crt could not be extended: " << builtInRoot.why();
    rsfake::UnrelatedCa unrelatedCa;
    ASSERT_TRUE(unrelatedCa.ok()) << "could not mint an unrelated CA";

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "localhost", endpoint,
        ";sslmode=" + mode + ";CaFile=" + rsfake::connStrValue(unrelatedCa.path()),
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    EXPECT_TRUE(SQL_SUCCEEDED(attempt.rc))
        << mode << " must still trust the built-in root.crt when the CA file does not "
        << "sign the server. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_EQ(attempt.diagnostics.find(rsfake::CHAIN_REJECTED_ERROR), std::string::npos)
        << mode << " must not fail on the chain here. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.logText.find("built-in root.crt=loaded"), std::string::npos)
        << mode << " must load the built-in root.crt beside the CA file. the log was:\n"
        << attempt.logText;
    // Receiving the password proves the login went over TLS, not a plaintext retry.
    EXPECT_TRUE(endpoint.passwordReceived()) << mode << " must log in over TLS";
    EXPECT_TRUE(builtInRoot.restoreNow()) << builtInRoot.why();
}

INSTANTIATE_TEST_SUITE_P(SslModeChainValidation, SslModeKeepsBuiltInRoot,
                         ::testing::Values("verify-ca", "verify-full"),
                         [](const ::testing::TestParamInfo<const char *> &info) {
                             // A test name holds no '-'.
                             std::string name = info.param;
                             std::replace(name.begin(), name.end(), '-', '_');
                             return name;
                         });

/*====================================================================================================================================================*/

namespace {

/** Writes the certificate in a PEM file out again in DER form; true on success. */
bool writeDerCopy(const std::string &pemPath, const std::string &derPath)
{
    BIO *in = BIO_new_file(pemPath.c_str(), "r");
    X509 *cert = (in != NULL) ? PEM_read_bio_X509(in, NULL, NULL, NULL) : NULL;
    BIO_free(in);
    if (cert == NULL) {
        return false;
    }
    BIO *out = BIO_new_file(derPath.c_str(), "wb");
    const bool ok = out != NULL && i2d_X509_bio(out, cert) == 1;
    BIO_free(out);
    X509_free(cert);
    return ok;
}

/** An existing CaFile the driver cannot read a certificate out of. */
struct UnusableCaFileCase {
    const char *name;
    const char *kind;           // "text", "directory" or "der"
    const char *sslMode;
    const char *expectedReason; // part of the driver's diagnostic
};

class SslUnusableCaFile : public ::testing::TestWithParam<UnusableCaFileCase> {};

}  // namespace

// A CA file that exists but holds no certificate OpenSSL reads fails the connection in every
// mode. The driver decides before any socket opens, so prefer and allow cannot reach libpq's
// plaintext retry, which is where they would have landed in mainline and in 1.x.
TEST_P(SslUnusableCaFile, fails_the_connection_naming_the_option_and_the_reason)
{
    const UnusableCaFileCase &c = GetParam();
    rsfake::FakeEndpoint endpoint("matching");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::UnrelatedCa unrelatedCa;
    ASSERT_TRUE(unrelatedCa.ok()) << "could not mint a CA";
    const std::string path = rsfake::uniquePath("rsodbc_unusable_ca_file_") +
                             (std::string(c.kind) == "directory" ? "" : ".crt");
    const std::string kind = c.kind;
    if (kind == "directory") {
        ASSERT_EQ(0, RS_MKDIR(path.c_str())) << "could not create " << path;
    } else if (kind == "der") {
        ASSERT_TRUE(writeDerCopy(unrelatedCa.path(), path)) << "could not write " << path;
    } else {
        FILE *out = fopen(path.c_str(), "wb");
        ASSERT_NE(out, (FILE *)nullptr) << "could not create " << path;
        fputs("not a certificate\n", out);
        fclose(out);
    }

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "localhost", endpoint,
        std::string(";sslmode=") + c.sslMode + ";CaFile=" + rsfake::connStrValue(path),
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);
    if (kind == "directory") {
        RS_RMDIR(path.c_str());
    } else {
        RS_UNLINK(path.c_str());
    }

    EXPECT_FALSE(SQL_SUCCEEDED(attempt.rc))
        << "a CaFile with no usable certificate must fail the connection. diagnostics "
        << "were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.diagnostics.find(c.expectedReason), std::string::npos)
        << "the failure must be the driver's and give the reason. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.diagnostics.find("CaFile"), std::string::npos)
        << "the failure must name the option. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.diagnostics.find(path), std::string::npos)
        << "the failure must name the file. diagnostics were:\n"
        << attempt.diagnostics;
    // No passwordReceived(): the driver refuses before dialling, and the endpoint would
    // wait out its accept timeout.
}

INSTANTIATE_TEST_SUITE_P(
    SslModeChainValidation, SslUnusableCaFile,
    ::testing::Values(
        UnusableCaFileCase{"allow_text_file", "text", "allow",
                           rsfake::CA_FILE_NOT_LOADABLE_ERROR},
        UnusableCaFileCase{"prefer_text_file", "text", "prefer",
                           rsfake::CA_FILE_NOT_LOADABLE_ERROR},
        UnusableCaFileCase{"require_directory", "directory", "require",
                           rsfake::CA_FILE_NOT_A_FILE_ERROR},
        UnusableCaFileCase{"require_der_file", "der", "require",
                           rsfake::CA_FILE_NOT_LOADABLE_ERROR}),
    [](const ::testing::TestParamInfo<UnusableCaFileCase> &info) {
        return std::string(info.param.name);
    });

/*====================================================================================================================================================*/

namespace {

/** A built-in root.crt the driver cannot use. */
struct UnusableBuiltInRootCase {
    const char *name;
    const char *content;        // "" moves the file aside
    const char *expectedReason; // what the driver says about the built-in root.crt
};

class SslUnusableBuiltInRoot : public ::testing::TestWithParam<UnusableBuiltInRootCase> {};

}  // namespace

// With a usable CA file in play, an unusable built-in root.crt is warned about and the CA
// file validates alone. Mainline connects here too.
TEST_P(SslUnusableBuiltInRoot, warns_and_validates_against_a_usable_ca_file_alone)
{
    const UnusableBuiltInRootCase &c = GetParam();
    rsfake::BuiltInRootReplacement builtInRoot(c.content);
    ASSERT_TRUE(builtInRoot.ok())
        << "the built-in root.crt could not be replaced: " << builtInRoot.why();

    {
        rsfake::FakeEndpoint endpoint("matching");
        ASSERT_TRUE(endpoint.started())
            << "the fake endpoint did not start: " << endpoint.startupOutput();

        rsfake::Attempt attempt = rsfake::connectTo(
            rsfake::DIRECT_DRIVER_CLAUSE, "localhost", endpoint,
            ";sslmode=verify-full;CaFile=" + rsfake::connStrValue(endpoint.caFile()),
            rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

        EXPECT_TRUE(SQL_SUCCEEDED(attempt.rc))
            << "the CaFile signs the server, so the connection must succeed. "
            << "diagnostics were:\n"
            << attempt.diagnostics;
        EXPECT_NE(attempt.logText.find("validating against the CaFile CA file alone"),
                  std::string::npos)
            << "the log must warn that the built-in root.crt was not used. the log was:\n"
            << attempt.logText;
        EXPECT_NE(attempt.logText.find(c.expectedReason), std::string::npos)
            << "the warning must give the reason. the log was:\n"
            << attempt.logText;
        EXPECT_TRUE(endpoint.passwordReceived()) << "the login must go over TLS";
    }

    {
        // CaFile signs nothing here, so success would mean nothing was validated.
        rsfake::FakeEndpoint endpoint("untrusted");
        ASSERT_TRUE(endpoint.started())
            << "the fake endpoint did not start: " << endpoint.startupOutput();
        rsfake::UnrelatedCa unrelatedCa;
        ASSERT_TRUE(unrelatedCa.ok()) << "could not mint a CA";

        rsfake::Attempt attempt = rsfake::connectTo(
            rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint,
            ";sslmode=verify-ca;CaFile=" + rsfake::connStrValue(unrelatedCa.path()),
            rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

        EXPECT_NE(attempt.diagnostics.find(rsfake::CHAIN_REJECTED_ERROR), std::string::npos)
            << "verify-ca must still validate against the CaFile. diagnostics were:\n"
            << attempt.diagnostics;
        EXPECT_FALSE(endpoint.passwordReceived())
            << "the password must not reach a server whose chain does not validate";
    }

    EXPECT_TRUE(builtInRoot.restoreNow()) << builtInRoot.why();
}

// With no usable CA file, the verify modes have only the built-in root.crt, so an unusable one
// fails the connection.
TEST_P(SslUnusableBuiltInRoot, fails_a_verify_mode_with_no_ca_file)
{
    const UnusableBuiltInRootCase &c = GetParam();
    rsfake::BuiltInRootReplacement builtInRoot(c.content);
    ASSERT_TRUE(builtInRoot.ok())
        << "the built-in root.crt could not be replaced: " << builtInRoot.why();

    {
        rsfake::FakeEndpoint endpoint("matching");
        ASSERT_TRUE(endpoint.started())
            << "the fake endpoint did not start: " << endpoint.startupOutput();

        rsfake::Attempt attempt = rsfake::connectTo(
            rsfake::DIRECT_DRIVER_CLAUSE, "localhost", endpoint, ";sslmode=verify-ca",
            rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

        EXPECT_FALSE(SQL_SUCCEEDED(attempt.rc))
            << "verify-ca has nothing to validate against. diagnostics were:\n"
            << attempt.diagnostics;
    }

    EXPECT_TRUE(builtInRoot.restoreNow()) << builtInRoot.why();
}

// In a verify mode a named CA file that cannot be found falls back to the built-in root.crt.
// When that is unusable too, nothing could validate, so the connection fails instead of
// going unchecked.
TEST_P(SslUnusableBuiltInRoot, fails_a_verify_mode_whose_missing_ca_file_would_fall_back_to_it)
{
    const UnusableBuiltInRootCase &c = GetParam();
    rsfake::BuiltInRootReplacement builtInRoot(c.content);
    ASSERT_TRUE(builtInRoot.ok())
        << "the built-in root.crt could not be replaced: " << builtInRoot.why();

    {
        rsfake::FakeEndpoint endpoint("untrusted");
        ASSERT_TRUE(endpoint.started())
            << "the fake endpoint did not start: " << endpoint.startupOutput();
        const std::string missing = rsfake::uniquePath("rsodbc_missing_ca_file_") + ".crt";

        rsfake::Attempt attempt = rsfake::connectTo(
            rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint,
            ";sslmode=verify-ca;CaFile=" + rsfake::connStrValue(missing),
            rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

        EXPECT_FALSE(SQL_SUCCEEDED(attempt.rc))
            << "verify-ca must not connect unvalidated after asking for a CA file. "
            << "diagnostics were:\n"
            << attempt.diagnostics;
        EXPECT_NE(attempt.diagnostics.find("built-in root.crt"), std::string::npos)
            << "the failure must be about the built-in root.crt. diagnostics were:\n"
            << attempt.diagnostics;
        EXPECT_NE(attempt.logText.find("reinstall"), std::string::npos)
            << "the log must say what to do. the log was:\n"
            << attempt.logText;
        EXPECT_NE(attempt.diagnostics.find("CaFile"), std::string::npos)
            << "the message must name the CA file that would have stood in, or it reads "
            << "as if none was asked for. diagnostics were:\n"
            << attempt.diagnostics;
    }

    EXPECT_TRUE(builtInRoot.restoreNow()) << builtInRoot.why();
}

// A PEM header alone passes the quick probe, so only the full parse beside a CA file catches it.
INSTANTIATE_TEST_SUITE_P(
    SslModeChainValidation, SslUnusableBuiltInRoot,
    ::testing::Values(
        UnusableBuiltInRootCase{"missing", "", rsfake::CA_FILE_UNREADABLE_ERROR},
        UnusableBuiltInRootCase{"not_a_certificate", "not a certificate\n",
                                rsfake::CA_FILE_NOT_LOADABLE_ERROR},
        UnusableBuiltInRootCase{
            "pem_header_without_a_certificate",
            "-----BEGIN CERTIFICATE-----\nnot base64\n-----END CERTIFICATE-----\n",
            rsfake::CA_FILE_NOT_LOADABLE_ERROR}),
    [](const ::testing::TestParamInfo<UnusableBuiltInRootCase> &info) {
        return std::string(info.param.name);
    });

/*====================================================================================================================================================*/

// disable has no trust store, so an unreadable CA file is not its problem. It fails only
// because the endpoint refuses plaintext.
TEST(SslModeChainValidation, disable_ignores_an_unreadable_ca_file)
{
    rsfake::FakeEndpoint endpoint("matching");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    const std::string missing = rsfake::uniquePath("rsodbc_missing_ca_file_") + ".crt";

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "localhost", endpoint,
        ";sslmode=disable;CaFile=" + rsfake::connStrValue(missing),
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    EXPECT_EQ(attempt.diagnostics.find(rsfake::CA_FILE_UNREADABLE_ERROR), std::string::npos)
        << "disable must not be failed over a CA file it never loads. diagnostics were:\n"
        << attempt.diagnostics;
    // Else the case would also pass if disable failed before dialling and said nothing.
    EXPECT_FALSE(attempt.diagnostics.empty())
        << "disable must get as far as the endpoint and fail on its refused plaintext "
        << "startup";
}

/*====================================================================================================================================================*/

namespace {

// Longer than the old 260-byte buffer and the 1024 bytes libpq accepts, so the driver must refuse
// it, not shorten it to another file. Built under a missing directory so nothing opens it.
const size_t LONG_PATH_LENGTH = 1200;

// The size of the old CA file buffer; a diagnostic showing more bytes proves the path was not shortened.
const size_t OLD_CA_FILE_BUFFER_PATH_BYTES = 260;

// Part of the diagnostic for a path the driver could not hold, as opposed to one it could not open.
const char *const CA_FILE_TOO_LONG_ERROR = "which is more than";

/** Builds an absolute path of a given length that no file occupies. */
std::string longCaFilePath(size_t length)
{
#ifdef WIN32
    std::string path = "C:/rsodbc_long/";
#else
    std::string path = "/tmp/rsodbc_long/";
#endif
    const std::string suffix = "/root.crt";
    while (path.size() + suffix.size() < length) {
        path += 'a';
    }
    return path + suffix;
}

}  // namespace

namespace {

/** A named CA file whose path names no file. */
struct UnresolvedCaFileCase {
    const char *name;
    const char *sslMode;
    const char *option;  // TrustStore, CaFile or CaPath
    const char *kind;    // "missing", "empty_directory" or "over_long"
};

class SslUnresolvedCaFile : public ::testing::TestWithParam<UnresolvedCaFileCase> {};

}  // namespace

// In a verify mode a named CA file that cannot be found is warned about and the built-in
// root.crt validates instead, as on mainline. The endpoint chains to neither, so the
// chain error proves the connection was validated, not merely allowed.
TEST_P(SslUnresolvedCaFile, warns_and_validates_a_verify_mode_against_the_built_in_root)
{
    const UnresolvedCaFileCase &c = GetParam();
    const std::string kind = c.kind;
    rsfake::FakeEndpoint endpoint("untrusted");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    std::string path;
    if (kind == "over_long") {
        path = longCaFilePath(LONG_PATH_LENGTH);
    } else if (kind == "empty_directory") {
        path = rsfake::uniquePath("rsodbc_empty_ca_dir_");
        ASSERT_EQ(0, RS_MKDIR(path.c_str())) << "could not create " << path;
    } else {
        path = rsfake::uniquePath("rsodbc_missing_ca_file_") + ".crt";
    }

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint,
        std::string(";sslmode=") + c.sslMode + ";" + c.option + "=" +
            rsfake::connStrValue(path),
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);
    if (kind == "empty_directory") {
        RS_RMDIR(path.c_str());
    }

    EXPECT_NE(attempt.diagnostics.find(rsfake::CHAIN_REJECTED_ERROR), std::string::npos)
        << "the connection must be validated against the built-in root.crt. diagnostics "
        << "were:\n"
        << attempt.diagnostics;
    EXPECT_FALSE(endpoint.passwordReceived())
        << "the password must not reach a server whose chain does not validate";
    EXPECT_NE(attempt.logText.find(std::string("The ") + c.option + " CA file"),
              std::string::npos)
        << "the warning must name the option. the log was:\n"
        << attempt.logText;
    EXPECT_NE(attempt.logText.find("validating against the built-in root.crt instead"),
              std::string::npos)
        << "the log must say the built-in root.crt took its place. the log was:\n"
        << attempt.logText;
    EXPECT_NE(attempt.logText.find("built-in root.crt=loaded"), std::string::npos)
        << "the log must show the built-in root.crt was loaded. the log was:\n"
        << attempt.logText;
    if (kind == "over_long") {
        EXPECT_NE(attempt.logText.find(CA_FILE_TOO_LONG_ERROR), std::string::npos)
            << "the warning must give the length, not a shortened path. the log was:\n"
            << attempt.logText;
        EXPECT_EQ(attempt.logText.find(path.substr(0, OLD_CA_FILE_BUFFER_PATH_BYTES)),
                  std::string::npos)
            << "a shortened path must not be reported as the one asked for. the log "
            << "was:\n"
            << attempt.logText;
    } else {
        EXPECT_NE(attempt.logText.find(rsfake::CA_FILE_UNREADABLE_ERROR), std::string::npos)
            << "the warning must give the reason. the log was:\n"
            << attempt.logText;
    }
}

// The over-long cases cover a hole found on Windows, where an over-long path was shortened to
// another file. They run under a verify mode, where the fallback is what the caller gets.
INSTANTIATE_TEST_SUITE_P(
    SslModeChainValidation, SslUnresolvedCaFile,
    ::testing::Values(
        UnresolvedCaFileCase{"verify_ca_missing_ca_file", "verify-ca", "CaFile", "missing"},
        UnresolvedCaFileCase{"verify_full_missing_ca_file", "verify-full", "CaFile",
                             "missing"},
        UnresolvedCaFileCase{"verify_ca_missing_trust_store", "verify-ca", "TrustStore",
                             "missing"},
        UnresolvedCaFileCase{"verify_ca_ca_path_without_root_crt", "verify-ca", "CaPath",
                             "empty_directory"},
        UnresolvedCaFileCase{"verify_ca_over_long_trust_store", "verify-ca", "TrustStore",
                             "over_long"},
        UnresolvedCaFileCase{"verify_ca_over_long_ca_file", "verify-ca", "CaFile",
                             "over_long"},
        UnresolvedCaFileCase{"verify_ca_over_long_ca_path", "verify-ca", "CaPath",
                             "over_long"}),
    [](const ::testing::TestParamInfo<UnresolvedCaFileCase> &info) {
        return std::string(info.param.name);
    });

/*====================================================================================================================================================*/

namespace {

class SslUnresolvedCaFileWithFallback
    : public ::testing::TestWithParam<UnresolvedCaFileCase> {};

}  // namespace

// In every encrypting non-verify mode, a named CA file that cannot be resolved warns and
// validates against the built-in root.crt. The untrusted endpoint proves validation occurred.
TEST_P(SslUnresolvedCaFileWithFallback, warns_and_validates_against_the_built_in_root)
{
    const UnresolvedCaFileCase &c = GetParam();
    const std::string kind = c.kind;
    rsfake::FakeEndpoint endpoint("untrusted");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    std::string path;
    if (kind == "over_long") {
        path = longCaFilePath(LONG_PATH_LENGTH);
    } else if (kind == "empty_directory") {
        path = rsfake::uniquePath("rsodbc_empty_ca_dir_");
        ASSERT_EQ(0, RS_MKDIR(path.c_str())) << "could not create " << path;
    } else {
        path = rsfake::uniquePath("rsodbc_missing_ca_file_") + ".crt";
    }

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint,
        std::string(";sslmode=") + c.sslMode + ";" + c.option + "=" +
            rsfake::connStrValue(path),
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);
    if (kind == "empty_directory") {
        RS_RMDIR(path.c_str());
    }

    EXPECT_FALSE(SQL_SUCCEEDED(attempt.rc))
        << c.sslMode << " must validate against the built-in root.crt. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.diagnostics.find(rsfake::CHAIN_REJECTED_ERROR), std::string::npos)
        << c.sslMode << " must reject the untrusted certificate chain. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.logText.find(std::string("The ") + c.option + " CA file"),
              std::string::npos)
        << "the warning must name the option. the log was:\n"
        << attempt.logText;
    EXPECT_NE(attempt.logText.find("validating against the built-in root.crt instead"),
              std::string::npos)
        << "the warning must say the built-in root.crt replaced the unresolved CA file. "
        << "the log was:\n"
        << attempt.logText;
    EXPECT_NE(attempt.logText.find("built-in root.crt=loaded"), std::string::npos)
        << "the built-in root.crt must stand in for the unresolved CA file. the log was:\n"
        << attempt.logText;
    EXPECT_FALSE(endpoint.passwordReceived())
        << c.sslMode << " must reject before sending the password";
    if (kind == "over_long") {
        EXPECT_NE(attempt.logText.find(CA_FILE_TOO_LONG_ERROR), std::string::npos)
            << "the warning must give the length, not a shortened path. the log was:\n"
            << attempt.logText;
        EXPECT_EQ(attempt.logText.find(path.substr(0, OLD_CA_FILE_BUFFER_PATH_BYTES)),
                  std::string::npos)
            << "a shortened path must not be reported as the one asked for. the log "
            << "was:\n"
            << attempt.logText;
    }
}

INSTANTIATE_TEST_SUITE_P(
    SslModeChainValidation, SslUnresolvedCaFileWithFallback,
    ::testing::Values(
        UnresolvedCaFileCase{"require_missing_ca_file", "require", "CaFile", "missing"},
        UnresolvedCaFileCase{"allow_missing_ca_file", "allow", "CaFile", "missing"},
        UnresolvedCaFileCase{"require_missing_trust_store", "require", "TrustStore",
                             "missing"},
        UnresolvedCaFileCase{"require_ca_path_without_root_crt", "require", "CaPath",
                             "empty_directory"},
        UnresolvedCaFileCase{"require_over_long_ca_file", "require", "CaFile",
                             "over_long"}),
    [](const ::testing::TestParamInfo<UnresolvedCaFileCase> &info) {
        return std::string(info.param.name);
    });

// prefer attempts TLS first. A named-but-unresolved CA makes that TLS attempt validate against
// the built-in root.crt. A validation failure does not retry credentials in plaintext.
TEST(SslModeChainValidation, prefer_does_not_fall_back_to_plaintext_after_rejecting_tls_with_a_missing_ca)
{
    rsfake::FakeEndpoint endpoint("untrusted", /*acceptPlaintextAfterTlsRejection=*/true);
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();
    const std::string missing = rsfake::uniquePath("rsodbc_missing_ca_file_") + ".crt";

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint,
        ";sslmode=prefer;CaFile=" + rsfake::connStrValue(missing),
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    EXPECT_FALSE(SQL_SUCCEEDED(attempt.rc))
        << "prefer must surface the built-in-root chain rejection. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.diagnostics.find(rsfake::CHAIN_REJECTED_ERROR), std::string::npos)
        << "the TLS chain rejection must remain visible. diagnostics were:\n" << attempt.diagnostics;
    EXPECT_NE(attempt.logText.find("validating against the built-in root.crt instead"),
              std::string::npos)
        << "the missing named CA must still make the TLS attempt use the built-in root. "
        << "the log was:\n" << attempt.logText;
    EXPECT_FALSE(endpoint.passwordReceived())
        << "the rejected TLS attempt must not send the password";
    EXPECT_FALSE(endpoint.plaintextPasswordReceived())
        << "certificate rejection must not retry credentials in plaintext";
}

/*====================================================================================================================================================*/

// A verify mode whose named CA file cannot be found connects when the built-in root.crt signs
// the server.
TEST(SslModeChainValidation, a_missing_ca_file_connects_a_verify_mode_through_the_built_in_root)
{
    rsfake::FakeEndpoint endpoint("matching");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::BuiltInRootExtension builtInRoot(endpoint.caFile());
    ASSERT_TRUE(builtInRoot.ok())
        << "the built-in root.crt could not be extended: " << builtInRoot.why();
    const std::string missing = rsfake::uniquePath("rsodbc_missing_ca_file_") + ".crt";

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "localhost", endpoint,
        ";sslmode=verify-ca;CaFile=" + rsfake::connStrValue(missing),
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    EXPECT_TRUE(SQL_SUCCEEDED(attempt.rc))
        << "a missing CaFile must not fail the connection. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.logText.find(missing), std::string::npos)
        << "the warning must name the missing file. the log was:\n"
        << attempt.logText;
    EXPECT_TRUE(endpoint.passwordReceived()) << "verify-ca must log in over TLS";
    EXPECT_TRUE(builtInRoot.restoreNow()) << builtInRoot.why();
}

/*====================================================================================================================================================*/

// The longest path the buffers hold is kept whole, not refused for its length. It names no
// file, so the warning carries it.
TEST(SslModeChainValidation, a_ca_file_path_of_the_greatest_fitting_length_is_kept_whole)
{
    rsfake::FakeEndpoint endpoint("untrusted");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    const std::string longest = longCaFilePath(MAX_CERT_PATH_LEN - 1);
    ASSERT_EQ(longest.size(), (size_t)(MAX_CERT_PATH_LEN - 1));

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint,
        ";sslmode=verify-ca;CaFile=" + rsfake::connStrValue(longest),
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    EXPECT_EQ(attempt.logText.find(CA_FILE_TOO_LONG_ERROR), std::string::npos)
        << "a path of exactly the length the buffers hold must not be refused for its "
        << "length. the log was:\n"
        << attempt.logText;
    EXPECT_NE(attempt.logText.find(longest), std::string::npos)
        << "the warning must carry the whole path. the log was:\n"
        << attempt.logText;
    EXPECT_NE(attempt.diagnostics.find(rsfake::CHAIN_REJECTED_ERROR), std::string::npos)
        << "the built-in root.crt must validate in its place. diagnostics were:\n"
        << attempt.diagnostics;
}

/*====================================================================================================================================================*/

// A dropped value leaves its buffer empty, so options are weighed in resolution order. A too-long
// option ranking below the one naming the CA file is ignored, with a warning.

// CaPath ranks last, so TrustStore names the CA file whatever happened to CaPath.
TEST(SslModeChainValidation, an_over_long_lower_ranked_option_is_ignored)
{
    rsfake::FakeEndpoint endpoint("matching");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    const std::string tooLong = longCaFilePath(LONG_PATH_LENGTH);

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "localhost", endpoint,
        ";sslmode=verify-full;TrustStore=" +
            rsfake::connStrValue(endpoint.caFile()) + ";CaPath=" +
            rsfake::connStrValue(tooLong),
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    EXPECT_TRUE(SQL_SUCCEEDED(attempt.rc))
        << "the CA file TrustStore names is valid, so an over-long CaPath nothing "
        << "would have read must not fail the connection. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_EQ(loggedCaFileChannel(attempt.logText), "TrustStore")
        << "TrustStore must still name the CA file. the log was:\n"
        << attempt.logText;
    EXPECT_NE(attempt.logText.find("CaPath path was too long to keep and is ignored"),
              std::string::npos)
        << "the ignored value must still be reported in the log. the log was:\n"
        << attempt.logText;
}

namespace {

/** Adjacent usable CA options whose competing roots prove the higher rank wins. */
struct UsableHigherOptionCase {
    const char *name;
    const char *higherOption;
    const char *lowerOption;
    bool higherSignsServer;
};

class SslUsableHigherOption : public ::testing::TestWithParam<UsableHigherOptionCase> {};

/** An over-long option above one that names the server's CA. */
struct OverLongHigherOptionCase {
    const char *name;
    const char *overLongOption;
    const char *lowerOption;  // CaFile or CaPath, set to the endpoint's CA
};

class SslOverLongHigherOption : public ::testing::TestWithParam<OverLongHigherOptionCase> {};

}  // namespace

TEST_P(SslUsableHigherOption, wins_over_a_conflicting_usable_lower_option)
{
    const UsableHigherOptionCase &c = GetParam();
    rsfake::FakeEndpoint endpoint("matching");
    rsfake::UnrelatedCa unrelated;
    ASSERT_TRUE(endpoint.started()) << endpoint.startupOutput();
    ASSERT_TRUE(unrelated.ok()) << "could not mint an unrelated CA";

    const std::string trusted = endpoint.caFile();
    const std::string untrusted = unrelated.path();
    std::string higher = c.higherSignsServer ? trusted : untrusted;
    std::string lower = c.higherSignsServer ? untrusted : trusted;
    std::string caDir;
    std::string caDirRoot;
    if (std::string(c.lowerOption) == "CaPath") {
        caDir = rsfake::uniquePath("rsodbc_precedence_ca_dir_");
        caDirRoot = caDir + PATH_SEPARATOR_CHAR + "root.crt";
        ASSERT_EQ(0, RS_MKDIR(caDir.c_str()));
        const std::string pem = rsfake::readFileIfPresent(lower);
        FILE *out = fopen(caDirRoot.c_str(), "wb");
        ASSERT_NE(out, (FILE *)nullptr);
        fwrite(pem.data(), 1, pem.size(), out);
        fclose(out);
        lower = caDir;
    }

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint,
        std::string(";sslmode=verify-ca;") + c.higherOption + "=" +
            rsfake::connStrValue(higher) + ";" + c.lowerOption + "=" +
            rsfake::connStrValue(lower),
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);
    if (!caDirRoot.empty()) {
        RS_UNLINK(caDirRoot.c_str());
        RS_RMDIR(caDir.c_str());
    }

    EXPECT_EQ(SQL_SUCCEEDED(attempt.rc), c.higherSignsServer) << attempt.diagnostics;
    EXPECT_EQ(loggedCaFileChannel(attempt.logText), c.higherOption) << attempt.logText;
    EXPECT_EQ(endpoint.passwordReceived(), c.higherSignsServer);
}

INSTANTIATE_TEST_SUITE_P(
    SslModeChainValidation, SslUsableHigherOption,
    ::testing::Values(
        UsableHigherOptionCase{"trust_store_wins_over_ca_file", "TrustStore", "CaFile", true},
        UsableHigherOptionCase{"trust_store_blocks_ca_file", "TrustStore", "CaFile", false},
        UsableHigherOptionCase{"ca_file_wins_over_ca_path", "CaFile", "CaPath", true},
        UsableHigherOptionCase{"ca_file_blocks_ca_path", "CaFile", "CaPath", false}),
    [](const ::testing::TestParamInfo<UsableHigherOptionCase> &info) {
        return std::string(info.param.name);
    });

// An over-long option keeps its rank, so the lower option is not read and the built-in
// root.crt validates instead. The lower option signs the server, so rejection proves it.
TEST_P(SslOverLongHigherOption, is_not_replaced_by_the_lower_option)
{
    const OverLongHigherOptionCase &c = GetParam();
    rsfake::FakeEndpoint endpoint("matching");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    // CaPath names a directory holding root.crt, so copy the endpoint's CA into one.
    std::string lowerValue = endpoint.caFile();
    const std::string caDir = rsfake::uniquePath("rsodbc_ca_dir_");
    const std::string caDirRoot = caDir + PATH_SEPARATOR_CHAR + "root.crt";
    const bool lowerIsCaPath = std::string(c.lowerOption) == "CaPath";
    if (lowerIsCaPath) {
        ASSERT_EQ(0, RS_MKDIR(caDir.c_str())) << "could not create " << caDir;
        const std::string pem = rsfake::readFileIfPresent(endpoint.caFile());
        FILE *out = fopen(caDirRoot.c_str(), "wb");
        ASSERT_NE(out, (FILE *)nullptr) << "could not create " << caDirRoot;
        fwrite(pem.data(), 1, pem.size(), out);
        fclose(out);
        lowerValue = caDir;
    }

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "localhost", endpoint,
        std::string(";sslmode=verify-full;") + c.overLongOption + "=" +
            rsfake::connStrValue(longCaFilePath(LONG_PATH_LENGTH)) + ";" + c.lowerOption +
            "=" + rsfake::connStrValue(lowerValue),
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);
    if (lowerIsCaPath) {
        RS_UNLINK(caDirRoot.c_str());
        RS_RMDIR(caDir.c_str());
    }

    EXPECT_NE(attempt.diagnostics.find(rsfake::CHAIN_REJECTED_ERROR), std::string::npos)
        << c.lowerOption << " must not take the over-long option's place. diagnostics "
        << "were:\n"
        << attempt.diagnostics;
    EXPECT_EQ(loggedCaFileChannel(attempt.logText), c.overLongOption)
        << "the CA file must still come from the over-long option. the log was:\n"
        << attempt.logText;
    EXPECT_NE(attempt.logText.find(CA_FILE_TOO_LONG_ERROR), std::string::npos)
        << "the warning must give the length. the log was:\n"
        << attempt.logText;
    EXPECT_FALSE(endpoint.passwordReceived())
        << "the password must not reach a server whose chain does not validate";
}

INSTANTIATE_TEST_SUITE_P(
    SslModeChainValidation, SslOverLongHigherOption,
    ::testing::Values(
        OverLongHigherOptionCase{"trust_store_over_ca_file", "TrustStore", "CaFile"},
        OverLongHigherOptionCase{"ca_file_over_ca_path", "CaFile", "CaPath"}),
    [](const ::testing::TestParamInfo<OverLongHigherOptionCase> &info) {
        return std::string(info.param.name);
    });

/*====================================================================================================================================================*/

// The identity-provider requests read CaFile over HTTPS before the database connection looks at any
// CA file, so a too-long CaFile must fail here. A valid TrustStore is named too, since it outranks
// CaFile and the database connection would only warn. The message tells this gate from a later error.
TEST(SslModeChainValidation, an_iam_connection_with_a_dropped_ca_file_fails_closed)
{
    rsfake::FakeEndpoint endpoint("matching");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    const std::string tooLong = longCaFilePath(LONG_PATH_LENGTH);

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "localhost", endpoint,
        ";sslmode=verify-full;IAM=1;TrustStore=" +
            rsfake::connStrValue(endpoint.caFile()) + ";CaFile=" +
            rsfake::connStrValue(tooLong),
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    EXPECT_FALSE(SQL_SUCCEEDED(attempt.rc))
        << "an identity-provider request whose CA file was dropped must not connect. "
        << "diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.diagnostics.find(rsfake::IAM_CA_FILE_DROPPED_ERROR),
              std::string::npos)
        << "the failure must be the driver's, raised before the request went out, "
        << "and must say which request reads the path. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.diagnostics.find("CaFile"), std::string::npos)
        << "the failure must name the option that was dropped. diagnostics were:\n"
        << attempt.diagnostics;
}

/*====================================================================================================================================================*/

// Connects twice on one handle. The first connection names a CaFile and a CaPath. The
// second names neither, or names them empty, so it must not keep the first one's paths.
TEST(SslModeChainValidation, a_second_connection_on_the_same_handle_does_not_keep_the_first_ca_paths)
{
    if (rsfake::ansiEntryPointIsRemapped()) {
        GTEST_SKIP() << "this case calls the ANSI entry point directly";
    }
    SQLHENV env = SQL_NULL_HENV;
    SQLHDBC dbc = SQL_NULL_HDBC;
    ASSERT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env));
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    ASSERT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc));
    // Each endpoint serves one client, so every connection gets its own.
    auto connect = [&](const rsfake::FakeEndpoint &endpoint, const std::string &caPart) {
        const std::string connStr = std::string(rsfake::DIRECT_DRIVER_CLAUSE) +
                                    "Server=localhost;Port=" + std::to_string(endpoint.port()) +
                                    ";Database=dev;UID=victim;PWD=" + rsfake::PROBE_PASSWORD +
                                    ";sslmode=require" + caPart;
        SQLCHAR out[1024] = {0};
        SQLSMALLINT outLen = 0;
        const SQLRETURN rc = SQLDriverConnect(dbc, NULL, (SQLCHAR *)connStr.c_str(), SQL_NTS,
                                              out, sizeof(out), &outLen, SQL_DRIVER_NOPROMPT);
        SQLDisconnect(dbc);
        return rc;
    };
    const RS_CONN_INFO *pConn = (const RS_CONN_INFO *)dbc;
    const char *const laterParts[] = {"", ";CaFile=;CaPath="};
    for (const char *later : laterParts) {
        rsfake::FakeEndpoint first("matching");
        rsfake::FakeEndpoint second("matching");
        ASSERT_TRUE(first.started() && second.started())
            << "the fake endpoints did not start: " << first.startupOutput()
            << second.startupOutput();
        const std::string firstParts =
            ";CaFile=" + rsfake::connStrValue(first.caFile()) + ";CaPath=" +
            rsfake::connStrValue(rsfake::uniquePath("rsodbc_first_ca_dir_"));
        ASSERT_TRUE(SQL_SUCCEEDED(connect(first, firstParts)))
            << "the first connection must succeed";
        ASSERT_STREQ(pConn->pConnectProps->pIamProps->szCaFile, first.caFile().c_str())
            << "the first connection did not set the CaFile, so this case proves nothing";
        EXPECT_TRUE(SQL_SUCCEEDED(connect(second, later)))
            << "the second connection must succeed. later part: \"" << later << "\"";
        EXPECT_STREQ(pConn->pConnectProps->szCaFile, "")
            << "the second connection kept the first CaFile. later part: \"" << later << "\"";
        EXPECT_STREQ(pConn->pConnectProps->pIamProps->szCaFile, "")
            << "the identity-provider CaFile kept the first path. later part: \"" << later << "\"";
        EXPECT_STREQ(pConn->pConnectProps->szCaPath, "")
            << "the second connection kept the first CaPath. later part: \"" << later << "\"";
        EXPECT_STREQ(pConn->pConnectProps->pIamProps->szCaPath, "")
            << "the identity-provider CaPath kept the first path. later part: \"" << later << "\"";
    }
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
}
/*====================================================================================================================================================*/

// PGSSLROOTCERT can carry any length. One longer than libpq accepts is dropped with a warning
// naming the channel and length, not failed, since the environment named it; in a verify mode
// the built-in root.crt validates in its place.
TEST(SslModeChainValidation, verify_ca_with_an_over_long_env_ca_file_path_warns_and_falls_through)
{
    const std::string tooLong = longCaFilePath(LONG_PATH_LENGTH);
    ScopedEnv rootCert(CA_FILE_CHANNEL_ENV, tooLong);
    ASSERT_TRUE(rootCert.ok()) << "could not set " << CA_FILE_CHANNEL_ENV;
    RUN_IN_CHILD_OR_CONTINUE("SslModeChainValidation."
                             "verify_ca_with_an_over_long_env_ca_file_path_warns_and_falls_through");

    rsfake::FakeEndpoint endpoint("matching");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::BuiltInRootExtension builtInRoot(endpoint.caFile());
    ASSERT_TRUE(builtInRoot.ok())
        << "the built-in root.crt could not be extended, so this case cannot tell whether "
        << "it was loaded: " << builtInRoot.why();

    const std::string inherited = inheritedValue(CA_FILE_CHANNEL_ENV);
    ASSERT_GT(inherited.size(), (size_t)260)
        << "the parent did not pass an over-long path";

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "localhost", endpoint, ";sslmode=verify-ca",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    ASSERT_DRIVER_OBSERVED_ENV_CA_FILE(attempt);

    EXPECT_TRUE(SQL_SUCCEEDED(attempt.rc))
        << "a path the ambient environment named must not fail a connection that "
        << "never referenced it. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_EQ(attempt.diagnostics.find(CA_FILE_TOO_LONG_ERROR), std::string::npos)
        << "the length must be reported in the log, not raised to the caller. "
        << "diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.logText.find(CA_FILE_DROPPED_ENV_WARNING), std::string::npos)
        << "the log must name the channel whose value was dropped. the log was:\n"
        << attempt.logText;
    EXPECT_NE(attempt.logText.find(CA_FILE_TOO_LONG_ERROR), std::string::npos)
        << "the log must say the path was refused for its length, not for being "
        << "unreadable once shortened. the log was:\n"
        << attempt.logText;
    EXPECT_EQ(attempt.logText.find(
                  inherited.substr(0, OLD_CA_FILE_BUFFER_PATH_BYTES)),
              std::string::npos)
        << "the warning must not report a shortened path as though it were the one "
        << "the variable named. the log was:\n"
        << attempt.logText;
    // The endpoint's CA is only in the built-in root.crt, so a login shows it was loaded.
    EXPECT_NE(attempt.logText.find("built-in root.crt=loaded"), std::string::npos)
        << "the built-in root.crt must stand in for the dropped CA file. the log was:\n"
        << attempt.logText;
    EXPECT_TRUE(endpoint.passwordReceived())
        << "verify-ca must still log in over TLS, validated against the built-in root.crt";
    EXPECT_TRUE(builtInRoot.restoreNow()) << builtInRoot.why();
}

/*====================================================================================================================================================*/

// A path past 260 bytes but within libpq's limit is carried whole, so the readability check
// reports all of it. The channel is ambient, so an unreadable path is dropped with a warning,
// where the path is read back.
TEST(SslModeChainValidation, a_long_env_ca_file_path_within_libpqs_limit_is_carried_whole)
{
    const std::string longPath = longCaFilePath(280);
    ScopedEnv rootCert(CA_FILE_CHANNEL_ENV, longPath);
    ASSERT_TRUE(rootCert.ok()) << "could not set " << CA_FILE_CHANNEL_ENV;
    RUN_IN_CHILD_OR_CONTINUE("SslModeChainValidation."
                             "a_long_env_ca_file_path_within_libpqs_limit_is_carried_whole");

    rsfake::FakeEndpoint endpoint("matching");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::BuiltInRootExtension builtInRoot(endpoint.caFile());
    ASSERT_TRUE(builtInRoot.ok())
        << "the built-in root.crt could not be extended, so this case cannot tell whether "
        << "it was loaded: " << builtInRoot.why();

    const std::string inherited = inheritedValue(CA_FILE_CHANNEL_ENV);
    ASSERT_GT(inherited.size(), OLD_CA_FILE_BUFFER_PATH_BYTES);
    ASSERT_LT(inherited.size(), (size_t)1024);

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "localhost", endpoint, ";sslmode=verify-ca",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    ASSERT_DRIVER_OBSERVED_ENV_CA_FILE(attempt);

    EXPECT_EQ(attempt.logText.find(CA_FILE_TOO_LONG_ERROR), std::string::npos)
        << "a path this long still fits, so it must not be refused for its length. "
        << "the log was:\n"
        << attempt.logText;
    // More than the old buffer held, so it was not shortened.
    EXPECT_NE(attempt.logText.find(
                  inherited.substr(0, OLD_CA_FILE_BUFFER_PATH_BYTES + 1)),
              std::string::npos)
        << "the warning must carry more than " << OLD_CA_FILE_BUFFER_PATH_BYTES
        << " bytes of the path it was given. the log was:\n"
        << attempt.logText;
    EXPECT_TRUE(SQL_SUCCEEDED(attempt.rc))
        << "an unreadable path the ambient environment named must be dropped rather "
        << "than fail the connection. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.logText.find("built-in root.crt=loaded"), std::string::npos)
        << "the built-in root.crt must stand in for the dropped CA file. the log was:\n"
        << attempt.logText;
    EXPECT_TRUE(endpoint.passwordReceived())
        << "verify-ca must log in over TLS, validated against the built-in root.crt";
    EXPECT_TRUE(builtInRoot.restoreNow()) << builtInRoot.why();
}

/*====================================================================================================================================================*/

// The fake endpoint refuses a peer-supplied message length before subtracting from it or reading that many.
TEST(FakeEndpointMessageLength, only_lengths_in_range_are_read)
{
    // Below the minimum: a length counting only itself carries no payload, and subtracting four underflows.
    EXPECT_FALSE(rsfake::messageLengthIsUsable(0));
    EXPECT_FALSE(rsfake::messageLengthIsUsable(1));
    EXPECT_FALSE(rsfake::messageLengthIsUsable(3));
    EXPECT_FALSE(rsfake::messageLengthIsUsable(4));
    EXPECT_FALSE(rsfake::messageLengthIsUsable(-1));
    EXPECT_FALSE(rsfake::messageLengthIsUsable(INT32_MIN));

    // The boundaries themselves.
    EXPECT_TRUE(rsfake::messageLengthIsUsable(rsfake::MIN_MESSAGE_LENGTH));
    EXPECT_TRUE(rsfake::messageLengthIsUsable(rsfake::MAX_MESSAGE_LENGTH));
    EXPECT_FALSE(rsfake::messageLengthIsUsable(rsfake::MIN_MESSAGE_LENGTH - 1));
    EXPECT_FALSE(rsfake::messageLengthIsUsable(rsfake::MAX_MESSAGE_LENGTH + 1));

    // An ordinary startup packet, and a value no peer should be believed about.
    EXPECT_TRUE(rsfake::messageLengthIsUsable(96));
    EXPECT_FALSE(rsfake::messageLengthIsUsable(INT32_MAX));
}

/*====================================================================================================================================================*/

// PGSSLROOTCERT is a fourth way to name a CA file, so the resolution and fallback rules apply
// to it as well. Each case sets the variable and runs in a child process, because the variable
// must be set before the driver loads; see runCaseInChild().
TEST(SslModeChainValidation, verify_ca_with_an_unreadable_env_ca_file_connects_through_the_built_in_root)
{
    EnvCaFile caFile(EnvCaFile::MISSING);
    ASSERT_TRUE(caFile.ok()) << "no CA file path was settled for this case";
    RUN_IN_CHILD_OR_CONTINUE(
        "SslModeChainValidation.verify_ca_with_an_unreadable_env_ca_file_connects_through_the_built_in_root");

    rsfake::FakeEndpoint endpoint("matching");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    // The endpoint's CA is only in the built-in root.crt, so a login shows it was loaded.
    rsfake::BuiltInRootExtension builtInRoot(endpoint.caFile());
    ASSERT_TRUE(builtInRoot.ok())
        << "the built-in root.crt could not be extended, so this case cannot tell whether "
        << "it was loaded: " << builtInRoot.why();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "localhost", endpoint, ";sslmode=verify-ca",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    ASSERT_DRIVER_OBSERVED_ENV_CA_FILE(attempt);

    EXPECT_TRUE(SQL_SUCCEEDED(attempt.rc))
        << "nothing on this connection named the CA file, so an unreadable one must "
        << "not fail it. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_EQ(attempt.diagnostics.find(rsfake::CA_FILE_UNREADABLE_ERROR), std::string::npos)
        << "an ambient CA file must be reported in the log, not raised to the caller. "
        << "diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.logText.find(CA_FILE_DROPPED_ENV_WARNING), std::string::npos)
        << "the log must name the channel whose value was dropped. the log was:\n"
        << attempt.logText;
    EXPECT_NE(attempt.logText.find(caFile.path()), std::string::npos)
        << "the warning must name the path that could not be read. the log was:\n"
        << attempt.logText;
    EXPECT_NE(attempt.logText.find("built-in root.crt=loaded"), std::string::npos)
        << "the built-in root.crt must stand in for the dropped CA file. the log was:\n"
        << attempt.logText;
    EXPECT_TRUE(endpoint.passwordReceived())
        << "verify-ca must still log in over TLS, validated against the built-in root.crt";
    EXPECT_TRUE(builtInRoot.restoreNow()) << builtInRoot.why();
}

/*====================================================================================================================================================*/

// PGSSLROOTCERT naming a directory fails the connection, as it does on mainline, where libpq
// stats the path and then cannot load it. The driver says so before any socket opens.
TEST(SslModeChainValidation, an_env_ca_file_that_is_a_directory_fails_the_connection)
{
    EnvCaFile caFile(EnvCaFile::DIRECTORY);
    ASSERT_TRUE(caFile.ok()) << "no CA file path was settled for this case";
    RUN_IN_CHILD_OR_CONTINUE(
        "SslModeChainValidation.an_env_ca_file_that_is_a_directory_fails_the_connection");

    rsfake::FakeEndpoint endpoint("matching");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "localhost", endpoint, ";sslmode=require",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    ASSERT_DRIVER_OBSERVED_ENV_CA_FILE(attempt);

    EXPECT_FALSE(SQL_SUCCEEDED(attempt.rc))
        << "a directory is not a CA file. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.diagnostics.find(rsfake::CA_FILE_NOT_A_FILE_ERROR), std::string::npos)
        << "the failure must give the reason. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.diagnostics.find(CA_FILE_CHANNEL_ENV), std::string::npos)
        << "the failure must name the variable. diagnostics were:\n"
        << attempt.diagnostics;
    // No passwordReceived(): the driver refuses before dialling.
}

/*====================================================================================================================================================*/

// A missing ambient CA is named but unresolved, so require warns and validates through the
// built-in root.crt rather than silently weakening certificate validation.
TEST(SslModeChainValidation, require_with_an_unresolved_env_ca_file_uses_the_built_in_root)
{
    EnvCaFile caFile(EnvCaFile::MISSING);
    ASSERT_TRUE(caFile.ok()) << "no CA file path was settled for this case";
    RUN_IN_CHILD_OR_CONTINUE(
        "SslModeChainValidation.require_with_an_unresolved_env_ca_file_uses_the_built_in_root");

    rsfake::FakeEndpoint endpoint("untrusted");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint, ";sslmode=require",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    ASSERT_DRIVER_OBSERVED_ENV_CA_FILE(attempt);

    EXPECT_FALSE(SQL_SUCCEEDED(attempt.rc))
        << "a named CA that cannot resolve must validate through the built-in root.crt. "
        << "diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.diagnostics.find(rsfake::CHAIN_REJECTED_ERROR), std::string::npos)
        << "require must reject the untrusted certificate chain. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_EQ(attempt.diagnostics.find(rsfake::CA_FILE_UNREADABLE_ERROR), std::string::npos)
        << "a missing ambient CA file must be reported in the log, not raised to the caller. "
        << "diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.logText.find(CA_FILE_DROPPED_ENV_WARNING), std::string::npos)
        << "the log must name the channel whose value was dropped. the log was:\n"
        << attempt.logText;
    EXPECT_NE(attempt.logText.find("built-in root.crt=loaded"), std::string::npos)
        << "the built-in root.crt must replace the unresolved CA file. the log was:\n"
        << attempt.logText;
    EXPECT_FALSE(endpoint.passwordReceived()) << "require must reject before login";
}

/*====================================================================================================================================================*/

// A verify mode loads the built-in root.crt on any host, so after the drop it validates against
// the Redshift roots and the failure is the chain's, not the dropped path's.
TEST(SslModeChainValidation, verify_ca_with_an_unreadable_env_ca_file_rejects_a_chain_the_built_in_root_distrusts)
{
    EnvCaFile caFile(EnvCaFile::MISSING);
    ASSERT_TRUE(caFile.ok()) << "no CA file path was settled for this case";
    RUN_IN_CHILD_OR_CONTINUE(
        "SslModeChainValidation.verify_ca_with_an_unreadable_env_ca_file_rejects_a_chain_the_built_in_root_distrusts");

    rsfake::FakeEndpoint endpoint("matching");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "localhost", endpoint, ";sslmode=verify-ca",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    ASSERT_DRIVER_OBSERVED_ENV_CA_FILE(attempt);

    EXPECT_FALSE(SQL_SUCCEEDED(attempt.rc))
        << "verify-ca must still validate against the built-in root.crt. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.diagnostics.find(rsfake::CHAIN_REJECTED_ERROR), std::string::npos)
        << "the failure must be the certificate chain's, which is what proves the "
        << "built-in root.crt was loaded in the dropped CA file's place. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_EQ(attempt.diagnostics.find(rsfake::CA_FILE_UNREADABLE_ERROR), std::string::npos)
        << "an ambient CA file must be reported in the log, not raised to the caller. "
        << "diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.logText.find(CA_FILE_DROPPED_ENV_WARNING), std::string::npos)
        << "the log must name the channel whose value was dropped. the log was:\n"
        << attempt.logText;
}

/*====================================================================================================================================================*/

// A readable CA file named by PGSSLROOTCERT is a usable ambient trust anchor, so require
// validates against it and the additive built-in root.crt.
TEST(SslModeChainValidation, require_with_a_readable_env_ca_file_validates)
{
    EnvCaFile caFile(EnvCaFile::READABLE);
    ASSERT_TRUE(caFile.ok()) << "could not mint an unrelated CA";
    RUN_IN_CHILD_OR_CONTINUE(
        "SslModeChainValidation.require_with_a_readable_env_ca_file_validates");

    rsfake::FakeEndpoint endpoint("untrusted");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint, ";sslmode=require",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    ASSERT_DRIVER_OBSERVED_ENV_CA_FILE(attempt);

    EXPECT_FALSE(SQL_SUCCEEDED(attempt.rc))
        << "require must validate against a CA file PGSSLROOTCERT names. diagnostics "
        << "were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.diagnostics.find(rsfake::CHAIN_REJECTED_ERROR), std::string::npos)
        << "require must reject the untrusted certificate chain. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.logText.find("built-in root.crt=loaded"), std::string::npos)
        << "require must add the built-in root.crt beside the ambient CA file. the log was:\n"
        << attempt.logText;
    EXPECT_FALSE(endpoint.passwordReceived()) << "require must reject before login";
}

/*====================================================================================================================================================*/

// In a verify mode the built-in root.crt stays beside the CA file PGSSLROOTCERT names, rather
// than being replaced by it. That CA signs nothing and the endpoint's CA is in the built-in
// root.crt, so a connection shows both were loaded.
TEST(SslModeChainValidation, verify_ca_with_a_readable_env_ca_file_keeps_the_built_in_root)
{
    EnvCaFile caFile(EnvCaFile::READABLE);
    ASSERT_TRUE(caFile.ok()) << "could not mint an unrelated CA";
    RUN_IN_CHILD_OR_CONTINUE("SslModeChainValidation."
                             "verify_ca_with_a_readable_env_ca_file_keeps_the_built_in_root");

    rsfake::FakeEndpoint endpoint("matching");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::BuiltInRootExtension builtInRoot(endpoint.caFile());
    ASSERT_TRUE(builtInRoot.ok())
        << "the built-in root.crt could not be extended, so this case cannot tell the two "
        << "CA files apart: " << builtInRoot.why();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "localhost", endpoint, ";sslmode=verify-ca",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    // Asserted because an unread variable would still connect through the built-in root.crt and prove nothing.
    ASSERT_DRIVER_OBSERVED_ENV_CA_FILE(attempt);

    EXPECT_TRUE(SQL_SUCCEEDED(attempt.rc))
        << "verify-ca must still trust the built-in root.crt when the CA file "
        << "PGSSLROOTCERT names does not sign the server certificate. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_EQ(attempt.diagnostics.find(rsfake::CHAIN_REJECTED_ERROR), std::string::npos)
        << "verify-ca must not fail on the certificate chain here. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_TRUE(endpoint.passwordReceived())
        << "verify-ca must log in over TLS";
    EXPECT_TRUE(builtInRoot.restoreNow()) << builtInRoot.why();
}

/*====================================================================================================================================================*/

// disable has no trust store and ignores this channel like TrustStore, CaFile and CaPath. No CA file
// channel line is asserted, since disable never reaches it. It still runs in a child so the driver has the variable.
TEST(SslModeChainValidation, disable_ignores_an_env_ca_file)
{
    EnvCaFile caFile(EnvCaFile::MISSING);
    ASSERT_TRUE(caFile.ok()) << "no CA file path was settled for this case";
    RUN_IN_CHILD_OR_CONTINUE("SslModeChainValidation.disable_ignores_an_env_ca_file");

    rsfake::FakeEndpoint endpoint("matching");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "localhost", endpoint, ";sslmode=disable",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    EXPECT_EQ(attempt.diagnostics.find(rsfake::CA_FILE_UNREADABLE_ERROR), std::string::npos)
        << "disable must not be failed over a CA file it never loads. diagnostics were:\n"
        << attempt.diagnostics;
    // Else the case would also pass if disable failed before dialling and said nothing.
    EXPECT_FALSE(attempt.diagnostics.empty())
        << "disable must get as far as the endpoint and fail on its refused plaintext "
        << "startup";
}

/*====================================================================================================================================================*/

namespace {

// A libpq service file is a fifth way to name a CA file: libpq fills unset options from the group
// PGSERVICE names. sslrootcert is the caller's CA file, read back with PQconndefaults(), so it
// joins the built-in root.crt and fails closed when unreadable. ssldefaultrootcert is the driver's
// own, so a service group naming it changes nothing. These cases set PGSERVICEFILE and PGSERVICE
// and run in a child process; see runCaseInChild().

/**
 * Drives verify-ca against an unvalidatable endpoint, with a service file naming an unrelated
 * readable CA file through sslrootcert. The chain is rejected, and the log must name the
 * service file as the channel the CA file came from.
 */
void expectServiceFileCaFileUsed(const std::string &caFilePath,
                                 const std::string &keyword)
{
    rsfake::FakeEndpoint endpoint("untrusted");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    ASSERT_FALSE(caFilePath.empty()) << "the parent named no CA file";

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint, ";sslmode=verify-ca",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    EXPECT_EQ(loggedCaFileChannel(attempt.logText), CA_FILE_CHANNEL_SERVICE_FILE)
        << "the CA file a service group names through " << keyword << " must be the one "
        << "resolved. the log was:\n"
        << attempt.logText;
    EXPECT_FALSE(SQL_SUCCEEDED(attempt.rc))
        << "verify-ca must validate against the CA file a service group names through "
        << keyword << ". diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.diagnostics.find(rsfake::CHAIN_REJECTED_ERROR), std::string::npos)
        << "the failure must be the certificate chain's. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_FALSE(endpoint.passwordReceived())
        << "verify-ca must not send the password to a server that fails its CA file";
}

/** Drives require against an untrusted endpoint, with the unrelated CA file named through
    ssldefaultrootcert rather than sslrootcert. */
void expectServiceFileCaFileIgnored(const std::string &caFilePath,
                                    const std::string &keyword)
{
    rsfake::FakeEndpoint endpoint("untrusted");
    ASSERT_FALSE(caFilePath.empty()) << "the parent named no CA file";
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint, ";sslmode=require",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    // The driver's own keyword, so the group must leave the channel at none.
    EXPECT_EQ(loggedCaFileChannel(attempt.logText), "none")
        << "a group setting " << keyword << " must not be read as naming a CA file. "
        << "the log was:\n"
        << attempt.logText;
    EXPECT_NE(attempt.logText.find("built-in root.crt=not loaded"), std::string::npos)
        << "require must load no built-in root.crt whatever " << keyword << " says. "
        << "the log was:\n"
        << attempt.logText;
    EXPECT_TRUE(SQL_SUCCEEDED(attempt.rc))
        << "require must keep its own policy, which is to validate nothing. " << keyword
        << " diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_EQ(attempt.diagnostics.find(rsfake::CHAIN_REJECTED_ERROR), std::string::npos)
        << "the CA file the service file names through " << keyword << " must not have "
        << "become the only CA file the connection trusts. diagnostics were:\n"
        << attempt.diagnostics;
}

/**
 * The missing-file case of ssldefaultrootcert. The driver settles that keyword itself, so a
 * service group naming it never reaches libpq and the outcome is the no-CA-file one.
 */
void expectMissingServiceFileCaFileIgnored(const std::string &caFilePath,
                                           const std::string &keyword)
{
    rsfake::FakeEndpoint endpoint("untrusted");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    ASSERT_FALSE(caFilePath.empty()) << "the parent named no CA file";

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint, ";sslmode=require",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    EXPECT_TRUE(SQL_SUCCEEDED(attempt.rc))
        << "require must behave as it does with no CA file named at all. " << keyword
        << " diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_EQ(attempt.diagnostics.find(caFilePath), std::string::npos)
        << "nothing must consult a path the driver's channels never named. "
        << keyword << " diagnostics were:\n"
        << attempt.diagnostics;
}

}  // namespace

// A CA file a service group names through sslrootcert is one the caller asked for, so a verify
// mode validates against it.
TEST(SslModeChainValidation, verify_ca_uses_a_ca_file_a_service_file_names)
{
    ServiceFileCaFile caFile("sslrootcert", "rsodbc_present_", EnvCaFile::READABLE);
    ASSERT_TRUE(caFile.ok()) << caFile.why();
    RUN_IN_CHILD_OR_CONTINUE(
        "SslModeChainValidation.verify_ca_uses_a_ca_file_a_service_file_names");

    expectServiceFileCaFileUsed(caFile.path(), "sslrootcert");
}

/*====================================================================================================================================================*/

// In a verify mode the built-in root.crt stays beside the service file's CA file, rather than
// being replaced by it. That CA signs nothing and the endpoint's CA is in the built-in
// root.crt, so a connection shows both were loaded.
TEST(SslModeChainValidation, verify_ca_with_a_service_file_ca_file_keeps_the_built_in_root)
{
    ServiceFileCaFile caFile("sslrootcert", "rsodbc_additive_", EnvCaFile::READABLE);
    ASSERT_TRUE(caFile.ok()) << caFile.why();
    RUN_IN_CHILD_OR_CONTINUE("SslModeChainValidation."
                             "verify_ca_with_a_service_file_ca_file_keeps_the_built_in_root");

    rsfake::FakeEndpoint endpoint("matching");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::BuiltInRootExtension builtInRoot(endpoint.caFile());
    ASSERT_TRUE(builtInRoot.ok())
        << "the built-in root.crt could not be extended, so this case cannot tell the two "
        << "CA files apart: " << builtInRoot.why();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "localhost", endpoint, ";sslmode=verify-ca",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    // Asserted because an unread service file would still connect through the built-in root.crt.
    ASSERT_NE(attempt.logText.find(std::string(CA_FILE_CHANNEL_MARKER) +
                                   CA_FILE_CHANNEL_SERVICE_FILE),
              std::string::npos)
        << "the driver did not resolve the CA file from the service file, so this case "
        << "proves nothing. The log named channel \""
        << loggedCaFileChannel(attempt.logText) << "\".";

    EXPECT_TRUE(SQL_SUCCEEDED(attempt.rc))
        << "verify-ca must still trust the built-in root.crt when the CA file the service "
        << "file names does not sign the server certificate. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_EQ(attempt.diagnostics.find(rsfake::CHAIN_REJECTED_ERROR), std::string::npos)
        << "verify-ca must not fail on the certificate chain here. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_TRUE(endpoint.passwordReceived()) << "verify-ca must log in over TLS";
    EXPECT_TRUE(builtInRoot.restoreNow()) << builtInRoot.why();
}

/*====================================================================================================================================================*/

// A CA file the driver resolved is passed as sslrootcert and libpq fills only unset options, so a
// service group cannot replace it. CaFile is the endpoint's CA and the service file names one that
// signs nothing, so connecting shows the driver's value reached the trust store.
TEST(SslModeChainValidation, a_supplied_ca_file_wins_over_one_a_service_file_names)
{
    ServiceFileCaFile caFile("sslrootcert", "rsodbc_supplied_present_",
                             EnvCaFile::READABLE);
    ASSERT_TRUE(caFile.ok()) << caFile.why();
    RUN_IN_CHILD_OR_CONTINUE(
        "SslModeChainValidation.a_supplied_ca_file_wins_over_one_a_service_file_names");

    rsfake::FakeEndpoint endpoint("matching");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::Attempt attempt =
        rsfake::connectTo(rsfake::DIRECT_DRIVER_CLAUSE, "localhost", endpoint,
                          ";sslmode=verify-ca");

    EXPECT_TRUE(SQL_SUCCEEDED(attempt.rc))
        << "the CA file CaFile named must still be the one the connection trusts. "
        << "diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_EQ(attempt.diagnostics.find(rsfake::CHAIN_REJECTED_ERROR), std::string::npos)
        << "the CA file the service file names must not have replaced the one CaFile "
        << "named. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_TRUE(endpoint.passwordReceived()) << "verify-ca must log in over TLS";
}

/*====================================================================================================================================================*/

// A service file's sslrootcert is ambient: only PGSERVICE reaches it, and no ODBC option can
// set that. A missing path is unresolved, so it warns and validates through the built-in root.
TEST(SslModeChainValidation, verify_ca_with_a_missing_service_file_ca_file_uses_the_built_in_root)
{
    ServiceFileCaFile caFile("sslrootcert", "rsodbc_missing_", EnvCaFile::MISSING);
    ASSERT_TRUE(caFile.ok()) << caFile.why();
    RUN_IN_CHILD_OR_CONTINUE("SslModeChainValidation."
                             "verify_ca_with_a_missing_service_file_ca_file_uses_the_built_in_root");

    rsfake::FakeEndpoint endpoint("matching");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::BuiltInRootExtension builtInRoot(endpoint.caFile());
    ASSERT_TRUE(builtInRoot.ok())
        << "the built-in root.crt could not be extended: " << builtInRoot.why();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "localhost", endpoint, ";sslmode=verify-ca",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    EXPECT_TRUE(SQL_SUCCEEDED(attempt.rc)) << attempt.diagnostics;
    EXPECT_NE(attempt.logText.find(CA_FILE_CHANNEL_SERVICE_FILE), std::string::npos)
        << "the log must name the service file channel. the log was:\n" << attempt.logText;
    EXPECT_NE(attempt.logText.find(caFile.path()), std::string::npos)
        << "the warning must name the missing path. the log was:\n" << attempt.logText;
    EXPECT_NE(attempt.logText.find("built-in root.crt=loaded"), std::string::npos)
        << "the built-in root.crt must validate in place of the missing CA. the log was:\n"
        << attempt.logText;
    EXPECT_TRUE(endpoint.passwordReceived()) << "verify-ca must log in over TLS";
    EXPECT_TRUE(builtInRoot.restoreNow()) << builtInRoot.why();
}

TEST(SslModeChainValidation, a_directory_service_file_ca_fails_before_dialing)
{
    ServiceFileCaFile caFile("sslrootcert", "rsodbc_directory_", EnvCaFile::DIRECTORY);
    ASSERT_TRUE(caFile.ok()) << caFile.why();
    RUN_IN_CHILD_OR_CONTINUE(
        "SslModeChainValidation.a_directory_service_file_ca_fails_before_dialing");

    rsfake::FakeEndpoint endpoint("matching");
    ASSERT_TRUE(endpoint.started()) << endpoint.startupOutput();
    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "localhost", endpoint, ";sslmode=prefer",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    EXPECT_FALSE(SQL_SUCCEEDED(attempt.rc)) << attempt.diagnostics;
    EXPECT_NE(attempt.diagnostics.find(rsfake::CA_FILE_NOT_A_FILE_ERROR), std::string::npos)
        << attempt.diagnostics;
    EXPECT_NE(attempt.diagnostics.find(CA_FILE_CHANNEL_SERVICE_FILE), std::string::npos)
        << attempt.diagnostics;
    EXPECT_FALSE(endpoint.passwordReceived());
    EXPECT_FALSE(endpoint.plaintextPasswordReceived());
}

/*====================================================================================================================================================*/

// libpq stops at PGSSLROOTCERT even when it names no file, so the user's default certificate
// file is not consulted and, in a verify mode, the built-in root.crt validates alone. Skips on
// a host without such a file.
TEST(SslModeChainValidation, an_unusable_env_ca_file_does_not_fall_through_to_the_user_default_cert)
{
    if (blockingUserDefaultCert().empty()) {
        GTEST_SKIP() << "this host has no " << defaultUserCertPath()
                     << ", so there is no default CA file for the dropped one to fall "
                        "through to";
    }
    EnvCaFile caFile(EnvCaFile::MISSING);
    ASSERT_TRUE(caFile.ok()) << "no CA file path was settled for this case";
    RUN_IN_CHILD_OR_CONTINUE("SslModeChainValidation."
                             "an_unusable_env_ca_file_does_not_fall_through_to_the_user_default_cert");

    rsfake::FakeEndpoint endpoint("matching");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "localhost", endpoint, ";sslmode=verify-ca",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    ASSERT_DRIVER_OBSERVED_ENV_CA_FILE(attempt);

    EXPECT_EQ(loggedCaFileChannel(attempt.logText), CA_FILE_CHANNEL_ENV)
        << "the CA file must still come from PGSSLROOTCERT. the log was:\n"
        << attempt.logText;
    EXPECT_NE(attempt.logText.find("built-in root.crt=loaded"), std::string::npos)
        << "the built-in root.crt must validate in its place. the log was:\n"
        << attempt.logText;
}

/*====================================================================================================================================================*/

// ssldefaultrootcert is the driver's own keyword: it carries the built-in root.crt decision, so
// the driver always passes it (empty when there is none) and a service group naming it has
// nothing to fill. Only sslrootcert, the caller's keyword, can be supplied that way.
TEST(SslModeChainValidation, require_ignores_a_default_ca_file_a_service_file_names)
{
    SKIP_WITHOUT_A_CA_FILE_FREE_HOST();
    ServiceFileCaFile caFile("ssldefaultrootcert", "rsodbc_default_present_",
                             EnvCaFile::READABLE);
    ASSERT_TRUE(caFile.ok()) << caFile.why();
    RUN_IN_CHILD_OR_CONTINUE(
        "SslModeChainValidation.require_ignores_a_default_ca_file_a_service_file_names");

    expectServiceFileCaFileIgnored(caFile.path(), "ssldefaultrootcert");
}

/*====================================================================================================================================================*/

TEST(SslModeChainValidation,
     require_ignores_a_missing_default_ca_file_a_service_file_names)
{
    SKIP_WITHOUT_A_CA_FILE_FREE_HOST();
    ServiceFileCaFile caFile("ssldefaultrootcert", "rsodbc_default_missing_",
                             EnvCaFile::MISSING);
    ASSERT_TRUE(caFile.ok()) << caFile.why();
    RUN_IN_CHILD_OR_CONTINUE("SslModeChainValidation."
                             "require_ignores_a_missing_default_ca_file_a_service_file_names");

    expectMissingServiceFileCaFileIgnored(caFile.path(), "ssldefaultrootcert");
}

/*====================================================================================================================================================*/

// libpq fills an option from the service file before the environment, so the service file outranks
// PGSSLROOTCERT, and the driver keeps that order. The service file names a readable CA and
// PGSSLROOTCERT a missing path. require validates the resolved service-file CA, so the case
// checks precedence and certificate rejection.
TEST(SslModeChainValidation, a_service_file_ca_file_wins_over_the_environment_one)
{
    EnvCaFile envCaFile(EnvCaFile::MISSING);
    ASSERT_TRUE(envCaFile.ok()) << "no environment CA file path was settled";
    ServiceFileCaFile caFile("sslrootcert", "rsodbc_over_env_", EnvCaFile::READABLE);
    ASSERT_TRUE(caFile.ok()) << caFile.why();
    RUN_IN_CHILD_OR_CONTINUE(
        "SslModeChainValidation.a_service_file_ca_file_wins_over_the_environment_one");

    rsfake::FakeEndpoint endpoint("untrusted");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint, ";sslmode=require",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    EXPECT_NE(attempt.logText.find(std::string(CA_FILE_CHANNEL_MARKER) +
                                   CA_FILE_CHANNEL_SERVICE_FILE),
              std::string::npos)
        << "the service file must outrank PGSSLROOTCERT, as it does inside libpq. "
        << "The log named channel \"" << loggedCaFileChannel(attempt.logText) << "\".";
    EXPECT_EQ(attempt.logText.find(CA_FILE_DROPPED_ENV_WARNING), std::string::npos)
        << "the unreadable path PGSSLROOTCERT names must not have been the one "
        << "resolved. the log was:\n"
        << attempt.logText;
    EXPECT_EQ(attempt.diagnostics.find(rsfake::CA_FILE_UNREADABLE_ERROR), std::string::npos)
        << "the unreadable path PGSSLROOTCERT names must not have been the one "
        << "resolved. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_FALSE(SQL_SUCCEEDED(attempt.rc))
        << "require must validate against the service-file CA. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.diagnostics.find(rsfake::CHAIN_REJECTED_ERROR), std::string::npos)
        << "require must reject the untrusted certificate chain. diagnostics were:\n"
        << attempt.diagnostics;
}

/*====================================================================================================================================================*/

// An over-long path cannot reach the driver this way: libpq refuses service file lines of 255
// bytes or more, so PQconndefaults() returns nothing. libpq reads the file again when connecting
// and refuses it there.
TEST(SslModeChainValidation, an_over_long_service_file_line_fails_the_connection)
{
    ServiceFileSetting service("sslrootcert", longCaFilePath(LONG_PATH_LENGTH),
                               "rsodbc_long_");
    ASSERT_TRUE(service.ok()) << service.why();
    RUN_IN_CHILD_OR_CONTINUE(
        "SslModeChainValidation.an_over_long_service_file_line_fails_the_connection");

    rsfake::FakeEndpoint endpoint("matching");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "localhost", endpoint, ";sslmode=require",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    // No passwordReceived(): libpq refuses the connection before any socket opens.
    EXPECT_FALSE(SQL_SUCCEEDED(attempt.rc))
        << "a service file libpq cannot read must not leave require connecting "
        << "unvalidated. diagnostics were:\n"
        << attempt.diagnostics;
    EXPECT_NE(attempt.diagnostics.find("service file"), std::string::npos)
        << "the failure must be libpq's about the service file it could not read. "
        << "diagnostics were:\n"
        << attempt.diagnostics;
}

/*====================================================================================================================================================*/

// A service group naming no CA file leaves the user's default certificate file, which libpq would load.
// Without that file require has no CA file and validates nothing, and the service file must not be reported.
TEST(SslModeChainValidation, a_service_file_naming_no_ca_file_resolves_no_ca_file)
{
    SKIP_WITHOUT_A_CA_FILE_FREE_HOST();
    ServiceFileSetting service("connect_timeout", "30", "rsodbc_no_ca_file_");
    ASSERT_TRUE(service.ok()) << service.why();
    RUN_IN_CHILD_OR_CONTINUE(
        "SslModeChainValidation.a_service_file_naming_no_ca_file_resolves_no_ca_file");

    rsfake::FakeEndpoint endpoint("untrusted");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint, ";sslmode=require",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    EXPECT_EQ(attempt.logText.find(std::string(CA_FILE_CHANNEL_MARKER) +
                                   CA_FILE_CHANNEL_SERVICE_FILE),
              std::string::npos)
        << "a group that sets no CA file keyword must not be reported as the channel "
        << "that supplied one. The log named channel \""
        << loggedCaFileChannel(attempt.logText) << "\".";
    EXPECT_TRUE(SQL_SUCCEEDED(attempt.rc))
        << "require must behave as it does with no CA file named at all. diagnostics "
        << "were:\n"
        << attempt.diagnostics;
    EXPECT_EQ(attempt.diagnostics.find(rsfake::CHAIN_REJECTED_ERROR), std::string::npos)
        << "nothing must have been loaded to validate against. diagnostics were:\n"
        << attempt.diagnostics;
}

/*====================================================================================================================================================*/

// A service group with an empty sslrootcert= stops the search there, as in libpq, so an
// unusable PGSSLROOTCERT is never read and require validates nothing.
TEST(SslModeChainValidation, an_empty_service_file_ca_file_hides_the_environment_one)
{
    SKIP_WITHOUT_A_CA_FILE_FREE_HOST();
    EnvCaFile envCaFile(EnvCaFile::MISSING);
    ASSERT_TRUE(envCaFile.ok()) << "no environment CA file path was settled";
    ServiceFileSetting service("sslrootcert", "", "rsodbc_empty_root_cert_");
    ASSERT_TRUE(service.ok()) << service.why();
    RUN_IN_CHILD_OR_CONTINUE(
        "SslModeChainValidation.an_empty_service_file_ca_file_hides_the_environment_one");

    rsfake::FakeEndpoint endpoint("untrusted");
    ASSERT_TRUE(endpoint.started())
        << "the fake endpoint did not start: " << endpoint.startupOutput();

    rsfake::Attempt attempt = rsfake::connectTo(
        rsfake::DIRECT_DRIVER_CLAUSE, "127.0.0.1", endpoint, ";sslmode=require",
        rsfake::ENTRY_POINT_NATIVE, /*supplyCaFile=*/false);

    EXPECT_EQ(loggedCaFileChannel(attempt.logText), "none")
        << "the empty service value must leave no CA file. the log was:\n"
        << attempt.logText;
    EXPECT_EQ(attempt.logText.find(CA_FILE_DROPPED_ENV_WARNING), std::string::npos)
        << "PGSSLROOTCERT must not be read past the service file. the log was:\n"
        << attempt.logText;
    EXPECT_TRUE(SQL_SUCCEEDED(attempt.rc))
        << "require with no CA file must connect without validating. diagnostics were:\n"
        << attempt.diagnostics;
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
 * @param sslMode The sslmode to connect with.
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

TEST_F(SslRootCertificate, UnreadableDefaultRootsFailAVerifyMode) {
    // With an unreadable bundle and a missing sslrootcert, verify-ca has nothing to
    // verify against, so the connect fails instead of trusting any certificate.
    ScopedFile const bundle{rsfake::uniquePath("rsodbc_bad_bundle_") + ".crt"};
    {
        std::ofstream out{bundle.path.c_str()};
        out << "not a certificate\n";
    }
    std::string const missing{rsfake::uniquePath("rsodbc_missing_root_") +
                              ".crt"};
    HandshakeResult const result = serveAndConnect(
        m_server, m_serverCtx,
        "sslmode=verify-ca ssldefaultrootcert=" + conninfoPath(bundle.path) +
            " sslrootcert=" + conninfoPath(missing));
    EXPECT_FALSE(result.serverCompleted)
        << "connected without verifying the server";
}

TEST_F(SslRootCertificate, RequireVerifiesWithAUsableCa) {
    // Option 2 keeps validation for require when a usable CA is configured.
    // The unrelated caller CA and built-in root are loaded additively, so this
    // server chain must still be rejected.
    HandshakeResult const result = connectTrustingRoot(
        m_server, m_serverCtx, "require", m_otherCert.path());
    EXPECT_FALSE(result.serverCompleted)
        << "require accepted a server whose chain is not trusted by either CA: "
        << result.clientError;
    EXPECT_TRUE(chainWasRejected(result.clientError))
        << "require did not verify the server certificate: " << result.clientError;
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
