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
