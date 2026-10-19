/*-------------------------------------------------------------------------
*
* Copyright(c) 2026, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
*-------------------------------------------------------------------------
*/

/*
 * How the DSN setup dialog handles SSLMode and EncryptionMethod. Kept apart from
 * setup.c, which builds only on Windows, so the unit tests can cover it.
 */
#ifndef RS_DSN_SSL_KEYS_H
#define RS_DSN_SSL_KEYS_H

#include <string.h>
#ifndef WIN32
#include <strings.h>
#ifndef _stricmp
#define _stricmp strcasecmp
#endif
#endif

#define RS_DSN_SSL_MODE_KEY           "SSLMode"
#define RS_DSN_ENCRYPTION_METHOD_KEY  "EncryptionMethod"

/* SSL tab combo entries, in EncryptionMethod order: the index is the value. */
static const char *const rs_dsn_em_modes[] = {
	"disable", "verify-ca", "verify-full", "require"
};

/**
 * @brief Finds the SSL tab combo entry for an SSL mode, ignoring case.
 * @param ssl_mode The SSL mode; may be NULL.
 * @return The combo index, which is also the EncryptionMethod code, or -1 when the
 *         combo has no entry for ssl_mode.
 */
static inline int
rs_dsn_em_index_for_mode(const char *ssl_mode)
{
	int i;

	if (ssl_mode == NULL)
		return -1;
	for (i = 0; i < (int)(sizeof(rs_dsn_em_modes) / sizeof(rs_dsn_em_modes[0])); i++) {
		if (_stricmp(ssl_mode, rs_dsn_em_modes[i]) == 0)
			return i;
	}
	return -1;
}

/**
 * @brief Picks the SSL tab combo entry for the mode the DSN resolves to. As in the
 *        driver, a usable SSLMode outranks EncryptionMethod.
 * @param ssl_mode_named Nonzero if the DSN has an SSLMode key.
 * @param ssl_mode The DSN's SSLMode value.
 * @param encryption_method The DSN's EncryptionMethod, or its table default.
 * @return The combo index to select.
 */
static inline int
rs_dsn_em_index_on_open(int ssl_mode_named, const char *ssl_mode,
						int encryption_method)
{
	if (ssl_mode_named) {
		int index = rs_dsn_em_index_for_mode(ssl_mode);

		if (index >= 0)
			return index;
	}
	return encryption_method;
}

/**
 * @brief Checks whether a DSN attribute may be saved or sent by Test Connection.
 *        An SSL key goes out only once named, so a save never adds one the DSN
 *        lacked or pins the driver's default.
 * @param attr The attribute name.
 * @param ssl_mode_named Nonzero if the DSN or the user named SSLMode.
 * @param encryption_method_named Nonzero if the DSN or the user named EncryptionMethod.
 * @return Nonzero if attr may be written; always nonzero for non-SSL attributes.
 */
static inline int
rs_dsn_ssl_key_is_named(const char *attr, int ssl_mode_named,
						int encryption_method_named)
{
	if (_stricmp(attr, RS_DSN_SSL_MODE_KEY) == 0)
		return ssl_mode_named;
	if (_stricmp(attr, RS_DSN_ENCRYPTION_METHOD_KEY) == 0)
		return encryption_method_named;
	return 1;
}

#endif /* RS_DSN_SSL_KEYS_H */
