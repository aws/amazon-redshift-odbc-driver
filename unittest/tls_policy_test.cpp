/*
 * Copyright (c) Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * tls_policy_test.cpp
 *
 * Unit tests for the TLS policy helpers in pq_tls_policy.h and
 * verification that the linked crypto library (AWS-LC) supports
 * the post-quantum key exchange groups the driver advertises.
 */

#include "common.h"

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
