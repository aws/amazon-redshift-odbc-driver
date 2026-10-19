/*
 * Copyright (c) Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * tls_policy_test.cpp
 *
 * Unit tests for the TLS policy helpers in pq_tls_policy.h and
 * verification that the linked crypto library (AWS-LC) supports
 * the post-quantum key exchange groups the driver advertises.
 */

// Include first: it pulls in winsock2.h, which must precede windows.h on Windows.
#include "fake_redshift_endpoint.hpp"

#include "common.h"
#include "rsutil.h"

#include <algorithm>
#include <cctype>
#include <string>

extern "C" {
#include "pq_tls_policy.h"
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
