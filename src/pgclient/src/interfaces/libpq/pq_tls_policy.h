/*
 * Copyright (c) Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * pq_tls_policy.h
 *
 * Helpers for the post-quantum key-exchange policy the driver enforces
 * on Redshift-endpoint connections.
 *
 * Defined as static inline so they are unit-testable from a C++ gtest
 * translation unit without exposing new symbols from libpq.
 */

#ifndef PQ_TLS_POLICY_H
#define PQ_TLS_POLICY_H

#include <string.h>

/**
 * @brief Returns whether the driver should advertise post-quantum hybrid
 *        key-exchange groups in the TLS 1.3 ClientHello.
 *
 * @param user_value  Value of the prefer_pq connection property.
 *                    NULL or empty string → default (on).
 *                    "0", "false", "False", "FALSE" → off.
 *                    Any other value (including typos) → on,
 *                    biasing toward PQ.
 * @return 1 if PQ groups should be advertised, 0 otherwise.
 */
static inline int
pq_should_prefer_pq(const char *user_value)
{
    if (user_value == NULL || user_value[0] == '\0')
        return 1;
    if (strcmp(user_value, "0") == 0 ||
        strcmp(user_value, "false") == 0 ||
        strcmp(user_value, "False") == 0 ||
        strcmp(user_value, "FALSE") == 0)
        return 0;
    return 1;
}

/**
 * @brief Minimum TLS protocol version the driver should negotiate.
 *
 * Kept independent of the crypto library's TLS1_x_VERSION constants so
 * this header stays openssl-free and unit-testable. The caller maps
 * these to the library's protocol constants.
 */
typedef enum
{
    RS_MIN_TLS_1_1,
    RS_MIN_TLS_1_2,
    RS_MIN_TLS_1_3
} rs_min_tls_version;

/**
 * @brief Resolve the effective minimum TLS version from the min_tls
 *        connection property.
 *
 * @param user_value  Value of the min_tls connection property.
 *                    NULL → default (1.2).
 *                    "1.1" / "1.2" / "1.3" → the matching version.
 *                    Any other value (invalid input) → 1.2, failing
 *                    secure rather than weakening the floor.
 * @return the resolved rs_min_tls_version.
 */
static inline rs_min_tls_version
pq_resolve_min_tls(const char *user_value)
{
    if (user_value == NULL)
        return RS_MIN_TLS_1_2;
    if (strcmp(user_value, "1.3") == 0)
        return RS_MIN_TLS_1_3;
    if (strcmp(user_value, "1.2") == 0)
        return RS_MIN_TLS_1_2;
    if (strcmp(user_value, "1.1") == 0)
        return RS_MIN_TLS_1_1;
    /* Invalid value: fail secure to TLS 1.2. */
    return RS_MIN_TLS_1_2;
}

/*
 * TLS 1.3 named-groups string offered in the ClientHello when
 * prefer_pq is on. Format is the colon-separated string accepted by
 * SSL_set1_groups_list (AWS-LC / OpenSSL 3.0+).
 *
 * PQ hybrid groups first (X25519MLKEM768 is the industry-default
 * hybrid -- same group Chrome, Cloudflare, and AWS-LC settled on),
 * classical fallback second for peers that do not support PQ groups.
 */
#define PQ_HYBRID_GROUPS_LIST \
    "X25519MLKEM768:SecP256r1MLKEM768:X25519:P-256:P-384"

#define PQ_CLASSICAL_GROUPS_LIST \
    "X25519:P-256:P-384"

#endif /* PQ_TLS_POLICY_H */
