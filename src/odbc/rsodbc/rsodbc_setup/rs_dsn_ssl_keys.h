/*-------------------------------------------------------------------------
*
* Copyright(c) 2026, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
*-------------------------------------------------------------------------
*/

/*
 * How the DSN setup dialog handles SSLMode and EncryptionMethod. Kept apart from
 * setup.c, which builds only on Windows, so the unit tests can cover it. Also holds
 * the auth plugin check the driver shares with the dialog.
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

/* The plugin name macros are the existing ones, so include this after them. */
#if !defined(PLUGIN_IDP_TOKEN_AUTH) || !defined(PLUGIN_BROWSER_IDC_AUTH)
#error "Define PLUGIN_IDP_TOKEN_AUTH and PLUGIN_BROWSER_IDC_AUTH before rs_dsn_ssl_keys.h"
#endif

#define RS_DSN_SSL_MODE_KEY           "SSLMode"
#define RS_DSN_ENCRYPTION_METHOD_KEY  "EncryptionMethod"

/*
 * The SSL modes the tab can show. Indices 0-3 are the EncryptionMethod codes; allow and
 * prefer (4-5) have none and are saved as SSLMode. An auth type is offered one run of
 * this table, and each combo entry carries its index as item data.
 */
static const char *const rs_dsn_em_modes[] = {
	"disable", "verify-ca", "verify-full", "require", "allow", "prefer"
};

#define RS_DSN_EM_MODE_COUNT ((int)(sizeof(rs_dsn_em_modes) / sizeof(rs_dsn_em_modes[0])))
/* The first table index with no matching EncryptionMethod code. */
#define RS_DSN_FIRST_SSL_MODE_ONLY_INDEX 4

/* The table index of "verify-ca", the weakest mode the "require" floor allows. */
#define RS_DSN_EM_INDEX_VERIFY_CA 1

/* The table index of "require", the fallback for auth types that enforce a
 * "require" floor. */
#define RS_DSN_EM_INDEX_REQUIRE 3

/** Returns the table entry at an index, or NULL when the index is out of range. */
static inline const char *
rs_dsn_em_mode_name(int index)
{
	if (index < 0 || index >= RS_DSN_EM_MODE_COUNT) {
		return NULL;
	}
	return rs_dsn_em_modes[index];
}

/**
 * Finds the table entry for an SSL mode, ignoring case.
 * @return The table index (the EncryptionMethod code for 0-3), or -1 if none.
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
 * Picks the table entry for the mode the DSN resolves to; a usable SSLMode outranks
 * EncryptionMethod, as in the driver.
 * @return The table index, or -1 to select nothing (an EncryptionMethod outside 0-3).
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
	/* The driver also rejects codes outside 0-3 and uses verify-full, so select nothing
	 * rather than show a weaker mode. */
	if (encryption_method >= 0 &&
		encryption_method < RS_DSN_FIRST_SSL_MODE_ONLY_INDEX) {
		return encryption_method;
	}
	return -1;
}

/** Tells whether the combo offers a table index: all of them, but only 1-3 under the "require" floor. */
static inline int
rs_dsn_em_index_is_offered(int index, int iam_floor)
{
	int first = iam_floor ? RS_DSN_EM_INDEX_VERIFY_CA : 0;
	int end = iam_floor ? RS_DSN_FIRST_SSL_MODE_ONLY_INDEX : RS_DSN_EM_MODE_COUNT;

	return index >= first && index < end;
}

/** Counts the table entries the combo offers: 3 under the "require" floor, else 6. */
static inline int
rs_dsn_em_offered_count(int iam_floor)
{
	int count = 0;
	int i;

	for (i = 0; i < RS_DSN_EM_MODE_COUNT; i++) {
		if (rs_dsn_em_index_is_offered(i, iam_floor)) {
			count++;
		}
	}
	return count;
}

