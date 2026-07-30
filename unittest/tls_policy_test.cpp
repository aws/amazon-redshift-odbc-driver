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