/**
 * Picks the table entry for the mode the DSN resolves to, lifted to "require" when the
 * auth type withholds the stored mode (under the floor: disable, allow, prefer).
 * @return The table index to select, or -1 to select nothing.
 */
static inline int
rs_dsn_em_index_on_open_for_auth(int ssl_mode_named, const char *ssl_mode,
								 int encryption_method, int iam_floor)
{
	int index = rs_dsn_em_index_on_open(ssl_mode_named, ssl_mode, encryption_method);

	if (index >= 0 && !rs_dsn_em_index_is_offered(index, iam_floor)) {
		return RS_DSN_EM_INDEX_REQUIRE;
	}
	return index;
}

/**
 * Picks the selection after an auth type change rebuilt the combo. Imposing the floor
 * keeps the shown mode, lifted to "require" if withheld. Lifting it recomputes from the
 * DSN (the shown "require" may be an earlier lift's), so an unsaved selection is lost.
 * @param iam_floor Nonzero when the floor was just imposed, zero when lifted.
 */
static inline int
rs_dsn_em_index_on_auth_change(int em_index_shown, int ssl_mode_named,
							   const char *ssl_mode, int encryption_method,
							   int iam_floor)
{
	if (!iam_floor) {
		return rs_dsn_em_index_on_open_for_auth(ssl_mode_named, ssl_mode,
												encryption_method, iam_floor);
	}
	if (!rs_dsn_em_index_is_offered(em_index_shown, iam_floor)) {
		return RS_DSN_EM_INDEX_REQUIRE;
	}
	return em_index_shown;
}

/**
 * Tells whether saving must rewrite SSLMode so the DSN keeps the mode the dialog showed:
 * when the selection moved, or when the stored mode differs from the one shown (a mode
 * the combo withheld and rs_dsn_em_index_on_open_for_auth() lifted). Nothing selected
 * (em_index < 0) never writes.
 * @return Nonzero if SSLMode must be written, with the EncryptionMethod derived from it.
 */
static inline int
rs_dsn_em_should_write_ssl_mode(int em_index, int em_index_on_open,
								int ssl_mode_named, const char *ssl_mode)
{
	int stored;

	if (em_index < 0) {
		return 0;
	}
	if (em_index != em_index_on_open) {
		return 1;
	}
	stored = rs_dsn_em_index_for_mode(ssl_mode);
	/* The selection has not moved, so a different stored mode was withheld by the combo.
	 * Asking the current auth type would not match the one the combo was built for. */
	return ssl_mode_named && stored >= 0 && stored != em_index;
}

/**
 * Tells whether a plugin name reaches the IAM path without the IAM flag (the two IdC
 * plugins). Compared without case, as the driver does.
 * @return Nonzero for such a plugin; zero for any other name, NULL or empty.
 */
static inline int
rs_auth_plugin_reaches_iam_path(const char *plugin_name)
{
	if (plugin_name == NULL || *plugin_name == '\0') {
		return 0;
	}
	return _stricmp(plugin_name, PLUGIN_IDP_TOKEN_AUTH) == 0 ||
		_stricmp(plugin_name, PLUGIN_BROWSER_IDC_AUTH) == 0;
}

/**
 * Tells whether a DSN's stored attributes can reach the IAM path, and so need the "require"
 * floor whatever auth type is shown. The IAM flag (spelled true as the driver reads it), an
 * IdC plugin_name, or an AuthProfile do, since a fetched profile can set IAM; a leftover
 * Profile or access key does not. The live tab does not ask this: it uses the auth type the
 * Connect tab shows now.
 */
static inline int
rs_dsn_auth_may_be_iam(const char *iam, const char *plugin_name, const char *auth_profile)
{
	if (iam != NULL && (_stricmp(iam, "1") == 0 || _stricmp(iam, "TRUE") == 0)) {
		return 1;
	}
	if (auth_profile != NULL && auth_profile[0] != '\0') {
		return 1;
	}
	return rs_auth_plugin_reaches_iam_path(plugin_name);
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
