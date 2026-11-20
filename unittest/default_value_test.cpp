#include "common.h"
#include <rsdesc.h>
#include <rsodbc.h>
#include <rsutil.h>
#include "rsodbc_setup/rs_dsn_ssl_keys.h"
#include <sql.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <new>
#include <optional>
#include <string>
#include <tuple>
#include <vector>
#ifndef WIN32
#include <unistd.h>
#endif
#include "pq_tls_policy.h"

// Unit tests for connection property default values and parsing.
// This unit test is for testing the default value when databaseMetadataCurrentDbOnly flag is not included in DSN.

TEST(DEFAULT_VALUE_TEST_SUITE, test_DatabaseMetadataCurrentDbOnly) {

    RS_CONNECT_PROPS_INFO obj;
    EXPECT_TRUE(obj.iDatabaseMetadataCurrentDbOnly == 1);

}

TEST(DEFAULT_VALUE_TEST_SUITE, test_BoolsAsChar) {

    RS_CONNECT_PROPS_INFO obj;
    EXPECT_TRUE(obj.iBoolsAsChar == 0);

}

TEST(DEFAULT_VALUE_TEST_SUITE, test_UseDeclareFetch) {

    RS_CONNECT_PROPS_INFO obj;
    EXPECT_TRUE(obj.iUseDeclareFetch == 0);

}

TEST(DEFAULT_VALUE_TEST_SUITE, test_FetchSize) {

    RS_CONNECT_PROPS_INFO obj;
    EXPECT_TRUE(obj.iFetchSize == 0);

}

// ===========================================================================
// Key define correctness
// ===========================================================================

TEST(DEFAULT_VALUE_TEST_SUITE, test_RS_USE_DECLARE_FETCH_key) {
    EXPECT_STREQ(RS_USE_DECLARE_FETCH, "UseDeclareFetch");
}

TEST(DEFAULT_VALUE_TEST_SUITE, test_RS_FETCH_SIZE_key) {
    // RS_FETCH_SIZE is the connection property key name "Fetch"
    // (the parameter name customers use in connection strings and DSN entries).
    EXPECT_STREQ(RS_FETCH_SIZE, "Fetch");
}

TEST(DEFAULT_VALUE_TEST_SUITE, test_RS_DEFAULT_FETCH_SIZE_value) {
    EXPECT_EQ(RS_DEFAULT_FETCH_SIZE, 100);
}

// szSslMode and iEncryptionMethod must agree, or DSN and DSN-less connects get
// different defaults.

TEST(DEFAULT_VALUE_TEST_SUITE, test_SslMode_default_is_verify_full) {
    RS_CONNECT_PROPS_INFO obj;
    EXPECT_STREQ(obj.szSslMode, "verify-full")
        << "The default SSL mode must verify the server's hostname";
}

TEST(DEFAULT_VALUE_TEST_SUITE, test_EncryptionMethod_default_matches_SslMode) {
    RS_CONNECT_PROPS_INFO obj;
    // libpqConnect maps 0=disable, 1=verify-ca, 2=verify-full, 3=require.
    EXPECT_EQ(obj.iEncryptionMethod, 2)
        << "iEncryptionMethod must re-derive the same mode szSslMode carries, "
           "or the default differs between the DSN and DSN-less paths";
}

TEST(DEFAULT_VALUE_TEST_SUITE, test_RS_SSL_MODE_key) {
    EXPECT_STREQ(RS_SSL_MODE, "SSLMode");
}

// ===========================================================================
// SSL tab combo index <-> mode mapping (rs_dsn_ssl_keys.h)
// ===========================================================================

// The four EncryptionMethod modes keep their index == EncryptionMethod code,
// and allow/prefer are appended at 4 and 5.
TEST(DsnSslComboTest, index_for_mode_covers_all_six_modes) {
    EXPECT_EQ(rs_dsn_em_index_for_mode("disable"), 0);
    EXPECT_EQ(rs_dsn_em_index_for_mode("verify-ca"), 1);
    EXPECT_EQ(rs_dsn_em_index_for_mode("verify-full"), 2);
    EXPECT_EQ(rs_dsn_em_index_for_mode("require"), 3);
    EXPECT_EQ(rs_dsn_em_index_for_mode("allow"), 4);
    EXPECT_EQ(rs_dsn_em_index_for_mode("prefer"), 5);
}

TEST(DsnSslComboTest, index_for_mode_ignores_case_and_rejects_unknown) {
    EXPECT_EQ(rs_dsn_em_index_for_mode("ALLOW"), 4);
    EXPECT_EQ(rs_dsn_em_index_for_mode("Prefer"), 5);
    EXPECT_EQ(rs_dsn_em_index_for_mode("nonsense"), -1);
    EXPECT_EQ(rs_dsn_em_index_for_mode(NULL), -1);
}

// Only allow/prefer (4, 5) have no EncryptionMethod code; 0-3 do.
TEST(DsnSslComboTest, only_allow_and_prefer_have_no_encryption_method_code) {
    EXPECT_LT(rs_dsn_em_index_for_mode("disable"), RS_DSN_FIRST_SSL_MODE_ONLY_INDEX);
    EXPECT_LT(rs_dsn_em_index_for_mode("require"), RS_DSN_FIRST_SSL_MODE_ONLY_INDEX);
    EXPECT_GE(rs_dsn_em_index_for_mode("allow"), RS_DSN_FIRST_SSL_MODE_ONLY_INDEX);
    EXPECT_GE(rs_dsn_em_index_for_mode("prefer"), RS_DSN_FIRST_SSL_MODE_ONLY_INDEX);
}

// The name accessor reads the table by index and returns NULL out of range.
TEST(DsnSslComboTest, mode_name_is_bounds_checked) {
    EXPECT_STREQ(rs_dsn_em_mode_name(0), "disable");
    EXPECT_STREQ(rs_dsn_em_mode_name(3), "require");
    EXPECT_STREQ(rs_dsn_em_mode_name(5), "prefer");
    EXPECT_EQ(rs_dsn_em_mode_name(-1), nullptr);
    EXPECT_EQ(rs_dsn_em_mode_name(6), nullptr);
    EXPECT_EQ(rs_dsn_em_mode_name(1000), nullptr);
}

// A DSN that saved SSLMode=allow/prefer reopens on that combo entry.
TEST(DsnSslComboTest, on_open_selects_allow_or_prefer_from_a_named_sslmode) {
    EXPECT_EQ(rs_dsn_em_index_on_open(/*ssl_mode_named=*/1, "allow", /*em=*/2), 4);
    EXPECT_EQ(rs_dsn_em_index_on_open(/*ssl_mode_named=*/1, "prefer", /*em=*/2), 5);
}

// A legacy DSN with only EncryptionMethod still opens on its numeric mode.
TEST(DsnSslComboTest, on_open_falls_back_to_encryption_method_when_unnamed) {
    EXPECT_EQ(rs_dsn_em_index_on_open(/*ssl_mode_named=*/0, NULL, /*em=*/3), 3);
    EXPECT_EQ(rs_dsn_em_index_on_open(/*ssl_mode_named=*/0, NULL, /*em=*/1), 1);
    // EncryptionMethod cannot carry allow or prefer, and the driver falls back to verify-full
    // for 4 and 5, so they select nothing rather than a weaker mode.
    EXPECT_EQ(rs_dsn_em_index_on_open(/*ssl_mode_named=*/0, NULL, /*em=*/4), -1);
    EXPECT_EQ(rs_dsn_em_index_on_open(/*ssl_mode_named=*/0, NULL, /*em=*/5), -1);
    EXPECT_EQ(rs_dsn_em_index_on_open(/*ssl_mode_named=*/0, NULL, /*em=*/-1), -1);
}

// IAM, IdC and native IdP auth types accept only require, verify-ca and verify-full, so the combo
// withholds disable, allow and prefer; saving one would leave a DSN whose every connect fails.
TEST(DsnSslComboTest, the_floor_offers_only_the_three_accepted_modes) {
    EXPECT_EQ(rs_dsn_em_offered_count(/*iam_floor=*/1), 3);
    EXPECT_FALSE(rs_dsn_em_index_is_offered(rs_dsn_em_index_for_mode("disable"),
                                            /*iam_floor=*/1));
    EXPECT_TRUE(rs_dsn_em_index_is_offered(rs_dsn_em_index_for_mode("verify-ca"),
                                           /*iam_floor=*/1));
    EXPECT_TRUE(rs_dsn_em_index_is_offered(rs_dsn_em_index_for_mode("verify-full"),
                                           /*iam_floor=*/1));
    EXPECT_TRUE(rs_dsn_em_index_is_offered(rs_dsn_em_index_for_mode("require"),
                                           /*iam_floor=*/1));
    EXPECT_FALSE(rs_dsn_em_index_is_offered(rs_dsn_em_index_for_mode("allow"),
                                            /*iam_floor=*/1));
    EXPECT_FALSE(rs_dsn_em_index_is_offered(rs_dsn_em_index_for_mode("prefer"),
                                            /*iam_floor=*/1));
}

// Every auth type that does not reach the IAM path is offered all six modes,
// disable included, starting at the top of the table.
TEST(DsnSslComboTest, without_the_floor_every_mode_is_offered) {
    EXPECT_EQ(rs_dsn_em_offered_count(/*iam_floor=*/0), 6);
    for (int i = 0; i < 6; i++) {
        EXPECT_TRUE(rs_dsn_em_index_is_offered(i, /*iam_floor=*/0)) << i;
    }
    EXPECT_STREQ(rs_dsn_em_modes[0], "disable");
    // Entry 0 is still EncryptionMethod 0, so a DSN that names only that code
    // opens on disable rather than on something stronger.
    EXPECT_EQ(rs_dsn_em_index_for_mode("disable"), 0);
    EXPECT_EQ(rs_dsn_em_index_on_open(/*ssl_mode_named=*/0, NULL, /*em=*/0), 0);
}

// Nothing selected is not an offered entry, and nothing past the table is either.
TEST(DsnSslComboTest, no_selection_and_out_of_range_indexes_are_not_offered) {
    for (int iam_floor = 0; iam_floor <= 1; iam_floor++) {
        EXPECT_FALSE(rs_dsn_em_index_is_offered(-1, iam_floor)) << iam_floor;
        EXPECT_FALSE(rs_dsn_em_index_is_offered(6, iam_floor)) << iam_floor;
    }
}

// A DSN that stored one of the withheld modes opens on require once an auth type
// that cannot use it is selected.
TEST(DsnSslComboTest, on_open_for_auth_coerces_withheld_modes_to_require_for_iam) {
    EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, "allow",
                                               /*em=*/2, /*iam_floor=*/1), 3);
    EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, "prefer",
                                               /*em=*/2, /*iam_floor=*/1), 3);
    // A DSN an older release let save disable next to an IAM auth type. The combo
    // no longer offers it, so the tab opens on require.
    EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, "disable",
                                               /*em=*/2, /*iam_floor=*/1), 3);
    EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, "DISABLE",
                                               /*em=*/2, /*iam_floor=*/1), 3);
    // An EncryptionMethod-only DSN carrying the disable code the same way.
    EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/0, NULL,
                                               /*em=*/0, /*iam_floor=*/1), 3);
    // Without the floor each of them opens where it is stored.
    EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, "allow",
                                               /*em=*/2, /*iam_floor=*/0), 4);
    EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, "prefer",
                                               /*em=*/2, /*iam_floor=*/0), 5);
    EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, "disable",
                                               /*em=*/2, /*iam_floor=*/0), 0);
}

// Every mode the floor still offers selects the same entry it would without the
// floor, and so does an EncryptionMethod-only or out-of-range DSN.
TEST(DsnSslComboTest, on_open_for_auth_leaves_the_other_modes_alone) {
    for (int iam_floor = 0; iam_floor <= 1; iam_floor++) {
        EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, "verify-ca",
                                                   /*em=*/2, iam_floor), 1)
            << "iam_floor=" << iam_floor;
        EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, "verify-full",
                                                   /*em=*/0, iam_floor), 2)
            << "iam_floor=" << iam_floor;
        EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, "require",
                                                   /*em=*/0, iam_floor), 3)
            << "iam_floor=" << iam_floor;
        // EncryptionMethod-only DSNs still open on their numeric mode.
        EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/0, NULL,
                                                   /*em=*/1, iam_floor), 1)
            << "iam_floor=" << iam_floor;
        EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/0, NULL,
                                                   /*em=*/3, iam_floor), 3)
            << "iam_floor=" << iam_floor;
        // An EncryptionMethod outside 0-3 still selects nothing.
        EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/0, NULL,
                                                   /*em=*/4, iam_floor), -1)
            << "iam_floor=" << iam_floor;
        EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, "nonsense",
                                                   /*em=*/-1, iam_floor), -1)
            << "iam_floor=" << iam_floor;
    }
}

// Each offered index is still the EncryptionMethod code for entries that have one.
TEST(DsnSslComboTest, the_offered_run_keeps_the_encryption_method_codes) {
    for (int iam_floor = 0; iam_floor <= 1; iam_floor++) {
        for (int index = 0; index < 6; index++) {
            if (!rs_dsn_em_index_is_offered(index, iam_floor)) {
                continue;
            }
            // The entry the combo shows at this index round-trips to it.
            EXPECT_EQ(rs_dsn_em_index_for_mode(rs_dsn_em_mode_name(index)), index)
                << "iam_floor=" << iam_floor << " index " << index;
            // And for the modes EncryptionMethod can carry, that index is the code
            // the driver re-derives the same mode from.
            if (index < RS_DSN_FIRST_SSL_MODE_ONLY_INDEX) {
                EXPECT_EQ(rs_dsn_em_index_on_open(/*ssl_mode_named=*/0, NULL, index),
                          index)
                    << "iam_floor=" << iam_floor << " index " << index;
            }
        }
    }
    EXPECT_STREQ(rs_dsn_em_modes[RS_DSN_EM_INDEX_REQUIRE], "require");
    EXPECT_STREQ(rs_dsn_em_modes[RS_DSN_EM_INDEX_VERIFY_CA], "verify-ca");
    // Under the floor the run stops exactly where the SSLMode-only modes begin.
    EXPECT_TRUE(rs_dsn_em_index_is_offered(RS_DSN_FIRST_SSL_MODE_ONLY_INDEX - 1,
                                           /*iam_floor=*/1));
    EXPECT_FALSE(rs_dsn_em_index_is_offered(RS_DSN_FIRST_SSL_MODE_ONLY_INDEX,
                                            /*iam_floor=*/1));
}

/*====================================================================================================================================================*/

namespace {

// The item data rs_dsn_fill_ssl_mode_combo stores on each entry it adds: one table
// index per entry, in the order the entries are added.
std::vector<int> comboItemData(int iam_floor) {
    std::vector<int> data;

    for (int i = 0; i < RS_DSN_EM_MODE_COUNT; i++) {
        if (rs_dsn_em_index_is_offered(i, iam_floor)) {
            data.push_back(i);
        }
    }
    return data;
}

} // namespace

// Under the floor the combo offers only verify-ca, verify-full and require; unfloored it offers all
// six. Asserted on the item data the save path reads.
TEST(DsnSslComboTest, the_combo_offers_the_right_entries_in_each_state) {
    EXPECT_EQ(comboItemData(/*iam_floor=*/1), std::vector<int>({1, 2, 3}));
    EXPECT_EQ(comboItemData(/*iam_floor=*/0), std::vector<int>({0, 1, 2, 3, 4, 5}));

    for (const char *mode : {"disable", "allow", "prefer"}) {
        const int em_index = rs_dsn_em_index_for_mode(mode);

        EXPECT_FALSE(rs_dsn_em_index_is_offered(em_index, /*iam_floor=*/1))
            << mode << " must not be offered under the floor";
        EXPECT_TRUE(rs_dsn_em_index_is_offered(em_index, /*iam_floor=*/0))
            << mode << " must be offered without it";
    }
}

// The floor must work the stored mode out as the driver does: a DSN whose mode comes only from
// EncryptionMethod would otherwise slip past it by reading SSLMode's verify-full default.
TEST(DsnSslComboTest, the_floor_sees_a_mode_that_comes_only_from_encryption_method) {
    // What the setup attribute table hands back for a DSN with no SSLMode key.
    const char *const table_default = "verify-full";
    const int iam_floor = 1;

    // EncryptionMethod=0 is disable, which the IAM path refuses.
    const int resolved = rs_dsn_em_index_on_open(/*ssl_mode_named=*/0, table_default,
                                                 /*em=*/0);
    EXPECT_EQ(resolved, 0);
    EXPECT_FALSE(rs_dsn_em_index_is_offered(resolved, iam_floor));

    // Reading SSLMode on its own sees the table default and finds nothing to lift.
    EXPECT_TRUE(rs_dsn_em_index_is_offered(rs_dsn_em_index_for_mode(table_default),
                                           iam_floor));

    // Every EncryptionMethod code the IAM path refuses, and the ones it accepts.
    EXPECT_FALSE(rs_dsn_em_index_is_offered(
        rs_dsn_em_index_on_open(/*ssl_mode_named=*/0, table_default, /*em=*/0),
        iam_floor));
    for (int em = 1; em <= 3; em++) {
        EXPECT_TRUE(rs_dsn_em_index_is_offered(
            rs_dsn_em_index_on_open(/*ssl_mode_named=*/0, table_default, em),
            iam_floor))
            << "EncryptionMethod=" << em;
    }

    // Lifting writes require into both keys, and require's table index is the
    // EncryptionMethod code for it, so the two cannot end up disagreeing.
    EXPECT_STREQ(rs_dsn_em_modes[RS_DSN_EM_INDEX_REQUIRE], "require");
    EXPECT_EQ(rs_dsn_em_index_on_open(/*ssl_mode_named=*/0, NULL,
                                      RS_DSN_EM_INDEX_REQUIRE),
              RS_DSN_EM_INDEX_REQUIRE);
    EXPECT_EQ(rs_dsn_em_index_on_open(/*ssl_mode_named=*/1, "require",
                                      RS_DSN_EM_INDEX_REQUIRE),
              RS_DSN_EM_INDEX_REQUIRE);
}

// Moving the combo is still written, in either direction, whatever the DSN stores.
TEST(DsnSslComboTest, should_write_ssl_mode_on_a_moved_selection) {
    // Tightened by hand: verify-full picked over a stored require.
    EXPECT_TRUE(rs_dsn_em_should_write_ssl_mode(/*em_index=*/2, /*em_index_on_open=*/3,
                                                /*ssl_mode_named=*/1, "require"));
    // Loosened by hand under an auth type that offers allow: still the user's call.
    EXPECT_TRUE(rs_dsn_em_should_write_ssl_mode(/*em_index=*/4, /*em_index_on_open=*/3,
                                                /*ssl_mode_named=*/1, "require"));
    // The Standard -> IAM direction: the rebuild's fallback to require is a move.
    EXPECT_TRUE(rs_dsn_em_should_write_ssl_mode(/*em_index=*/3, /*em_index_on_open=*/4,
                                                /*ssl_mode_named=*/1, "allow"));
    // An EncryptionMethod-only DSN has no stored SSLMode to compare against.
    EXPECT_TRUE(rs_dsn_em_should_write_ssl_mode(/*em_index=*/3, /*em_index_on_open=*/1,
                                                /*ssl_mode_named=*/0, NULL));
}

// Opening the SSL tab of a DSN whose stored mode is the one on screen must not
// write anything, which is what keeps a save from pinning the driver's default.
TEST(DsnSslComboTest, should_not_write_ssl_mode_for_an_untouched_tab) {
    for (int i = 0; i < 6; i++) {
        EXPECT_FALSE(rs_dsn_em_should_write_ssl_mode(/*em_index=*/i,
                                                     /*em_index_on_open=*/i,
                                                     /*ssl_mode_named=*/1,
                                                     rs_dsn_em_modes[i]))
            << "untouched entry " << i << " (" << rs_dsn_em_modes[i] << ")";
    }
    // Same for a DSN that named no SSLMode at all, and for one whose SSLMode the
    // combo has no entry for - there is no withheld mode to reconcile either way.
    EXPECT_FALSE(rs_dsn_em_should_write_ssl_mode(/*em_index=*/3, /*em_index_on_open=*/3,
                                                 /*ssl_mode_named=*/0, NULL));
    EXPECT_FALSE(rs_dsn_em_should_write_ssl_mode(/*em_index=*/3, /*em_index_on_open=*/3,
                                                 /*ssl_mode_named=*/1, "nonsense"));
}

// The reported sequence: a DSN storing allow or prefer opens under an IAM auth type showing require;
// the user switches to Standard and back, so the combo is rebuilt and re-selects the shown require.
// The index equals em_index_on_open but the DSN holds the weaker mode, so the save must rewrite SSLMode.
TEST(DsnSslComboTest, should_write_ssl_mode_when_the_auth_type_coerced_the_shown_mode) {
    const int shown = rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, "allow",
                                                       /*em=*/2, /*iam_floor=*/1);
    EXPECT_EQ(shown, RS_DSN_EM_INDEX_REQUIRE);
    // On open under the IAM auth type, before any tab switch.
    EXPECT_TRUE(rs_dsn_em_should_write_ssl_mode(shown, /*em_index_on_open=*/shown,
                                                /*ssl_mode_named=*/1, "allow"));
    // And after the rebuild re-selected that same shown entry.
    EXPECT_TRUE(rs_dsn_em_should_write_ssl_mode(/*em_index=*/RS_DSN_EM_INDEX_REQUIRE,
                                                /*em_index_on_open=*/RS_DSN_EM_INDEX_REQUIRE,
                                                /*ssl_mode_named=*/1, "allow"));
    EXPECT_TRUE(rs_dsn_em_should_write_ssl_mode(/*em_index=*/RS_DSN_EM_INDEX_REQUIRE,
                                                /*em_index_on_open=*/RS_DSN_EM_INDEX_REQUIRE,
                                                /*ssl_mode_named=*/1, "Prefer"));
    // The coercion only tightens, and every entry that triggers the rewrite has an EncryptionMethod
    // code (allow and prefer, which have none, are not coerced).
    EXPECT_LT(RS_DSN_EM_INDEX_REQUIRE, RS_DSN_FIRST_SSL_MODE_ONLY_INDEX);
    // A stored allow left showing as allow is no coercion: the auth type offers
    // it, so an untouched tab must still leave the DSN alone.
    EXPECT_FALSE(rs_dsn_em_should_write_ssl_mode(/*em_index=*/4, /*em_index_on_open=*/4,
                                                 /*ssl_mode_named=*/1, "allow"));
}

// A DSN an older release left holding disable beside an auth type that rejects it.
// The tab opens on require, so an untouched save must persist require.
TEST(DsnSslComboTest, a_stored_disable_persists_require_under_the_floor) {
    const int shown = rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, "disable",
                                                       /*em=*/0, /*iam_floor=*/1);

    EXPECT_EQ(shown, RS_DSN_EM_INDEX_REQUIRE);
    EXPECT_STREQ(rs_dsn_em_modes[shown], "require");
    EXPECT_TRUE(rs_dsn_em_should_write_ssl_mode(shown, /*em_index_on_open=*/shown,
                                                /*ssl_mode_named=*/1, "disable"));
}

// The same stored mode on a Standard DSN. disable is offered there, so the tab
// opens on disable and an untouched save leaves the DSN exactly as it was.
TEST(DsnSslComboTest, a_stored_disable_is_kept_on_a_standard_dsn) {
    const int shown = rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, "disable",
                                                       /*em=*/0, /*iam_floor=*/0);

    EXPECT_EQ(shown, 0);
    EXPECT_STREQ(rs_dsn_em_modes[shown], "disable");
    EXPECT_FALSE(rs_dsn_em_should_write_ssl_mode(shown, /*em_index_on_open=*/shown,
                                                 /*ssl_mode_named=*/1, "disable"));
}

namespace {

// The SSLMode rs_dsn_read_ssl_tab leaves stored for a selection: the combo's mode only when
// rs_dsn_em_should_write_ssl_mode() says so and the index names a table entry.
std::string savedSslMode(int em_index, int em_index_on_open, int ssl_mode_named,
                         const char *ssl_mode) {
    const char *stored = ssl_mode == NULL ? "" : ssl_mode;

    if (!rs_dsn_em_should_write_ssl_mode(em_index, em_index_on_open, ssl_mode_named,
                                        ssl_mode)) {
        return stored;
    }
    const char *name = rs_dsn_em_mode_name(em_index);
    return name == NULL ? stored : name;
}

} // namespace

// A combo with nothing selected never writes, whatever it showed on open or the DSN stores.
TEST(DsnSslComboTest, a_combo_with_nothing_selected_never_writes) {
    for (int on_open = -1; on_open <= 5; on_open++) {
        EXPECT_FALSE(rs_dsn_em_should_write_ssl_mode(/*em_index=*/-1, on_open,
                                                     /*ssl_mode_named=*/1, "allow"))
            << "on_open=" << on_open;
        EXPECT_FALSE(rs_dsn_em_should_write_ssl_mode(/*em_index=*/-1, on_open,
                                                     /*ssl_mode_named=*/0, NULL))
            << "on_open=" << on_open;
    }
    // So the stored SSLMode is left as it was, and so is one for an index outside the table.
    EXPECT_EQ(savedSslMode(/*em_index=*/-1, /*em_index_on_open=*/3,
                           /*ssl_mode_named=*/1, "allow"), "allow");
    EXPECT_EQ(savedSslMode(/*em_index=*/6, /*em_index_on_open=*/3,
                           /*ssl_mode_named=*/1, "allow"), "allow");
}

// Selecting an auth type that imposes the floor rebuilds the combo with three entries. A mode they
// still offer stays; the other three are lifted to require.
TEST(DsnSslComboTest, imposing_the_floor_keeps_a_mode_it_still_offers) {
    for (int shown = RS_DSN_EM_INDEX_VERIFY_CA; shown <= RS_DSN_EM_INDEX_REQUIRE;
         shown++) {
        EXPECT_EQ(rs_dsn_em_index_on_auth_change(shown, /*ssl_mode_named=*/1, "allow",
                                                 /*em=*/2, /*iam_floor=*/1),
                  shown)
            << rs_dsn_em_modes[shown] << " is offered under the floor";
    }
    for (const char *mode : {"disable", "allow", "prefer"}) {
        EXPECT_EQ(rs_dsn_em_index_on_auth_change(rs_dsn_em_index_for_mode(mode),
                                                 /*ssl_mode_named=*/1, mode,
                                                 /*em=*/2, /*iam_floor=*/1),
                  RS_DSN_EM_INDEX_REQUIRE)
            << mode << " is withheld under the floor";
    }
    // Nothing selected is lifted too: the floor has to leave a usable mode on screen.
    EXPECT_EQ(rs_dsn_em_index_on_auth_change(/*em_index_shown=*/-1,
                                             /*ssl_mode_named=*/0, NULL, /*em=*/4,
                                             /*iam_floor=*/1),
              RS_DSN_EM_INDEX_REQUIRE);
}

// Selecting an auth type that lifts the floor rebuilds the combo with all six. The shown require may
// be the floor's own lift, so the DSN decides.
TEST(DsnSslComboTest, lifting_the_floor_takes_the_selection_from_the_dsn) {
    EXPECT_EQ(rs_dsn_em_index_on_auth_change(/*em_index_shown=*/RS_DSN_EM_INDEX_REQUIRE,
                                             /*ssl_mode_named=*/1, "allow", /*em=*/2,
                                             /*iam_floor=*/0), 4);
    EXPECT_EQ(rs_dsn_em_index_on_auth_change(/*em_index_shown=*/RS_DSN_EM_INDEX_REQUIRE,
                                             /*ssl_mode_named=*/1, "prefer", /*em=*/2,
                                             /*iam_floor=*/0), 5);
    // A DSN whose mode comes only from EncryptionMethod is read the same way the
    // first build reads it, down to an out-of-range code selecting nothing.
    EXPECT_EQ(rs_dsn_em_index_on_auth_change(/*em_index_shown=*/RS_DSN_EM_INDEX_REQUIRE,
                                             /*ssl_mode_named=*/0, NULL, /*em=*/0,
                                             /*iam_floor=*/0), 0);
    EXPECT_EQ(rs_dsn_em_index_on_auth_change(/*em_index_shown=*/RS_DSN_EM_INDEX_REQUIRE,
                                             /*ssl_mode_named=*/0, NULL, /*em=*/4,
                                             /*iam_floor=*/0), -1);
    // The accepted cost of deciding from the DSN: a verify-full the user picked by
    // hand before changing the auth type goes the same way as the lift's require.
    EXPECT_EQ(rs_dsn_em_index_on_auth_change(/*em_index_shown=*/2,
                                             /*ssl_mode_named=*/1, "allow", /*em=*/2,
                                             /*iam_floor=*/0), 4);
}

// A round trip on a Standard DSN storing SSLMode=allow: the Connect tab goes to an IAM auth type
// (which shows require) and back. SSL was not touched, so the save must keep allow.
TEST(DsnSslComboTest, an_auth_type_round_trip_keeps_a_stored_allow) {
    const char *const stored = "allow";
    const int em = 2; /* the attribute table's EncryptionMethod default */

    // The tab is first built under Standard: all six modes, allow selected.
    const int on_open = rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, stored,
                                                         em, /*iam_floor=*/0);
    ASSERT_EQ(on_open, 4);
    ASSERT_NE(rs_dsn_em_offered_count(/*iam_floor=*/1), rs_dsn_em_offered_count(0));

    // Switched to an IAM auth type: the tab rebuilds and lifts the withheld allow; saving here
    // persists the require on screen.
    const int floored = rs_dsn_em_index_on_auth_change(on_open, /*ssl_mode_named=*/1,
                                                       stored, em, /*iam_floor=*/1);
    EXPECT_EQ(floored, RS_DSN_EM_INDEX_REQUIRE);
    EXPECT_EQ(savedSslMode(floored, on_open, /*ssl_mode_named=*/1, stored), "require");

    // Switched back to Standard: the run grows, so the tab rebuilds again and the
    // selection comes from the DSN rather than from the require on screen.
    const int relaxed = rs_dsn_em_index_on_auth_change(floored, /*ssl_mode_named=*/1,
                                                       stored, em, /*iam_floor=*/0);
    EXPECT_STREQ(rs_dsn_em_modes[relaxed], "allow");
    // The selection is back where the tab opened, so the change detection agrees
    // with it as it stands and the save writes nothing.
    EXPECT_EQ(relaxed, on_open);
    EXPECT_FALSE(rs_dsn_em_should_write_ssl_mode(relaxed, on_open,
                                                 /*ssl_mode_named=*/1, stored));
    EXPECT_EQ(savedSslMode(relaxed, on_open, /*ssl_mode_named=*/1, stored), "allow");
}

// The same round trip for a DSN whose SSL tab was first built under an IAM auth type, so it opened on
// require. The re-derived allow differs from the opening index, so the save writes SSLMode back as stored.
TEST(DsnSslComboTest, a_round_trip_that_opened_lifted_still_keeps_the_stored_mode) {
    for (const char *stored : {"allow", "prefer"}) {
        const int on_open = rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1,
                                                             stored, /*em=*/2,
                                                             /*iam_floor=*/1);
        ASSERT_EQ(on_open, RS_DSN_EM_INDEX_REQUIRE) << stored;

        // Switched to Standard, which offers the stored mode again.
        const int relaxed = rs_dsn_em_index_on_auth_change(on_open,
                                                           /*ssl_mode_named=*/1,
                                                           stored, /*em=*/2,
                                                           /*iam_floor=*/0);
        EXPECT_STREQ(rs_dsn_em_modes[relaxed], stored);
        EXPECT_TRUE(rs_dsn_em_should_write_ssl_mode(relaxed, on_open,
                                                    /*ssl_mode_named=*/1, stored))
            << stored;
        EXPECT_EQ(savedSslMode(relaxed, on_open, /*ssl_mode_named=*/1, stored),
                  stored);
    }
}

// ===========================================================================
// Which DSNs the SSL tab holds to the "require" floor (rs_dsn_ssl_keys.h)
//
// rs_dsn_auth_may_be_iam() takes the IAM flag, plugin_name and AuthProfile. "" is a key the DSN never named;
// "0" is what the setup table returns for an absent IAM flag.
// ===========================================================================

// The ordinary Standard DSN. It keeps the full six-entry combo.
TEST(DsnSslComboTest, an_ordinary_standard_dsn_may_not_be_iam) {
    EXPECT_FALSE(rs_dsn_auth_may_be_iam(/*iam=*/"0", /*plugin_name=*/"", /*auth_profile=*/""));
    // The same DSN read back as absent keys rather than table defaults.
    EXPECT_FALSE(rs_dsn_auth_may_be_iam("", "", ""));
    EXPECT_FALSE(rs_dsn_auth_may_be_iam(NULL, NULL, NULL));
    // A flag spelled false is not a signal either.
    EXPECT_FALSE(rs_dsn_auth_may_be_iam("false", "", ""));
    EXPECT_FALSE(rs_dsn_auth_may_be_iam("FALSE", "", ""));
    // So it is offered every mode, and a stored allow or prefer survives.
    EXPECT_EQ(rs_dsn_em_offered_count(rs_dsn_auth_may_be_iam("0", "", "")), 6);
    EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(
                  /*ssl_mode_named=*/1, "allow", /*em=*/2,
                  rs_dsn_auth_may_be_iam("0", "", "")), 4);
}

// The IAM flag is the attribute that admits a DSN to the IAM path at connect time,
// where ValidateConnectionAttributes rejects allow and prefer.
TEST(DsnSslComboTest, a_dsn_with_the_iam_flag_may_be_iam) {
    EXPECT_TRUE(rs_dsn_auth_may_be_iam("1", "", ""));
    EXPECT_TRUE(rs_dsn_auth_may_be_iam("TRUE", "", ""));
    EXPECT_TRUE(rs_dsn_auth_may_be_iam("true", "", ""));
    // With an IdP plugin beside it, which is the ordinary federated IAM shape.
    EXPECT_TRUE(rs_dsn_auth_may_be_iam("1", "AzureAD", ""));
}

// The two IdC plugins reach the IAM path through handleFederatedNonIamConnection() without the IAM
// flag, so plugin_name alone holds those DSNs to the floor (compared without case).
TEST(DsnSslComboTest, a_dsn_with_an_idc_plugin_may_be_iam_without_the_iam_flag) {
    EXPECT_TRUE(rs_dsn_auth_may_be_iam("0", "IdpTokenAuthPlugin", ""));
    EXPECT_TRUE(rs_dsn_auth_may_be_iam("0", "BrowserIdcAuthPlugin", ""));
    EXPECT_TRUE(rs_dsn_auth_may_be_iam("0", "idptokenauthplugin", ""));
    EXPECT_TRUE(rs_dsn_auth_may_be_iam("0", "BROWSERIDCAUTHPLUGIN", ""));
}

// A fetched AuthProfile can set IAM=1 at connect time, so a Standard DSN that stores one is held
// to the floor on the save path. Without it the same DSN keeps allow.
TEST(DsnSslComboTest, a_standard_dsn_with_an_auth_profile_is_held_to_the_floor_on_save) {
    // Step 1: IAM=0 and no plugin, but an AuthProfile: allow is withheld, so save lifts it.
    const int withProfile = rs_dsn_auth_may_be_iam(/*iam=*/"0", /*plugin_name=*/"",
                                                   /*auth_profile=*/"my-profile");
    EXPECT_TRUE(withProfile);
    EXPECT_FALSE(rs_dsn_em_index_is_offered(rs_dsn_em_index_for_mode("allow"), withProfile));
    EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, "allow", /*em=*/2,
                                               withProfile),
              RS_DSN_EM_INDEX_REQUIRE);

    // Step 2: the same DSN with an empty AuthProfile keeps allow.
    const int withoutProfile = rs_dsn_auth_may_be_iam(/*iam=*/"0", /*plugin_name=*/"",
                                                      /*auth_profile=*/"");
    EXPECT_FALSE(withoutProfile);
    EXPECT_TRUE(rs_dsn_em_index_is_offered(rs_dsn_em_index_for_mode("allow"), withoutProfile));
    EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, "allow", /*em=*/2,
                                               withoutProfile),
              rs_dsn_em_index_for_mode("allow"));
}

// Every other plugin_name needs the IAM flag to reach the IAM path. Without it the
// connection never enters RsIamClient, so the floor does not apply.
TEST(DsnSslComboTest, a_plugin_name_without_the_iam_flag_does_not_reach_the_floor) {
    EXPECT_FALSE(rs_dsn_auth_may_be_iam("0", "AzureAD", ""));
    EXPECT_FALSE(rs_dsn_auth_may_be_iam("0", "BrowserSAML", ""));
    EXPECT_FALSE(rs_dsn_auth_may_be_iam("0", "JwtIamAuthPlugin", ""));
    // A truncated IdC name is not one of the two the runtime matches.
    EXPECT_FALSE(rs_dsn_auth_may_be_iam("0", "BrowserIdcAuth", ""));
}

// Credential attributes cannot reach the IAM path on their own: a Standard DSN with a leftover
// Profile, instance profile flag or access keys has IAM=0 and no plugin_name, so it keeps allow
// and prefer. The dialog writes IAM explicitly, so a saved Standard DSN reads back as 0.
TEST(DsnSslComboTest, stale_credential_attributes_do_not_reach_the_floor) {
    const int floor = rs_dsn_auth_may_be_iam(/*iam=*/"0", /*plugin_name=*/"", /*auth_profile=*/"");

    EXPECT_FALSE(floor);
    EXPECT_EQ(rs_dsn_em_offered_count(floor), 6);
    // A stored allow or prefer is offered and is not lifted to require.
    EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, "allow",
                                               /*em=*/2, floor), 4);
    EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, "prefer",
                                               /*em=*/2, floor), 5);
}

// Switching a DSN from an IAM auth type back to Standard. The dialog clears AuthType, plugin_name
// and the IAM flag but leaves Profile, so allow and prefer must be offered again and kept.
TEST(DsnSslComboTest, a_dsn_switched_from_iam_with_a_profile_back_to_standard_keeps_allow) {
    // While it was an IAM Profile DSN the floor applied.
    const int iam_floor = rs_dsn_auth_may_be_iam(/*iam=*/"1", /*plugin_name=*/"", /*auth_profile=*/"");

    EXPECT_TRUE(iam_floor);
    EXPECT_EQ(rs_dsn_em_offered_count(iam_floor), 3);

    // Switched back to Standard: IAM=0 and plugin_name cleared, Profile left over.
    const int standard_floor = rs_dsn_auth_may_be_iam(/*iam=*/"0",
                                                      /*plugin_name=*/"", /*auth_profile=*/"");

    EXPECT_FALSE(standard_floor);
    EXPECT_EQ(rs_dsn_em_offered_count(standard_floor), 6);
    // Both weaker modes are offered again and open where they are stored.
    EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, "allow",
                                               /*em=*/2, standard_floor), 4);
    EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, "prefer",
                                               /*em=*/2, standard_floor), 5);
    // And asking for allow persists it: the selection moved, so the save path
    // writes SSLMode rather than leaving the stored require in place.
    EXPECT_TRUE(rs_dsn_em_should_write_ssl_mode(/*em_index=*/4,
                                                /*em_index_on_open=*/3,
                                                /*ssl_mode_named=*/1, "require"));
}

// What the user sees for a DSN that is held to the floor: the combo withholds
// disable, allow and prefer, and a DSN that had stored one of them opens on require.
TEST(DsnSslComboTest, a_dsn_that_may_be_iam_is_offered_only_the_accepted_modes) {
    const int floor = rs_dsn_auth_may_be_iam(/*iam=*/"1", /*plugin_name=*/"AzureAD", /*auth_profile=*/"");

    EXPECT_TRUE(floor);
    EXPECT_EQ(rs_dsn_em_offered_count(floor), 3);
    EXPECT_FALSE(rs_dsn_em_index_is_offered(RS_DSN_EM_INDEX_VERIFY_CA - 1, floor));
    EXPECT_TRUE(rs_dsn_em_index_is_offered(RS_DSN_EM_INDEX_VERIFY_CA, floor));
    EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, "allow",
                                               /*em=*/2, floor),
              RS_DSN_EM_INDEX_REQUIRE);
    EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, "prefer",
                                               /*em=*/2, floor),
              RS_DSN_EM_INDEX_REQUIRE);
    EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, "disable",
                                               /*em=*/2, floor),
              RS_DSN_EM_INDEX_REQUIRE);
    // And the stronger modes it did store are left exactly where they were.
    EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, "verify-full",
                                               /*em=*/0, floor), 2);
}

// A DSN naming a non-Standard AuthType but no IAM flag: AuthType is read only after the IAM flag or
// an IdC plugin_name sends the connection down the IAM path, so it keeps prefer.
TEST(DsnSslComboTest, a_non_standard_auth_type_without_the_iam_flag_keeps_prefer) {
    // AuthType=Profile as an older release or a hand-written DSN may hold it, with
    // no IAM flag and no plugin_name.
    const int floor = rs_dsn_auth_may_be_iam(/*iam=*/"0", /*plugin_name=*/"", /*auth_profile=*/"");

    EXPECT_FALSE(floor);
    EXPECT_EQ(rs_dsn_em_offered_count(floor), 6);
    // Neither weaker mode is withheld, so there is nothing for the floor to lift.
    for (const char *mode : {"prefer", "allow", "disable"}) {
        EXPECT_TRUE(rs_dsn_em_index_is_offered(rs_dsn_em_index_for_mode(mode),
                                               floor))
            << mode << " must stay available to a DSN that cannot reach IAM";
        EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, mode,
                                                   /*em=*/2, floor),
                  rs_dsn_em_index_for_mode(mode))
            << mode;
    }
    // The same DSN once its IAM flag is set does reach the floor.
    EXPECT_TRUE(rs_dsn_auth_may_be_iam(/*iam=*/"1", /*plugin_name=*/"", /*auth_profile=*/""));
    EXPECT_EQ(rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, "prefer",
                                               /*em=*/2,
                                               rs_dsn_auth_may_be_iam("1", "", "")),
              RS_DSN_EM_INDEX_REQUIRE);
}

// ===========================================================================
// The live auth type, not the stored IAM flag, decides the tab's floor
// ===========================================================================

// A DSN opens under an IAM auth type storing SSLMode=allow, then the Connect tab is switched to
// Standard. The IAM flag is still "1" in the DSN, but the live auth type lifts the floor, so allow
// comes back and is saved.
TEST(DsnSslComboTest, a_dsn_opened_as_iam_switched_to_standard_gets_its_allow_back) {
    const char *const stored = "allow";
    const int em = 2; /* the attribute table's EncryptionMethod default */
    const int iam_floor = 1;
    const int standard_floor = 0;

    // Built under the IAM auth type the DSN opened with: three entries, require.
    const int on_open = rs_dsn_em_index_on_open_for_auth(/*ssl_mode_named=*/1, stored, em,
                                                         iam_floor);
    ASSERT_EQ(on_open, RS_DSN_EM_INDEX_REQUIRE);
    // The offered run grows, which is what makes the tab rebuild.
    EXPECT_EQ(rs_dsn_em_offered_count(standard_floor), 6);
    EXPECT_NE(rs_dsn_em_offered_count(standard_floor), rs_dsn_em_offered_count(iam_floor));

    const int relaxed = rs_dsn_em_index_on_auth_change(on_open, /*ssl_mode_named=*/1, stored,
                                                       em, standard_floor);
    EXPECT_STREQ(rs_dsn_em_modes[relaxed], "allow");
    EXPECT_EQ(savedSslMode(relaxed, on_open, /*ssl_mode_named=*/1, stored), "allow");
}

// ===========================================================================
// Connection string parsing tests for UseDeclareFetch / Fetch
// Uses RS_CONN_INFO::parseConnectString which is the same path used
// by SQLDriverConnect.
// ===========================================================================

class UseDeclareFetchConnStringTest : public ::testing::Test {
protected:
    RS_ENV_INFO envInfo;
    RS_CONN_INFO *pConn;

    void SetUp() override {
        memset(&envInfo, 0, sizeof(RS_ENV_INFO));
        pConn = new RS_CONN_INFO(&envInfo);
        pConn->pConnectProps = new RS_CONNECT_PROPS_INFO();
        pConn->pConnAttr = new RS_CONN_ATTR_INFO();
    }

    void TearDown() override {
        delete pConn->pConnAttr;
        delete pConn->pConnectProps;
        delete pConn;
    }

    // Parse a connection string (non-DSN path)
    void parseConnStr(const char *connStr) {
        pConn->parseConnectString(const_cast<char *>(connStr), SQL_NTS, FALSE, FALSE);
    }
};

// The parser only records what it saw; resolveSslPolicy decides, tested below.

TEST_F(UseDeclareFetchConnStringTest, EncryptionMethod_is_recorded) {
    parseConnStr("Driver=Amazon Redshift ODBC Driver;EncryptionMethod=2;");
    ASSERT_TRUE(pConn->pConnectProps->connEncryptionMethod.has_value());
    EXPECT_EQ(*pConn->pConnectProps->connEncryptionMethod, 2);
}

TEST_F(UseDeclareFetchConnStringTest, EM_alias_is_recorded) {
    parseConnStr("Driver=Amazon Redshift ODBC Driver;EM=1;");
    ASSERT_TRUE(pConn->pConnectProps->connEncryptionMethod.has_value());
    EXPECT_EQ(*pConn->pConnectProps->connEncryptionMethod, 1);
}

// A typo still counts as a choice, like an unreadable SSLMode: default mode, strict.
TEST_F(UseDeclareFetchConnStringTest, A_nonnumeric_EncryptionMethod_is_unreadable_and_strict) {
    parseConnStr("Driver=Amazon Redshift ODBC Driver;EncryptionMethod=abc;");
    ASSERT_TRUE(pConn->pConnectProps->connEncryptionMethod.has_value());
    EXPECT_EQ(*pConn->pConnectProps->connEncryptionMethod, RS_ENCRYPTION_METHOD_UNREADABLE);

    RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
    EXPECT_STREQ(pConn->pConnectProps->szSslMode, "verify-full");
    EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, PQ_SSL_MISMATCH_STRICT);
}

// Only a whole number is a value; a numeric prefix must not turn SSL off.
TEST(EncryptionMethodReadTest, reads_only_a_whole_number) {
    const struct {
        const char *text;
        int expected;
    } cases[] = {
        {"0", 0}, {"1", 1}, {"2", 2}, {"3", 3}, {" 2", 2}, {"2 ", 2}, {" 2 ", 2},
        {"+2", 2}, {"-1", -1},
        {"0oops", RS_ENCRYPTION_METHOD_UNREADABLE},
        {"1x", RS_ENCRYPTION_METHOD_UNREADABLE},
        {"2.0", RS_ENCRYPTION_METHOD_UNREADABLE},
        {"0x1", RS_ENCRYPTION_METHOD_UNREADABLE},
        {"2 2", RS_ENCRYPTION_METHOD_UNREADABLE},
        {"abc", RS_ENCRYPTION_METHOD_UNREADABLE},
        {" ", RS_ENCRYPTION_METHOD_UNREADABLE},
        {"99999999999", RS_ENCRYPTION_METHOD_UNREADABLE},
        {"-99999999999", RS_ENCRYPTION_METHOD_UNREADABLE},
    };
    for (const auto &c : cases) {
        EXPECT_EQ(RS_CONN_INFO::readEncryptionMethod(c.text), c.expected)
            << "text: [" << c.text << "]";
    }
}

TEST_F(UseDeclareFetchConnStringTest, A_numeric_prefix_EncryptionMethod_is_unreadable_and_strict) {
    for (const char *value : {"0oops", "1x", "3abc"}) {
        pConn->pConnectProps->connEncryptionMethod.reset();
        parseConnStr((std::string("Driver=Amazon Redshift ODBC Driver;EncryptionMethod=") +
                      value + ";").c_str());
        ASSERT_TRUE(pConn->pConnectProps->connEncryptionMethod.has_value()) << value;
        EXPECT_EQ(*pConn->pConnectProps->connEncryptionMethod,
                  RS_ENCRYPTION_METHOD_UNREADABLE) << value;

        RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
        EXPECT_STREQ(pConn->pConnectProps->szSslMode, "verify-full") << value;
        EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, PQ_SSL_MISMATCH_STRICT) << value;
    }
}

TEST_F(UseDeclareFetchConnStringTest, A_blank_EncryptionMethod_records_nothing) {
    parseConnStr("Driver=Amazon Redshift ODBC Driver;EncryptionMethod=;");
    EXPECT_FALSE(pConn->pConnectProps->connEncryptionMethod.has_value());
}

TEST_F(UseDeclareFetchConnStringTest, No_mode_keyword_records_nothing) {
    parseConnStr("Driver=Amazon Redshift ODBC Driver;");
    EXPECT_FALSE(pConn->pConnectProps->connSslMode.has_value());
    EXPECT_FALSE(pConn->pConnectProps->connEncryptionMethod.has_value());
}

// A blank SSLMode is how a caller drops the DSN's mode, so presence is recorded.
TEST_F(UseDeclareFetchConnStringTest, An_empty_SSLMode_is_recorded_as_present) {
    parseConnStr("Driver=Amazon Redshift ODBC Driver;SSLMode=;");
    ASSERT_TRUE(pConn->pConnectProps->connSslMode.has_value());
    EXPECT_EQ(*pConn->pConnectProps->connSslMode, "");
}

// ===========================================================================
// resolveSslPolicy runs after both sources are read, so parse order can't matter.
// ===========================================================================

namespace {

struct SslResolveCase {
    const char *name;
    const char *dsnSslMode;      // nullptr = key absent
    int dsnEncryptionMethod;     // -1 = key absent
    const char *connSslMode;     // nullptr = key absent
    int connEncryptionMethod;    // -1 = key absent
    const char *expectedMode;
    int expectedSource;
    int expectedPolicy;
};

const SslResolveCase kSslResolveCases[] = {
    // Nothing named: the built-in default, which tolerates a mismatch.
    {"nothing named", nullptr, -1, nullptr, -1,
     "verify-full", RS_SSL_MODE_SOURCE_BUILT_IN_DEFAULT,
     PQ_SSL_MISMATCH_ALLOW_TEMPORARILY},

    // A single source names the mode.
    {"connection SSLMode", nullptr, -1, "verify-ca", -1,
     "verify-ca", RS_SSL_MODE_SOURCE_CONN_SSL_MODE, PQ_SSL_MISMATCH_STRICT},
    {"connection EncryptionMethod", nullptr, -1, nullptr, 2,
     "verify-full", RS_SSL_MODE_SOURCE_CONN_ENCRYPTION_METHOD,
     PQ_SSL_MISMATCH_STRICT},
    {"DSN SSLMode", "require", -1, nullptr, -1,
     "require", RS_SSL_MODE_SOURCE_DSN_SSL_MODE, PQ_SSL_MISMATCH_STRICT},
    {"DSN EncryptionMethod", nullptr, 3, nullptr, -1,
     "require", RS_SSL_MODE_SOURCE_DSN_ENCRYPTION_METHOD,
     PQ_SSL_MISMATCH_STRICT},

    // The connection string outranks the DSN, whichever key each one uses.
    {"connection EncryptionMethod beats DSN SSLMode", "verify-ca", -1, nullptr, 2,
     "verify-full", RS_SSL_MODE_SOURCE_CONN_ENCRYPTION_METHOD,
     PQ_SSL_MISMATCH_STRICT},
    {"connection SSLMode beats DSN SSLMode", "verify-ca", -1, "require", -1,
     "require", RS_SSL_MODE_SOURCE_CONN_SSL_MODE, PQ_SSL_MISMATCH_STRICT},
    {"SSLMode outranks EncryptionMethod in one source", nullptr, -1, "require", 2,
     "require", RS_SSL_MODE_SOURCE_CONN_SSL_MODE, PQ_SSL_MISMATCH_STRICT},

    // A blank connection SSLMode drops the DSN's SSLMode but not an EncryptionMethod.
    {"empty SSLMode drops a DSN SSLMode", "verify-ca", -1, "", -1,
     "verify-full", RS_SSL_MODE_SOURCE_BUILT_IN_DEFAULT,
     PQ_SSL_MISMATCH_ALLOW_TEMPORARILY},
    {"empty SSLMode keeps a DSN EncryptionMethod", nullptr, 3, "", -1,
     "require", RS_SSL_MODE_SOURCE_DSN_ENCRYPTION_METHOD,
     PQ_SSL_MISMATCH_STRICT},
    {"empty SSLMode keeps a connection EncryptionMethod", nullptr, -1, "", 3,
     "require", RS_SSL_MODE_SOURCE_CONN_ENCRYPTION_METHOD,
     PQ_SSL_MISMATCH_STRICT},

    // An unreadable value is skipped when picking the mode but still forces strict,
    // so a typo can't relax a connection.
    {"invalid SSLMode names no mode but stays strict", nullptr, -1,
     "verify-fulll", -1, "verify-full", RS_SSL_MODE_SOURCE_BUILT_IN_DEFAULT,
     PQ_SSL_MISMATCH_STRICT},
    {"invalid EncryptionMethod names no mode but stays strict", nullptr, -1,
     nullptr, 99, "verify-full", RS_SSL_MODE_SOURCE_BUILT_IN_DEFAULT,
     PQ_SSL_MISMATCH_STRICT},
    {"invalid connection SSLMode drops the DSN mode and stays strict", "require",
     -1, "nonsense", -1, "verify-full", RS_SSL_MODE_SOURCE_BUILT_IN_DEFAULT,
     PQ_SSL_MISMATCH_STRICT},
    {"invalid connection EncryptionMethod falls back to the DSN", "require", -1,
     nullptr, 99, "require", RS_SSL_MODE_SOURCE_DSN_SSL_MODE,
     PQ_SSL_MISMATCH_STRICT},

    // A DSN's own mode is honoured, including the verify-ca that older releases
    // wrote into every DSN, so upgrading never changes how an existing DSN connects.
    {"DSN SSLMode=verify-ca is kept", "verify-ca", -1, nullptr, -1,
     "verify-ca", RS_SSL_MODE_SOURCE_DSN_SSL_MODE, PQ_SSL_MISMATCH_STRICT},
    {"DSN SSLMode=verify-ca is kept in any case", "VERIFY-CA", -1, nullptr, -1,
     "verify-ca", RS_SSL_MODE_SOURCE_DSN_SSL_MODE, PQ_SSL_MISMATCH_STRICT},
    {"DSN EncryptionMethod=1 is kept", nullptr, 1, nullptr, -1,
     "verify-ca", RS_SSL_MODE_SOURCE_DSN_ENCRYPTION_METHOD, PQ_SSL_MISMATCH_STRICT},
    {"DSN SSLMode outranks DSN EncryptionMethod", "verify-ca", 3, nullptr, -1,
     "verify-ca", RS_SSL_MODE_SOURCE_DSN_SSL_MODE, PQ_SSL_MISMATCH_STRICT},
    {"connection verify-full overrides a DSN verify-ca", "verify-ca", 1,
     "verify-full", -1, "verify-full", RS_SSL_MODE_SOURCE_CONN_SSL_MODE,
     PQ_SSL_MISMATCH_STRICT},
    {"connection EncryptionMethod=1 is honoured", nullptr, -1, nullptr, 1,
     "verify-ca", RS_SSL_MODE_SOURCE_CONN_ENCRYPTION_METHOD,
     PQ_SSL_MISMATCH_STRICT},
    {"a DSN verify-full is honoured", "verify-full", -1,
     nullptr, -1, "verify-full", RS_SSL_MODE_SOURCE_DSN_SSL_MODE,
     PQ_SSL_MISMATCH_STRICT},

    // disable is a named mode, so it's strict too.
    {"EncryptionMethod 0 names disable", nullptr, -1, nullptr, 0,
     "disable", RS_SSL_MODE_SOURCE_CONN_ENCRYPTION_METHOD,
     PQ_SSL_MISMATCH_STRICT},

    // allow and prefer are named SSLMode values with no EncryptionMethod code.
    // They resolve and, being named, are strict like any other named mode.
    {"connection SSLMode=allow", nullptr, -1, "allow", -1,
     "allow", RS_SSL_MODE_SOURCE_CONN_SSL_MODE, PQ_SSL_MISMATCH_STRICT},
    {"connection SSLMode=prefer", nullptr, -1, "prefer", -1,
     "prefer", RS_SSL_MODE_SOURCE_CONN_SSL_MODE, PQ_SSL_MISMATCH_STRICT},
    {"DSN SSLMode=allow", "allow", -1, nullptr, -1,
     "allow", RS_SSL_MODE_SOURCE_DSN_SSL_MODE, PQ_SSL_MISMATCH_STRICT},
    {"DSN SSLMode=prefer in any case", "PREFER", -1, nullptr, -1,
     "prefer", RS_SSL_MODE_SOURCE_DSN_SSL_MODE, PQ_SSL_MISMATCH_STRICT},
    // SSLMode=allow outranks a DSN EncryptionMethod, which cannot express allow.
    {"connection SSLMode=allow beats DSN EncryptionMethod", nullptr, 3, "allow", -1,
     "allow", RS_SSL_MODE_SOURCE_CONN_SSL_MODE, PQ_SSL_MISMATCH_STRICT},
};

} // namespace

TEST(SslModeResolveTest, resolves_every_combination_of_sources) {
    for (const SslResolveCase &c : kSslResolveCases) {
        RS_CONNECT_PROPS_INFO props;
        if (c.dsnSslMode) {
            props.dsnSslMode = std::string(c.dsnSslMode);
        }
        if (c.dsnEncryptionMethod != -1) {
            props.dsnEncryptionMethod = c.dsnEncryptionMethod;
        }
        if (c.connSslMode) {
            props.connSslMode = std::string(c.connSslMode);
        }
        if (c.connEncryptionMethod != -1) {
            props.connEncryptionMethod = c.connEncryptionMethod;
        }

        RS_CONN_INFO::resolveSslPolicy(&props);

        EXPECT_STREQ(props.szSslMode, c.expectedMode) << "case: " << c.name;
        EXPECT_EQ(props.iSslModeSource, c.expectedSource) << "case: " << c.name;
        EXPECT_EQ(props.iSslMismatchPolicy, c.expectedPolicy) << "case: " << c.name;
    }
}

// Checks the resolver's output, not the table's expectations: a mismatch is
// only ever tolerated when the mode came from the built-in default.
TEST(SslModeResolveTest, only_the_built_in_default_tolerates_a_mismatch) {
    for (const SslResolveCase &c : kSslResolveCases) {
        RS_CONNECT_PROPS_INFO props;
        if (c.dsnSslMode) {
            props.dsnSslMode = std::string(c.dsnSslMode);
        }
        if (c.dsnEncryptionMethod != -1) {
            props.dsnEncryptionMethod = c.dsnEncryptionMethod;
        }
        if (c.connSslMode) {
            props.connSslMode = std::string(c.connSslMode);
        }
        if (c.connEncryptionMethod != -1) {
            props.connEncryptionMethod = c.connEncryptionMethod;
        }

        RS_CONN_INFO::resolveSslPolicy(&props);

        bool tolerated =
            props.iSslMismatchPolicy == PQ_SSL_MISMATCH_ALLOW_TEMPORARILY;
        bool defaulted =
            props.iSslModeSource == RS_SSL_MODE_SOURCE_BUILT_IN_DEFAULT;

        // Not the reverse: an unreadable value can land on the default and stay strict.
        if (tolerated) {
            EXPECT_TRUE(defaulted)
                << "case: " << c.name
                << " -- a mismatch was tolerated for a mode the caller named";
        }
    }
}

/**
 * @brief Frees what a fixture's connection allocated, as ParseConnectStringTest
 *        does: RS_CONNECT_PROPS_INFO and RS_ENV_INFO have no destructors.
 * @param envInfo The fixture's environment; its attributes are freed.
 * @param pConn The fixture's connection; deleted on return.
 */
static void freeTestConnection(RS_ENV_INFO &envInfo, RS_CONN_INFO *pConn) {
    RS_CONNECT_PROPS_INFO *props = pConn->pConnectProps;
    RsFree(props->pConnectStr);
    props->pConnectStr = NULL;
    RsFree(props->pInitializationString);
    props->pInitializationString = NULL;
    if (props->pIamProps) {
        RsFree(props->pIamProps->pszJwt);
        props->pIamProps->pszJwt = NULL;
        RsFree(props->pIamProps);
        props->pIamProps = NULL;
    }
    RsFree(props->pHttpsProps);
    props->pHttpsProps = NULL;
    RsFree(props->pTcpProxyProps);
    props->pTcpProxyProps = NULL;
    RsFree(pConn->pConnAttr->pTraceFile);
    pConn->pConnAttr->pTraceFile = NULL;

    delete pConn->pConnAttr;
    delete pConn->pConnectProps;
    delete pConn;

    delete envInfo.pEnvAttr;
    envInfo.pEnvAttr = NULL;
}

// DSNs are read by readMoreConnectPropsFromRegistry: from a temp odbc.ini (ODBCINI) on
// Linux/macOS, and from a real registry DSN on Windows.
class SslModeDsnTest : public ::testing::Test {
protected:
    RS_ENV_INFO envInfo;
    RS_CONN_INFO *pConn = NULL;
#ifdef WIN32
    std::vector<std::string> writtenDsns;
#else
    std::string workdir;
    std::string savedOdbcIni;
    bool hadOdbcIni = false;
#endif

    /** @brief Creates the connection and, off Windows, a temp directory for odbc.ini. */
    void SetUp() override {
        pConn = new RS_CONN_INFO(&envInfo);
        pConn->pConnectProps = new RS_CONNECT_PROPS_INFO();
        pConn->pConnAttr = new RS_CONN_ATTR_INFO();
#ifndef WIN32
        const char *existing = getenv(ODBCINI_ENV_VAR);
        hadOdbcIni = existing != NULL;
        if (hadOdbcIni) {
            savedOdbcIni = existing;
        }

        char templ[] = "/tmp/rsodbc_dsn_XXXXXX";
        ASSERT_NE(mkdtemp(templ), (char *)NULL);
        workdir = templ;
#endif
    }

    /** @brief Removes the DSN the test wrote, restores ODBCINI and frees the connection. */
    void TearDown() override {
#ifdef WIN32
        for (const std::string &dsn : writtenDsns) {
            SQLWritePrivateProfileString(dsn.c_str(), NULL, NULL, ODBC_INI);
        }
#else
        if (hadOdbcIni) {
            setenv(ODBCINI_ENV_VAR, savedOdbcIni.c_str(), 1);
        } else {
            unsetenv(ODBCINI_ENV_VAR);
        }
        if (!workdir.empty()) {
            unlink((workdir + "/odbc.ini").c_str());
            rmdir(workdir.c_str());
        }
#endif
        freeTestConnection(envInfo, pConn);
    }

    /** @brief The DSN name a test's base name maps to, unique to this process. */
    static std::string uniqueDsn(const char *base) {
#ifdef WIN32
        const unsigned long pid = (unsigned long)GetCurrentProcessId();
#else
        const unsigned long pid = (unsigned long)getpid();
#endif
        return std::string(base) + "_rsodbc_" + std::to_string(pid);
    }

    /**
     * @brief Saves a DSN under uniqueDsn(base), in the registry on Windows and in a temp
     *        odbc.ini elsewhere, then reads it into the connection. On Windows it refuses
     *        to touch a DSN of that name it did not write itself.
     * @param base The DSN's base name; "[base]" in body is renamed to match.
     * @param text "[base]" then one key=value per line.
     */
    void readDsn(const char *base, const std::string &text) {
        const std::string name = uniqueDsn(base);
        const char *dsn = name.c_str();
        std::string body = text;
        const std::string header = std::string("[") + base + "]";
        if (body.compare(0, header.size(), header) == 0) {
            body.replace(0, header.size(), "[" + name + "]");
        }
#ifdef WIN32
        bool ours = false;
        for (const std::string &written : writtenDsns) {
            ours = ours || written == name;
        }
        if (!ours) {
            char keys[256];
            keys[0] = '\0';
            SQLGetPrivateProfileString(dsn, NULL, "", keys, (int)sizeof(keys), ODBC_INI);
            ASSERT_EQ(keys[0], '\0')
                << "a DSN named " << name << " already exists; refusing to overwrite it";
            writtenDsns.push_back(name);
        }
        SQLWritePrivateProfileString(dsn, NULL, NULL, ODBC_INI);
        size_t start = 0;
        while (start < body.size()) {
            size_t end = body.find('\n', start);
            std::string line = body.substr(start, end == std::string::npos ? std::string::npos : end - start);
            start = end == std::string::npos ? body.size() : end + 1;
            size_t eq = line.find('=');
            if (line.empty() || line[0] == '[' || eq == std::string::npos) {
                continue;
            }
            ASSERT_TRUE(SQLWritePrivateProfileString(dsn, line.substr(0, eq).c_str(),
                                                     line.substr(eq + 1).c_str(), ODBC_INI))
                << "could not write " << line << " to registry DSN " << dsn;
        }
#else
        std::string path = workdir + "/odbc.ini";
        FILE *fp = fopen(path.c_str(), "w");
        ASSERT_NE(fp, (FILE *)NULL);
        fputs(body.c_str(), fp);
        fclose(fp);
        setenv(ODBCINI_ENV_VAR, path.c_str(), 1);
#endif
        strncpy(pConn->pConnectProps->szDSN, dsn,
                sizeof(pConn->pConnectProps->szDSN) - 1);
        pConn->readMoreConnectPropsFromRegistry(FALSE);
    }
};

TEST_F(SslModeDsnTest, a_DSN_EncryptionMethod_is_recorded) {
    readDsn("emdsn",
            "[emdsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
            "Database=d\nEncryptionMethod=2\n");

    ASSERT_TRUE(pConn->pConnectProps->dsnEncryptionMethod.has_value());
    EXPECT_EQ(*pConn->pConnectProps->dsnEncryptionMethod, 2);

    RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, PQ_SSL_MISMATCH_STRICT)
        << "A DSN naming the mode through EncryptionMethod must fail a mismatch, "
           "not warn about it";
}

TEST_F(SslModeDsnTest, a_DSN_SSLMode_is_recorded) {
    readDsn("modedsn",
            "[modedsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
            "Database=d\nSSLMode=require\n");

    ASSERT_TRUE(pConn->pConnectProps->dsnSslMode.has_value());
    EXPECT_EQ(*pConn->pConnectProps->dsnSslMode, "require");

    RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
    EXPECT_STREQ(pConn->pConnectProps->szSslMode, "require");
    EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, PQ_SSL_MISMATCH_STRICT)
        << "A DSN naming the mode outright is the caller's choice";
}

// DSNs made by older releases keep the mode they carry after an upgrade.
TEST_F(SslModeDsnTest, an_upgraded_DSN_keeps_its_mode) {
    const char *const bodies[] = {
        // The old Linux/macOS odbc.ini template.
        "[olddsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
        "Database=d\nSSLMode=verify-ca\n",
        // What the old Windows dialog saved.
        "[olddsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
        "Database=d\nSSLMode=verify-ca\nEncryptionMethod=1\n",
    };
    for (const char *body : bodies) {
        pConn->pConnectProps->dsnSslMode.reset();
        pConn->pConnectProps->dsnEncryptionMethod.reset();
        readDsn("olddsn", body);

        RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
        EXPECT_STREQ(pConn->pConnectProps->szSslMode, "verify-ca")
            << "an upgraded DSN must keep its mode: " << body;
        EXPECT_EQ(pConn->pConnectProps->iSslModeSource,
                  RS_SSL_MODE_SOURCE_DSN_SSL_MODE) << body;
        EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy,
                  PQ_SSL_MISMATCH_STRICT) << body;
    }
}

TEST_F(SslModeDsnTest, a_DSN_EncryptionMethod_with_trailing_text_is_unreadable) {
    readDsn("typodsn",
            "[typodsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
            "Database=d\nEncryptionMethod=0oops\n");

    ASSERT_TRUE(pConn->pConnectProps->dsnEncryptionMethod.has_value());
    EXPECT_EQ(*pConn->pConnectProps->dsnEncryptionMethod, RS_ENCRYPTION_METHOD_UNREADABLE);

    RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
    EXPECT_STREQ(pConn->pConnectProps->szSslMode, "verify-full")
        << "0oops must not be read as 0, which disables SSL";
    EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, PQ_SSL_MISMATCH_STRICT);
}

TEST_F(SslModeDsnTest, a_nonnumeric_DSN_EncryptionMethod_is_unreadable_and_strict) {
    readDsn("typodsn",
            "[typodsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
            "Database=d\nEncryptionMethod=abc\n");

    ASSERT_TRUE(pConn->pConnectProps->dsnEncryptionMethod.has_value());
    EXPECT_EQ(*pConn->pConnectProps->dsnEncryptionMethod, RS_ENCRYPTION_METHOD_UNREADABLE);

    RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
    EXPECT_STREQ(pConn->pConnectProps->szSslMode, "verify-full");
    EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, PQ_SSL_MISMATCH_STRICT);
}

TEST_F(SslModeDsnTest, a_DSN_with_no_mode_key_stays_defaulted) {
    readDsn("plaindsn",
            "[plaindsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
            "Database=d\n");

    EXPECT_FALSE(pConn->pConnectProps->dsnSslMode.has_value());
    EXPECT_FALSE(pConn->pConnectProps->dsnEncryptionMethod.has_value());

    RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
    EXPECT_STREQ(pConn->pConnectProps->szSslMode, "verify-full");
    EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy,
              PQ_SSL_MISMATCH_ALLOW_TEMPORARILY)
        << "A DSN made from the shipped template names no mode, so it must keep "
           "the transitional warning instead of hard-failing";
}

// A DSN EncryptionMethod that fills the read buffer may be cut off, so it is never
// read as a number; the boundary is one character short of the buffer.
// Windows returns nothing for a value longer than the buffer, so the key reads as unset.
TEST_F(SslModeDsnTest, a_DSN_EncryptionMethod_that_fills_the_buffer_is_unreadable) {
    const std::string padded = "0" + std::string(70, ' ') + "oops";
    readDsn("longdsn", "[longdsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
                       "Database=d\nEncryptionMethod=" + padded + "\n");
#ifdef WIN32
    EXPECT_FALSE(pConn->pConnectProps->dsnEncryptionMethod.has_value());
#else
    ASSERT_TRUE(pConn->pConnectProps->dsnEncryptionMethod.has_value());
    EXPECT_EQ(*pConn->pConnectProps->dsnEncryptionMethod, RS_ENCRYPTION_METHOD_UNREADABLE)
        << "a cut-off value read as 0 would turn SSL off";
#endif

    RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
    EXPECT_STREQ(pConn->pConnectProps->szSslMode, "verify-full");
#ifdef WIN32
    EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, PQ_SSL_MISMATCH_ALLOW_TEMPORARILY);
#else
    EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, PQ_SSL_MISMATCH_STRICT);
#endif
}

TEST_F(SslModeDsnTest, a_DSN_EncryptionMethod_at_the_buffer_boundary) {
    // MAX_NUMBER_BUF_LEN - 2 characters still fit, so the number is read.
    const std::string fits = std::string(MAX_NUMBER_BUF_LEN - 3, '0') + "3";
    readDsn("edgedsn", "[edgedsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
                       "Database=d\nEncryptionMethod=" + fits + "\n");
    ASSERT_TRUE(pConn->pConnectProps->dsnEncryptionMethod.has_value());
    EXPECT_EQ(*pConn->pConnectProps->dsnEncryptionMethod, 3) << "value: " << fits;

    // One more character fills the buffer, so it is no longer trusted.
    const std::string fills = std::string(MAX_NUMBER_BUF_LEN - 2, '0') + "3";
    pConn->pConnectProps->dsnEncryptionMethod.reset();
    readDsn("edgedsn", "[edgedsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
                       "Database=d\nEncryptionMethod=" + fills + "\n");
    ASSERT_TRUE(pConn->pConnectProps->dsnEncryptionMethod.has_value());
    EXPECT_EQ(*pConn->pConnectProps->dsnEncryptionMethod, RS_ENCRYPTION_METHOD_UNREADABLE)
        << "value: " << fills;
}

// A DSN SSLMode longer than the buffer is read as a terminated string that names no mode.
// Windows returns nothing for it, so the key reads as unset.
TEST_F(SslModeDsnTest, a_DSN_SSLMode_that_fills_the_buffer_is_unreadable) {
    const std::string padded = "disable" + std::string(3000, ' ') + "x";
    readDsn("longdsn", "[longdsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
                       "Database=d\nSSLMode=" + padded + "\n");
#ifdef WIN32
    EXPECT_FALSE(pConn->pConnectProps->dsnSslMode.has_value());
#else
    ASSERT_TRUE(pConn->pConnectProps->dsnSslMode.has_value());
    EXPECT_LT(pConn->pConnectProps->dsnSslMode->size(), (size_t)MAX_IDEN_LEN)
        << "the value was not terminated inside the buffer";
#endif

    RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
    EXPECT_STREQ(pConn->pConnectProps->szSslMode, "verify-full") << "a cut-off value must not disable SSL";
#ifdef WIN32
    EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, PQ_SSL_MISMATCH_ALLOW_TEMPORARILY);
#else
    EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, PQ_SSL_MISMATCH_STRICT);
#endif
}

// A blank DSN key names nothing, so the mode stays defaulted.
TEST_F(SslModeDsnTest, blank_DSN_SSL_keys_stay_defaulted) {
    readDsn("blankdsn", "[blankdsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
                        "Database=d\nSSLMode=\nEncryptionMethod=\n");
    EXPECT_FALSE(pConn->pConnectProps->dsnSslMode.has_value());
    EXPECT_FALSE(pConn->pConnectProps->dsnEncryptionMethod.has_value());

    RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
    EXPECT_STREQ(pConn->pConnectProps->szSslMode, "verify-full");
    EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, PQ_SSL_MISMATCH_ALLOW_TEMPORARILY);
}

// Test Connection sends the DSN's SSL keys in a connection string; it must reach the
// same mode and policy as using the saved DSN. Keys are picked as setup.c does: the
// dialog holds the DSN value or its table default, and only named keys go out.
TEST_F(SslModeDsnTest, test_connection_matches_the_saved_dsn) {
    struct DsnKeys { const char *sslMode; const char *encryptionMethod; };
    const DsnKeys cases[] = {
        {nullptr, nullptr}, {"verify-ca", nullptr}, {nullptr, "1"},
        {"verify-ca", "1"}, {"verify-full", "2"}, {"require", nullptr},
        {"disable", nullptr}, {nullptr, "3"},
    };
    for (const DsnKeys &keys : cases) {
        std::string body = "[cmpdsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\nDatabase=d\n";
        if (keys.sslMode) {
            body += std::string("SSLMode=") + keys.sslMode + "\n";
        }
        if (keys.encryptionMethod) {
            body += std::string("EncryptionMethod=") + keys.encryptionMethod + "\n";
        }
        const int sslModeNamed = keys.sslMode != nullptr;
        const int encryptionMethodNamed = keys.encryptionMethod != nullptr;
        std::string testConnStr = "Driver=Amazon Redshift ODBC Driver";
        if (rs_dsn_ssl_key_is_named(RS_DSN_SSL_MODE_KEY, sslModeNamed, encryptionMethodNamed)) {
            testConnStr += std::string(";SSLMode=") + (keys.sslMode ? keys.sslMode : "verify-full");
        }
        if (rs_dsn_ssl_key_is_named(RS_DSN_ENCRYPTION_METHOD_KEY, sslModeNamed, encryptionMethodNamed)) {
            testConnStr += std::string(";EncryptionMethod=") + (keys.encryptionMethod ? keys.encryptionMethod : "2");
        }
        const std::string label = std::string("SSLMode=") + (keys.sslMode ? keys.sslMode : "<none>") +
                                  " EncryptionMethod=" + (keys.encryptionMethod ? keys.encryptionMethod : "<none>");

        pConn->resetConnectProps();
        readDsn("cmpdsn", body);
        RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
        const std::string savedMode = pConn->pConnectProps->szSslMode;
        const int savedPolicy = pConn->pConnectProps->iSslMismatchPolicy;

        pConn->resetConnectProps();
        pConn->parseConnectString(const_cast<char *>(testConnStr.c_str()), SQL_NTS, FALSE, FALSE);
        RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
        EXPECT_EQ(std::string(pConn->pConnectProps->szSslMode), savedMode)
            << "Test Connection picked a different mode for " << label;
        EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, savedPolicy)
            << "Test Connection picked a different policy for " << label;
    }
}

// ===========================================================================
// TrustStore, CaFile and CaPath read from a DSN
// ===========================================================================
// All three feed libpq's sslrootcert and hold MAX_CERT_PATH_LEN bytes, so a path libpq accepts
// must survive the read whole. One that does not fit is dropped, not shortened, and the option marked.

namespace {

/** @brief Builds a path of a given length that no file occupies. */
std::string certPathOfLength(size_t length) {
#ifdef WIN32
    std::string path = "C:/rsodbc_cert/";
#else
    std::string path = "/tmp/rsodbc_cert/";
#endif
    const std::string suffix = "/root.crt";
    while (path.size() + suffix.size() < length) {
        path += 'a';
    }
    return path + suffix;
}

// The base name of the DSN the certificate cases write; readDsn() makes it unique.
const char *const CERT_DSN = "certdsn";

/** @brief A DSN body naming one certificate option. */
std::string certDsnBody(const char *key, const std::string &value) {
    return std::string("[") + CERT_DSN + "]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
           "Database=d\n" + key + "=" + value + "\n";
}

// The read-back asks for CERT_PATH_READ_LEN, the driver's own read size, so it sees what the
// driver's read path saw; a roomier buffer could pass where the driver's reader fails.

/** @brief What the DSN gives the driver for a key, read back the way the driver reads it. */
std::string dsnCertValueAsStored(const std::string &dsn, const char *key) {
    std::vector<char> buf(CERT_PATH_READ_LEN + 1, '\0');
    RS_SQLGetPrivateProfileString(dsn.c_str(), (char *)key, (char *)"", buf.data(),
                                  (int)buf.size(), (char *)ODBC_INI);
    buf[buf.size() - 1] = '\0';
    return std::string(buf.data());
}

}  // namespace

// Checks that the driver's read path sees the whole value just written. A reader that returns less
// must fail here with the byte counts: it could not tell a too-long value from an empty key, and
// would drop the named CA file and connect unvalidated. Use after readDsn.
#define ASSERT_DSN_HOLDS(dsn, key, value)                                             \
    do {                                                                              \
        const std::string stored = dsnCertValueAsStored((dsn), (key));                \
        ASSERT_EQ(stored, (value))                                                    \
            << "this platform's DSN gave the driver " << stored.size()                \
            << " of the " << (value).size() << " bytes written for " << (key)         \
            << ", so the driver cannot tell a value too long for the option from a "  \
               "key naming nothing and would connect with no validation at all";      \
    } while (0)

// The longest path the option buffers hold, which an earlier boundary refused.
TEST_F(SslModeDsnTest, a_DSN_cert_path_of_the_greatest_fitting_length_is_kept) {
    const std::string longest = certPathOfLength(MAX_CERT_PATH_LEN - 1);
    ASSERT_EQ(longest.size(), (size_t)(MAX_CERT_PATH_LEN - 1));

    // A mark left over from an earlier read must not outlive the value it belonged
    // to, or the connection would be failed over a value no longer in use.
    CertPathStatus stale;
    stale.overlong = true;
    stale.requiredLength = 4096;
    pConn->pConnectProps->certPathStatusOf(CertPathOption::TrustStore) = stale;

    readDsn(CERT_DSN, certDsnBody(RS_TRUST_STORE, longest));

    EXPECT_STREQ(pConn->pConnectProps->szTrustStore, longest.c_str())
        << "a path of exactly the length the buffer holds must be kept";
    EXPECT_EQ(pConn->pConnectProps->certPathStatusOf(CertPathOption::TrustStore).overlong, false)
        << "a path that fits must not be recorded as over-long";
    EXPECT_FALSE(pConn->pConnectProps->certPathStatusOf(CertPathOption::TrustStore).requiredLength.has_value())
        << "a kept value must take the earlier mark's length with it";
}

// One byte more than the option holds cannot be kept, as a shortened path names another file.
TEST_F(SslModeDsnTest, a_DSN_cert_path_one_byte_past_the_option_is_dropped) {
    const std::string tooLong = certPathOfLength(MAX_CERT_PATH_LEN);
    ASSERT_EQ(tooLong.size(), (size_t)MAX_CERT_PATH_LEN);

    readDsn(CERT_DSN, certDsnBody(RS_TRUST_STORE, tooLong));
    ASSERT_DSN_HOLDS(uniqueDsn(CERT_DSN), RS_TRUST_STORE, tooLong);

    EXPECT_STREQ(pConn->pConnectProps->szTrustStore, "")
        << "a value that does not fit must not be kept shortened";
    EXPECT_EQ(pConn->pConnectProps->certPathStatusOf(CertPathOption::TrustStore).overlong, true)
        << "the option must be marked so the connection can fail closed on it";
    // The value arrived whole, so its length is one the diagnostic can name.
    EXPECT_EQ(pConn->pConnectProps->certPathStatusOf(CertPathOption::TrustStore).requiredLength,
              std::optional<size_t>(MAX_CERT_PATH_LEN));
}

// Well past the option but short of the read buffer, so the value arrives whole and is refused for its length.
TEST_F(SslModeDsnTest, a_DSN_cert_path_far_past_the_option_is_dropped) {
    const std::string tooLong = certPathOfLength(MAX_CERT_PATH_LEN * 2);
    ASSERT_EQ(tooLong.size(), (size_t)(MAX_CERT_PATH_LEN * 2));

    readDsn(CERT_DSN, certDsnBody(RS_TRUST_STORE, tooLong));
    ASSERT_DSN_HOLDS(uniqueDsn(CERT_DSN), RS_TRUST_STORE, tooLong);

    EXPECT_STREQ(pConn->pConnectProps->szTrustStore, "")
        << "a value too long for the option must not be kept shortened, and must not "
           "be read as naming nothing";
    EXPECT_EQ(pConn->pConnectProps->certPathStatusOf(CertPathOption::TrustStore).overlong, true)
        << "the option must be marked so the connection can fail closed on it";
    EXPECT_EQ(pConn->pConnectProps->certPathStatusOf(CertPathOption::TrustStore).requiredLength,
              std::optional<size_t>(MAX_CERT_PATH_LEN * 2))
        << "a value well short of the buffer it was read into arrived whole, so the "
           "diagnostic can report how long it really was";
}

// Thousands of bytes past the option but inside the read buffer, so the value arrives whole and is
// refused with its length named, not dropped silently as a key naming nothing.
TEST_F(SslModeDsnTest, a_DSN_cert_path_thousands_of_bytes_past_the_option_is_dropped) {
    const std::string tooLong = certPathOfLength(5000);
    ASSERT_EQ(tooLong.size(), (size_t)5000);

    readDsn(CERT_DSN, certDsnBody(RS_TRUST_STORE, tooLong));
    ASSERT_DSN_HOLDS(uniqueDsn(CERT_DSN), RS_TRUST_STORE, tooLong);

    EXPECT_STREQ(pConn->pConnectProps->szTrustStore, "")
        << "a value that does not fit must not be kept shortened";
    EXPECT_EQ(pConn->pConnectProps->certPathStatusOf(CertPathOption::TrustStore).overlong, true)
        << "the option must be marked so the connection can fail closed on it";
    EXPECT_EQ(pConn->pConnectProps->certPathStatusOf(CertPathOption::TrustStore).requiredLength,
              std::optional<size_t>(5000))
        << "the value arrived whole, so the diagnostic can report how long it really "
           "was";
}

TEST_F(SslModeDsnTest, a_dropped_DSN_cert_path_marks_only_its_own_option) {
    const std::string tooLong = certPathOfLength(MAX_CERT_PATH_LEN + 200);

    readDsn(CERT_DSN, certDsnBody(RS_IAM_CA_FILE, tooLong));
    ASSERT_DSN_HOLDS(uniqueDsn(CERT_DSN), RS_IAM_CA_FILE, tooLong);

    EXPECT_EQ(pConn->pConnectProps->certPathStatusOf(CertPathOption::CaFile).overlong, true);
    EXPECT_EQ(pConn->pConnectProps->certPathStatusOf(CertPathOption::CaFile).requiredLength,
              std::optional<size_t>(tooLong.size()))
        << "the option that was marked must carry the length its value really had";
    EXPECT_EQ(pConn->pConnectProps->certPathStatusOf(CertPathOption::TrustStore).overlong, false)
        << "TrustStore named nothing, so it must not be marked";
    EXPECT_EQ(pConn->pConnectProps->certPathStatusOf(CertPathOption::CaPath).overlong, false)
        << "CaPath named nothing, so it must not be marked";
}

// A DSN naming none of the three options must leave them empty and unmarked, so a missing key is
// never read as one too long to keep.
TEST_F(SslModeDsnTest, a_DSN_naming_no_cert_path_marks_nothing) {
    readDsn(CERT_DSN, std::string("[") + CERT_DSN +
                          "]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\nDatabase=d\n");

    EXPECT_STREQ(pConn->pConnectProps->szTrustStore, "")
        << "an absent key must leave the option empty";
    EXPECT_STREQ(pConn->pConnectProps->szCaFile, "")
        << "an absent key must leave the option empty";
    EXPECT_STREQ(pConn->pConnectProps->szCaPath, "")
        << "an absent key must leave the option empty";
    EXPECT_EQ(pConn->pConnectProps->certPathStatusOf(CertPathOption::TrustStore).overlong, false)
        << "an absent key must not be read as a value too long to keep";
    EXPECT_EQ(pConn->pConnectProps->certPathStatusOf(CertPathOption::CaFile).overlong, false)
        << "an absent key must not be read as a value too long to keep";
    EXPECT_EQ(pConn->pConnectProps->certPathStatusOf(CertPathOption::CaPath).overlong, false)
        << "an absent key must not be read as a value too long to keep";
}

// A key holding nothing (what the dialog writes for a blank option) must read the same as no key.
TEST_F(SslModeDsnTest, a_DSN_cert_path_holding_nothing_marks_nothing) {
    readDsn(CERT_DSN, certDsnBody(RS_TRUST_STORE, ""));

    EXPECT_STREQ(pConn->pConnectProps->szTrustStore, "")
        << "a key holding nothing must leave the option empty";
    EXPECT_EQ(pConn->pConnectProps->certPathStatusOf(CertPathOption::TrustStore).overlong, false)
        << "a key holding nothing names no path, so it must not be marked";
}

namespace {

/** One DSN certificate option: its key, its status slot and its destination buffer. */
struct CertDsnOption {
    const char *key;
    CertPathOption slot;
    char *(*destination)(RS_CONNECT_PROPS_INFO *props);
};

/** How the DSN holds the option's value. */
enum class CertDsnShape { Fits, AtTheBoundary, OverLong, Blank, Absent };

const CertDsnOption kCertDsnOptions[] = {
    {RS_TRUST_STORE, CertPathOption::TrustStore,
     [](RS_CONNECT_PROPS_INFO *props) { return props->szTrustStore; }},
    {RS_IAM_CA_FILE, CertPathOption::CaFile,
     [](RS_CONNECT_PROPS_INFO *props) { return props->szCaFile; }},
    {RS_IAM_CA_PATH, CertPathOption::CaPath,
     [](RS_CONNECT_PROPS_INFO *props) { return props->szCaPath; }},
};

const char *certDsnShapeName(CertDsnShape shape) {
    switch (shape) {
        case CertDsnShape::Fits:          return "fits";
        case CertDsnShape::AtTheBoundary: return "at_the_boundary";
        case CertDsnShape::OverLong:      return "over_long";
        case CertDsnShape::Blank:         return "blank";
        default:                          return "absent";
    }
}

class SslCertPathDsnTest
    : public SslModeDsnTest,
      public ::testing::WithParamInterface<std::tuple<CertDsnOption, CertDsnShape>> {};

}  // namespace

// Each of the three DSN reads must use its own key, buffer and status slot, so every shape
// lands in that option alone.
TEST_P(SslCertPathDsnTest, lands_in_its_own_option) {
    const CertDsnOption option = std::get<0>(GetParam());
    const CertDsnShape shape = std::get<1>(GetParam());
    std::string value;
    if (shape == CertDsnShape::Fits) {
        value = certPathOfLength(120);
    } else if (shape == CertDsnShape::AtTheBoundary) {
        value = certPathOfLength(MAX_CERT_PATH_LEN - 1);
    } else if (shape == CertDsnShape::OverLong) {
        value = certPathOfLength(MAX_CERT_PATH_LEN);
    }

    // Step 1: write the DSN with the key, or without it for Absent.
    std::string body = std::string("[") + CERT_DSN +
                       "]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\nDatabase=d\n";
    if (shape != CertDsnShape::Absent) {
        body += std::string(option.key) + "=" + value + "\n";
    }
    readDsn(CERT_DSN, body);
    RS_CONNECT_PROPS_INFO *props = pConn->pConnectProps;

    // Step 2: the option's own buffer and slot.
    const bool overLong = shape == CertDsnShape::OverLong;
    EXPECT_STREQ(option.destination(props), overLong ? "" : value.c_str()) << option.key;
    EXPECT_EQ(props->certPathStatusOf(option.slot).overlong, overLong) << option.key;
    if (overLong) {
        EXPECT_EQ(props->certPathStatusOf(option.slot).requiredLength,
                  std::optional<size_t>(value.size()))
            << option.key;
    }

    // Step 3: the other two options are untouched.
    for (const CertDsnOption &other : kCertDsnOptions) {
        if (other.slot == option.slot) {
            continue;
        }
        EXPECT_STREQ(other.destination(props), "") << other.key << " after " << option.key;
        EXPECT_FALSE(props->certPathStatusOf(other.slot).overlong)
            << other.key << " after " << option.key;
    }
}

INSTANTIATE_TEST_SUITE_P(
    SslModeDsn, SslCertPathDsnTest,
    ::testing::Combine(::testing::ValuesIn(kCertDsnOptions),
                       ::testing::Values(CertDsnShape::Fits, CertDsnShape::AtTheBoundary,
                                         CertDsnShape::OverLong, CertDsnShape::Blank,
                                         CertDsnShape::Absent)),
    [](const ::testing::TestParamInfo<std::tuple<CertDsnOption, CertDsnShape>> &info) {
        return std::string(std::get<0>(info.param).key) + "_" +
               certDsnShapeName(std::get<1>(info.param));
    });

// A connection string value that fits must clear the mark a dropped DSN value left, or the connection
// would fail over a value no longer in use. The drop is established first so the case proves something.
TEST_F(SslModeDsnTest, a_connection_string_cert_path_clears_a_dropped_DSN_value) {
    const std::string tooLong = certPathOfLength(MAX_CERT_PATH_LEN + 200);

    readDsn(CERT_DSN, certDsnBody(RS_TRUST_STORE, tooLong));
    ASSERT_DSN_HOLDS(uniqueDsn(CERT_DSN), RS_TRUST_STORE, tooLong);
    ASSERT_EQ(pConn->pConnectProps->certPathStatusOf(CertPathOption::TrustStore).overlong, true)
        << "the DSN value must have been dropped for this case to mean anything";

    // The order SQLDriverConnect and SQLBrowseConnect use: DSN read between a DSN-only pass and the full pass.
    const std::string valid = certPathOfLength(120);
    const std::string connStr = std::string("TrustStore=") + valid;
    pConn->parseConnectString(const_cast<char *>(connStr.c_str()), SQL_NTS, TRUE,
                              FALSE);

    EXPECT_STREQ(pConn->pConnectProps->szTrustStore, valid.c_str())
        << "the connection string overrides the DSN";
    EXPECT_EQ(pConn->pConnectProps->certPathStatusOf(CertPathOption::TrustStore).overlong, false)
        << "the mark the DSN left must go with the value it belonged to";
}

// The identity-provider requests read CaFile and CaPath from the IAM properties before the database
// connection looks at either. A value too long to keep must not replace what is there, or the
// credential exchange would use a CA file the caller did not name.
TEST_F(SslModeDsnTest, a_dropped_ca_file_keeps_the_iam_ca_file_it_had) {
    const std::string installed = certPathOfLength(120);
    const std::string tooLong = certPathOfLength(MAX_CERT_PATH_LEN + 76);

    const std::string first = std::string(RS_IAM_CA_FILE) + "=" + installed;
    pConn->parseConnectString(const_cast<char *>(first.c_str()), SQL_NTS, FALSE, FALSE);
    ASSERT_STREQ(pConn->pConnectProps->pIamProps->szCaFile, installed.c_str())
        << "the CA file the identity-provider request starts from was never installed, so "
           "this case would pass over an empty value either way";

    const std::string second = std::string(RS_IAM_CA_FILE) + "=" + tooLong;
    pConn->parseConnectString(const_cast<char *>(second.c_str()), SQL_NTS, TRUE, FALSE);

    EXPECT_STREQ(pConn->pConnectProps->szCaFile, "")
        << "a value that does not fit must not be kept shortened";
    EXPECT_EQ(pConn->pConnectProps->certPathStatusOf(CertPathOption::CaFile).overlong, true)
        << "the option must be marked so the connection can fail closed on it";
    EXPECT_STREQ(pConn->pConnectProps->pIamProps->szCaFile, installed.c_str())
        << "the dropped value must not have replaced the identity-provider request's "
           "CA file with nothing";
}

TEST_F(SslModeDsnTest, a_dropped_ca_path_keeps_the_iam_ca_file_it_had) {
    const std::string installed = certPathOfLength(120);
    const std::string tooLong = certPathOfLength(MAX_CERT_PATH_LEN + 76);

    const std::string first = std::string(RS_IAM_CA_PATH) + "=" + installed;
    pConn->parseConnectString(const_cast<char *>(first.c_str()), SQL_NTS, FALSE, FALSE);
    ASSERT_STREQ(pConn->pConnectProps->pIamProps->szCaPath, installed.c_str())
        << "the CA file the identity-provider request starts from was never installed, so "
           "this case would pass over an empty value either way";

    const std::string second = std::string(RS_IAM_CA_PATH) + "=" + tooLong;
    pConn->parseConnectString(const_cast<char *>(second.c_str()), SQL_NTS, TRUE, FALSE);

    EXPECT_STREQ(pConn->pConnectProps->szCaPath, "")
        << "a value that does not fit must not be kept shortened";
    EXPECT_EQ(pConn->pConnectProps->certPathStatusOf(CertPathOption::CaPath).overlong, true)
        << "the option must be marked so the connection can fail closed on it";
    EXPECT_STREQ(pConn->pConnectProps->pIamProps->szCaPath, installed.c_str())
        << "the dropped value must not have replaced the identity-provider request's "
           "CA file with nothing";
}

// ===========================================================================
// The IAM props across a reconnect on one handle
// ===========================================================================

namespace {
/** Reads a DSN twice on one connection, reusing SslModeDsnTest's DSN machinery. */
class IamPropsReconnectTest : public SslModeDsnTest {};
}  // namespace

// Replacing the props object would abandon the first one and its pszJwt.
TEST_F(IamPropsReconnectTest, a_second_dsn_read_keeps_the_same_iam_props) {
    readDsn("iamreconnect",
            "[iamreconnect]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
            "Database=d\nIAM=1\nClusterId=first\n");
    RS_IAM_CONN_PROPS_INFO *const first = pConn->pConnectProps->pIamProps;
    ASSERT_NE(first, (RS_IAM_CONN_PROPS_INFO *)NULL)
        << "the DSN names IAM, so the props must have been read";

    pConn->readMoreConnectPropsFromRegistry(FALSE);

    EXPECT_EQ(pConn->pConnectProps->pIamProps, first)
        << "a reconnect must reuse the props object, not allocate a second one";
}

// Reusing without clearing would keep a key the new DSN leaves unset. pszJwt is the one
// that carries a credential.
TEST_F(IamPropsReconnectTest, a_second_dsn_read_drops_the_previous_web_identity_token) {
    readDsn("jwtreconnect",
            "[jwtreconnect]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
            "Database=d\nIAM=1\nplugin_name=JWT\nweb_identity_token=first-token\n");
    ASSERT_NE(pConn->pConnectProps->pIamProps, (RS_IAM_CONN_PROPS_INFO *)NULL);
    ASSERT_STREQ(pConn->pConnectProps->pIamProps->pszJwt, "first-token")
        << "without a token to begin with this case proves nothing";

    readDsn("plainreconnect",
            "[plainreconnect]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
            "Database=d\nIAM=1\nClusterId=second\n");

    EXPECT_EQ(pConn->pConnectProps->pIamProps->pszJwt, (char *)NULL)
        << "the new DSN names no token, so the previous one must not still be held";
    EXPECT_STREQ(pConn->pConnectProps->pIamProps->szClusterId, "second")
        << "the new DSN's values must have been read into the reused object";
}

// ===========================================================================
// storeCertPathOrRecordOverflow and resetCertPathState, without a DSN
// ===========================================================================
namespace {
/** @brief Asserts a status is the default one: not overlong, no length. */
void expectDefaultStatus(const CertPathStatus &status) {
    EXPECT_FALSE(status.overlong);
    EXPECT_FALSE(status.requiredLength.has_value());
}
}  // namespace
TEST(DEFAULT_VALUE_TEST_SUITE, store_cert_path_with_capacity_zero_keeps_nothing) {
    char buffer[4] = {'a', 'b', 'c', '\0'};
    const CertPathStatus status = storeCertPathOrRecordOverflow(buffer, 0, "x");
    EXPECT_TRUE(status.overlong);
    EXPECT_EQ(status.requiredLength, std::optional<size_t>(1));
    EXPECT_STREQ(buffer, "abc") << "a zero-size destination must not be written";
}
TEST(DEFAULT_VALUE_TEST_SUITE, store_cert_path_that_exactly_fits_is_kept) {
    char buffer[16];
    memset(buffer, 'z', sizeof(buffer));
    const std::string value(sizeof(buffer) - 1, 'p');
    const CertPathStatus status =
        storeCertPathOrRecordOverflow(buffer, sizeof(buffer), value.c_str());
    expectDefaultStatus(status);
    EXPECT_STREQ(buffer, value.c_str());
}
TEST(DEFAULT_VALUE_TEST_SUITE, store_cert_path_one_byte_over_is_dropped_with_its_length) {
    char buffer[16];
    memset(buffer, 'z', sizeof(buffer));
    const std::string value(sizeof(buffer), 'p');
    const CertPathStatus status =
        storeCertPathOrRecordOverflow(buffer, sizeof(buffer), value.c_str());
    EXPECT_TRUE(status.overlong);
    EXPECT_EQ(status.requiredLength, std::optional<size_t>(sizeof(buffer)));
    EXPECT_STREQ(buffer, "") << "a value that does not fit must not be kept shortened";
}
TEST(DEFAULT_VALUE_TEST_SUITE, store_cert_path_far_over_is_dropped_with_its_length) {
    char buffer[MAX_CERT_PATH_LEN];
    memset(buffer, 'z', sizeof(buffer));
    const std::string value(5000, 'p');
    const CertPathStatus status =
        storeCertPathOrRecordOverflow(buffer, sizeof(buffer), value.c_str());
    EXPECT_TRUE(status.overlong);
    EXPECT_EQ(status.requiredLength, std::optional<size_t>(5000));
    EXPECT_STREQ(buffer, "");
}
TEST(DEFAULT_VALUE_TEST_SUITE, store_cert_path_null_value_clears_the_destination) {
    char buffer[16] = "stale";
    expectDefaultStatus(storeCertPathOrRecordOverflow(buffer, sizeof(buffer), NULL));
    EXPECT_STREQ(buffer, "");
}
TEST(DEFAULT_VALUE_TEST_SUITE, store_cert_path_empty_value_clears_the_destination) {
    char buffer[16] = "stale";
    expectDefaultStatus(storeCertPathOrRecordOverflow(buffer, sizeof(buffer), ""));
    EXPECT_STREQ(buffer, "");
}
TEST(DEFAULT_VALUE_TEST_SUITE, store_cert_path_that_fits_returns_the_default_status_so_assignment_clears_an_overflow) {
    char buffer[16];
    CertPathStatus slot = storeCertPathOrRecordOverflow(
        buffer, sizeof(buffer), std::string(40, 'p').c_str());
    ASSERT_TRUE(slot.overlong) << "the overflow must have been recorded first";
    ASSERT_TRUE(slot.requiredLength.has_value());
    slot = storeCertPathOrRecordOverflow(buffer, sizeof(buffer), "short");
    expectDefaultStatus(slot);
    EXPECT_STREQ(buffer, "short");
}
TEST(DEFAULT_VALUE_TEST_SUITE, reset_cert_path_state_clears_the_three_buffers_and_their_status) {
    RS_CONNECT_PROPS_INFO obj;
    strcpy(obj.szTrustStore, "/trust");
    strcpy(obj.szCaFile, "/ca/file");
    strcpy(obj.szCaPath, "/ca/path");
    for (CertPathStatus &status : obj.certPathStatus) {
        status.overlong = true;
        status.requiredLength = 4096;
    }
    obj.resetCertPathState();
    EXPECT_STREQ(obj.szTrustStore, "");
    EXPECT_STREQ(obj.szCaFile, "");
    EXPECT_STREQ(obj.szCaPath, "");
    for (const CertPathStatus &status : obj.certPathStatus) {
        expectDefaultStatus(status);
    }
}
#ifdef WIN32
// ===========================================================================
// ConfigDSN: what a save leaves in the DSN
// ===========================================================================
// These drive rsodbcsetup.dll's ConfigDSN and read the saved DSN back from the registry, so they
// assert what persisted, not what a helper returns. With a NULL parent window ConfigDSN is
// non-interactive and cannot stop on a dialog; the ctest timeout is the only backstop if one appears.
// Not covered: the SSL tab rebuild on an auth-type switch, Test Connection and Cancel (need UI automation).

namespace {

/** @brief rsodbcsetup.dll's ConfigDSN; CALLBACK is the __stdcall setup.c declares. */
typedef BOOL(CALLBACK *rsConfigDsnFn)(HWND, WORD, LPCSTR, LPCSTR);

// Loaded from beside this binary, where the build copies it; no installed driver is needed.
const char *const RS_SETUP_DLL_NAME = "rsodbcsetup.dll";

// The driver DLL beside this binary, named in a temporary driver registration.
const char *const RS_TEST_DRIVER_DLL_NAME = "rsodbc64-test.dll";

// Serialises runs, which share the machine-wide values below and the driver registration.
const char *const RS_CONFIG_DSN_MUTEX = "Local\\RedshiftOdbcConfigDsnPersistenceTest";

// The installer section that lists registered drivers.
const char *const RS_ODBC_DRIVERS = "ODBC Drivers";

// ConfigDSN returns FALSE for any other description (case-insensitive exact match).
const char *const RS_SETUP_DRIVER_DESC = "Amazon Redshift ODBC Driver (x64)";

// Tells a key the DSN lacks from one it holds empty: a save writes every attribute.
const char *const RS_DSN_KEY_ABSENT = "<absent>";

// The section SQLWriteDSNToIni adds the DSN name to, read back to check that
// removal left no entry behind.
const char *const RS_ODBC_DATA_SOURCES = "ODBC Data Sources";

// The machine-wide values ConfigDSN writes under TRACE_KEY_NAME on every save. The fixture
// snapshots and restores them so a run does not change the driver's logging configuration.
const char *const RS_GLOBAL_DRIVER_VALUES[] = {"LogLevel", "LogPath"};

/** @brief One machine-wide registry value as it stood before the tests ran. */
struct GlobalValueSnapshot {
    std::string name;
    bool present = false;
    DWORD type = REG_SZ;
    std::vector<BYTE> data;
};

/** @brief One DSN shape to hand to ConfigDSN. A NULL key ends the attribute list. */
struct ConfigDsnShape {
    const char *tag;       /* short, unique part of the DSN name */
    const char *authType;  /* NULL when the DSN names no AuthType */
    const char *key1;      /* a further attribute, or NULL */
    const char *val1;
    const char *key2;      /* a second further attribute, or NULL */
    const char *val2;
    const char *requestedSslMode;
    const char *persistedSslMode;
};

} // namespace

class ConfigDsnPersistenceTest : public ::testing::Test {
  protected:
    HMODULE setupDll = NULL;
    rsConfigDsnFn configDsn = NULL;
    HANDLE runMutex = NULL;
    bool holdsRunMutex = false;
    // Set when SetUp registered the driver itself, so TearDown removes the registration.
    bool registeredDriverHere = false;
    std::vector<std::string> dsnsToRemove;
    std::vector<GlobalValueSnapshot> globalSnapshot;
    // The machine-wide values as the last ConfigDSN call left them; restored only if unchanged.
    std::vector<GlobalValueSnapshot> globalAfterWrite;
    bool globalWritten = false;
    bool globalKeyExisted = false;
    // Whether SetUp reached the snapshot. TearDown runs even after a failed SetUp, and an
    // empty snapshot must not read as "the key was not there".
    bool globalSnapshotTaken = false;

    /** @brief The directory this test binary is in, with a trailing separator. */
    static std::string binaryDirectory() {
        char path[MAX_PATH];
        const DWORD len = GetModuleFileNameA(NULL, path, (DWORD)sizeof(path));
        if (len == 0 || len >= sizeof(path)) {
            return std::string();
        }
        std::string dir(path, len);
        const size_t slash = dir.find_last_of("\\/");
        return slash == std::string::npos ? std::string() : dir.substr(0, slash + 1);
    }

    /**
     * Takes the run lock, loads the setup DLL from beside this binary, makes sure the driver is
     * registered and snapshots the machine-wide values a save overwrites. A broken test package
     * fails here rather than skipping.
     */
    void SetUp() override {
        runMutex = CreateMutexA(NULL, FALSE, RS_CONFIG_DSN_MUTEX);
        ASSERT_NE(runMutex, (HANDLE)NULL)
            << "CreateMutexA failed with error " << GetLastError();
        const DWORD waited = WaitForSingleObject(runMutex, 5 * 60 * 1000);
        ASSERT_TRUE(waited == WAIT_OBJECT_0 || waited == WAIT_ABANDONED)
            << "another run held " << RS_CONFIG_DSN_MUTEX << " for five minutes";
        holdsRunMutex = true;

        const std::string dir = binaryDirectory();
        ASSERT_FALSE(dir.empty()) << "could not find the directory of this test binary";
        const std::string setupPath = dir + RS_SETUP_DLL_NAME;
        setupDll = LoadLibraryExA(setupPath.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        ASSERT_NE(setupDll, (HMODULE)NULL)
            << "LoadLibraryExA(\"" << setupPath << "\") failed with error " << GetLastError()
            << "; the build copies that DLL beside this binary, so the test package is broken";
        configDsn =
            reinterpret_cast<rsConfigDsnFn>(GetProcAddress(setupDll, "ConfigDSN"));
        ASSERT_NE(configDsn, (rsConfigDsnFn)NULL)
            << setupPath << " exports no ConfigDSN; GetProcAddress failed with error "
            << GetLastError();

        // rs_dsn_write_attrs resolves the driver description through ODBCINST.INI. With no
        // registration, add a temporary one; failing to is a failure, not a skip.
        char registeredDriver[MAX_PATH];
        registeredDriver[0] = '\0';
        SQLGetPrivateProfileString(RS_SETUP_DRIVER_DESC, "Driver", "",
                                   registeredDriver, (int)sizeof(registeredDriver),
                                   "ODBCINST.INI");
        if (registeredDriver[0] == '\0') {
            const std::string driverPath = dir + RS_TEST_DRIVER_DLL_NAME;
            registeredDriverHere = true;
            ASSERT_TRUE(SQLWritePrivateProfileString(RS_ODBC_DRIVERS, RS_SETUP_DRIVER_DESC,
                                                     "Installed", "ODBCINST.INI") &&
                        SQLWritePrivateProfileString(RS_SETUP_DRIVER_DESC, "Driver",
                                                     driverPath.c_str(), "ODBCINST.INI") &&
                        SQLWritePrivateProfileString(RS_SETUP_DRIVER_DESC, "Setup",
                                                     setupPath.c_str(), "ODBCINST.INI"))
                << "could not register \"" << RS_SETUP_DRIVER_DESC
                << "\" for this run, which usually needs administrator rights:"
                << installerErrors();
        }
        // Last, after every check above. The flag tells TearDown the snapshot is real.
        snapshotGlobalDriverValues();
        globalSnapshotTaken = true;
    }

    /** @brief Removes the temporary driver registration SetUp added. */
    void removeTemporaryDriverRegistration() {
        if (!registeredDriverHere) {
            return;
        }
        SQLWritePrivateProfileString(RS_SETUP_DRIVER_DESC, NULL, NULL, "ODBCINST.INI");
        SQLWritePrivateProfileString(RS_ODBC_DRIVERS, RS_SETUP_DRIVER_DESC, NULL,
                                     "ODBCINST.INI");
        char left[MAX_PATH];
        left[0] = '\0';
        SQLGetPrivateProfileString(RS_SETUP_DRIVER_DESC, "Driver", "", left,
                                   (int)sizeof(left), "ODBCINST.INI");
        EXPECT_EQ(left[0], '\0')
            << "the temporary registration of \"" << RS_SETUP_DRIVER_DESC
            << "\" was not removed";
        registeredDriverHere = false;
    }

    /** @brief Reads the machine-wide values as they are now. */
    static std::vector<GlobalValueSnapshot> readGlobalDriverValues() {
        std::vector<GlobalValueSnapshot> values;
        HKEY key = NULL;
        const bool opened =
            RegOpenKeyExA(HKEY_LOCAL_MACHINE, TRACE_KEY_NAME, 0, KEY_READ, &key) ==
            ERROR_SUCCESS;
        for (const char *name : RS_GLOBAL_DRIVER_VALUES) {
            GlobalValueSnapshot shot;
            shot.name = name;
            DWORD size = 0;
            DWORD type = REG_SZ;
            if (opened &&
                RegQueryValueExA(key, name, NULL, &type, NULL, &size) == ERROR_SUCCESS) {
                shot.data.resize(size);
                if (RegQueryValueExA(key, name, NULL, &type,
                                     size ? &shot.data[0] : NULL, &size) == ERROR_SUCCESS) {
                    shot.data.resize(size);
                    shot.present = true;
                    shot.type = type;
                }
            }
            values.push_back(shot);
        }
        if (opened) {
            RegCloseKey(key);
        }
        return values;
    }

    /** @brief Whether two readings of one value are the same. */
    static bool sameValue(const GlobalValueSnapshot &a, const GlobalValueSnapshot &b) {
        return a.present == b.present && (!a.present || (a.type == b.type && a.data == b.data));
    }

    /** Records the machine-wide LogLevel and LogPath, and whether their key existed, so TearDown can restore them. */
    void snapshotGlobalDriverValues() {
        globalSnapshot.clear();
        globalKeyExisted = false;

        HKEY key = NULL;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, TRACE_KEY_NAME, 0, KEY_READ, &key) !=
            ERROR_SUCCESS) {
            // No key yet, so a save will create it. Record both names as absent;
            // TearDown walks the snapshot, so an empty one would delete nothing.
            for (const char *name : RS_GLOBAL_DRIVER_VALUES) {
                GlobalValueSnapshot shot;
                shot.name = name;
                globalSnapshot.push_back(shot);
            }
            return;
        }
        globalKeyExisted = true;
        for (const char *name : RS_GLOBAL_DRIVER_VALUES) {
            GlobalValueSnapshot shot;
            shot.name = name;
            DWORD size = 0;
            DWORD type = REG_SZ;
            if (RegQueryValueExA(key, name, NULL, &type, NULL, &size) ==
                ERROR_SUCCESS) {
                shot.data.resize(size);
                if (RegQueryValueExA(key, name, NULL, &type,
                                     size ? &shot.data[0] : NULL,
                                     &size) == ERROR_SUCCESS) {
                    shot.data.resize(size);
                    shot.present = true;
                    shot.type = type;
                }
            }
            globalSnapshot.push_back(shot);
        }
        RegCloseKey(key);
    }

    /** Puts the machine-wide values back, deleting those that were absent, and checks each. */
    void restoreGlobalDriverValues() {
        // Nothing was written, so there is nothing to put back or delete.
        if (!globalSnapshotTaken || !globalWritten) {
            return;
        }

        // A value another process changed since this test's last save is left alone.
        const std::vector<GlobalValueSnapshot> now = readGlobalDriverValues();
        std::vector<GlobalValueSnapshot> toRestore;
        for (size_t i = 0; i < globalSnapshot.size(); i++) {
            if (sameValue(now[i], globalAfterWrite[i])) {
                toRestore.push_back(globalSnapshot[i]);
            } else {
                fprintf(stderr, "WARNING: the machine-wide %s changed after this test wrote "
                                "it, so it is left as it is\n", globalSnapshot[i].name.c_str());
            }
        }

        HKEY key = NULL;
        const LONG opened = RegOpenKeyExA(HKEY_LOCAL_MACHINE, TRACE_KEY_NAME, 0,
                                          KEY_READ | KEY_SET_VALUE, &key);
        if (opened != ERROR_SUCCESS) {
            // Only a problem if there was something to restore. A key that never
            // existed and still does not is the clean outcome.
            EXPECT_FALSE(globalKeyExisted)
                << "the machine-wide driver key existed before these tests but "
                   "cannot be reopened to restore LogLevel and LogPath; "
                   "RegOpenKeyExA failed with " << opened
                << (opened == ERROR_ACCESS_DENIED
                        ? ", which is access denied: run these tests elevated, because "
                          "restoring a machine-wide value needs administrator rights"
                        : "");
            return;
        }
        for (const GlobalValueSnapshot &shot : toRestore) {
            if (shot.present) {
                const LONG set = RegSetValueExA(
                    key, shot.name.c_str(), 0, shot.type,
                    shot.data.empty() ? NULL : &shot.data[0],
                    (DWORD)shot.data.size());
                EXPECT_EQ(set, ERROR_SUCCESS)
                    << "failed to restore the machine-wide " << shot.name;
            } else {
                const LONG removed = RegDeleteValueA(key, shot.name.c_str());
                EXPECT_TRUE(removed == ERROR_SUCCESS ||
                            removed == ERROR_FILE_NOT_FOUND)
                    << "failed to remove the machine-wide " << shot.name
                    << ", which these tests added; RegDeleteValueA returned "
                    << removed;
            }
        }
        // Read back, so a restore that reported success but did not take is caught.
        for (const GlobalValueSnapshot &shot : toRestore) {
            DWORD size = 0;
            const LONG found =
                RegQueryValueExA(key, shot.name.c_str(), NULL, NULL, NULL, &size);
            if (shot.present) {
                EXPECT_EQ(found, ERROR_SUCCESS)
                    << "the machine-wide " << shot.name << " is missing after restore";
                EXPECT_EQ((size_t)size, shot.data.size())
                    << "the machine-wide " << shot.name
                    << " did not come back at its original size";
            } else {
                EXPECT_EQ(found, ERROR_FILE_NOT_FOUND)
                    << "the machine-wide " << shot.name
                    << " is still present after these tests, which added it";
            }
        }
        RegCloseKey(key);

        // A save created the key, so remove it. Only this leaf, and it refuses to go
        // if a save left anything else under it.
        if (!globalKeyExisted && toRestore.size() == globalSnapshot.size()) {
            const LONG removed = RegDeleteKeyA(HKEY_LOCAL_MACHINE, TRACE_KEY_NAME);
            EXPECT_TRUE(removed == ERROR_SUCCESS || removed == ERROR_FILE_NOT_FOUND)
                << "failed to remove the machine-wide driver key these tests "
                   "created; RegDeleteKeyA returned " << removed;
        }
    }

    /** @brief Whether the DSN still has a section or a Data Sources entry. */
    static bool dsnHasResidue(const std::string &dsn, std::string &what) {
        what.clear();
        char buf[1024];

        buf[0] = '\0';
        SQLGetPrivateProfileString(RS_ODBC_DATA_SOURCES, dsn.c_str(), "", buf,
                                   (int)sizeof(buf), ODBC_INI);
        if (buf[0] != '\0') {
            what += std::string(" [") + RS_ODBC_DATA_SOURCES + "] entry=\"" + buf +
                    "\"";
        }
        // Driver is the one key every written DSN carries, so its absence is what
        // tells an emptied section from a surviving one.
        buf[0] = '\0';
        SQLGetPrivateProfileString(dsn.c_str(), "Driver", "", buf,
                                   (int)sizeof(buf), ODBC_INI);
        if (buf[0] != '\0') {
            what += " section Driver=\"" + std::string(buf) + "\"";
        }
        return !what.empty();
    }

    /**
     * Removes every DSN the test named, restores the machine-wide LogLevel and LogPath and unloads
     * the DLL, unconditionally so a failing body leaves nothing behind. Each removal is checked.
     */
    void TearDown() override {
        for (const std::string &dsn : dsnsToRemove) {
            std::string before;
            const bool existed = dsnHasResidue(dsn, before);

            // Both are attempted whatever either returns: the first drops the name from [ODBC Data Sources],
            // the second the DSN's own section, which is all a hand-written DSN has.
            const BOOL removedFromIni = SQLRemoveDSNFromIni(dsn.c_str());
            const BOOL removedSection =
                SQLWritePrivateProfileString(dsn.c_str(), NULL, NULL, ODBC_INI);

            // A DSN ConfigDSN declined to write was never there, so neither result means anything. Only a
            // DSN that existed is held to both.
            if (existed) {
                EXPECT_TRUE(removedFromIni)
                    << "SQLRemoveDSNFromIni failed for " << dsn << ":"
                    << installerErrors();
                EXPECT_TRUE(removedSection)
                    << "SQLWritePrivateProfileString failed to remove the section "
                       "for " << dsn << ":" << installerErrors();
            }

            // Read back either way: a removal that reported success but left the
            // DSN behind would otherwise contaminate every later test.
            std::string residue;
            EXPECT_FALSE(dsnHasResidue(dsn, residue))
                << "cleanup left " << dsn << " behind:" << residue;
        }
        restoreGlobalDriverValues();
        removeTemporaryDriverRegistration();
        if (setupDll != NULL) {
            FreeLibrary(setupDll);
            setupDll = NULL;
            configDsn = NULL;
        }
        if (holdsRunMutex) {
            ReleaseMutex(runMutex);
            holdsRunMutex = false;
        }
        if (runMutex != NULL) {
            CloseHandle(runMutex);
            runMutex = NULL;
        }
    }

    /** @brief A DSN name no concurrent run and no other case can collide with. */
    static std::string dsnName(const char *tag) {
        return std::string("rscfg_") + tag + "_" +
               std::to_string((unsigned long)GetCurrentProcessId());
    }

    /** @brief The attributes every shape carries, so a saved DSN is a usable one. */
    static std::vector<std::string> baseAttributes() {
        std::vector<std::string> pairs;
        pairs.push_back(std::string(RS_HOST_NAME) + "=redshift.example.com");
        pairs.push_back(std::string(RS_DATABASE) + "=dev");
        pairs.push_back(std::string(RS_PORT_NUMBER) + "=5439");
        return pairs;
    }

    /** Builds lpszAttributes as ConfigDSN reads it: each "key=value" NUL-terminated, then one extra NUL. */
    static std::vector<char> makeAttributes(const std::vector<std::string> &pairs) {
        std::vector<char> buf;
        for (const std::string &pair : pairs) {
            buf.insert(buf.end(), pair.begin(), pair.end());
            buf.push_back('\0');
        }
        buf.push_back('\0');
        return buf;
    }

    /** @brief The installer's own reason for a FALSE return; it does not always give one. */
    static std::string installerErrors() {
        std::string out;
        for (WORD i = 1; i <= 8; i++) {
            DWORD code = 0;
            char msg[SQL_MAX_MESSAGE_LENGTH];
            WORD len = 0;
            msg[0] = '\0';
            const RETCODE rc =
                SQLInstallerError(i, &code, msg, (WORD)sizeof(msg), &len);
            if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
                break;
            }
            out += " [" + std::to_string((unsigned long)code) + "] " + msg;
        }
        // SQLValidDSN rejecting the DSN name is the case that leaves nothing here.
        return out.empty() ? std::string(" <the installer reported no error>") : out;
    }

    /** @brief Reads one key back out of the saved DSN. */
    static std::string readDsnKey(const std::string &dsn, const char *key) {
        char buf[1024];
        buf[0] = '\0';
        SQLGetPrivateProfileString(dsn.c_str(), key, RS_DSN_KEY_ABSENT, buf,
                                   (int)sizeof(buf), ODBC_INI);
        return std::string(buf);
    }

    /** @brief Records a DSN name for removal in TearDown, once per name. */
    void rememberDsn(const std::string &dsn) {
        for (const std::string &known : dsnsToRemove) {
            if (known == dsn) {
                return;
            }
        }
        dsnsToRemove.push_back(dsn);
    }

    /**
     * Drives ConfigDSN headlessly for one DSN and reports the installer's reason if it declines.
     * The DSN is recorded for removal before the call so a partial write is cleaned up.
     * @return True if ConfigDSN returned TRUE; non-fatal so table-driven tests report every row.
     */
    bool runConfigDsn(WORD request, const std::string &dsn,
                      const std::vector<std::string> &pairs) {
        std::vector<std::string> all;
        all.push_back(std::string("DSN=") + dsn);
        all.insert(all.end(), pairs.begin(), pairs.end());
        std::vector<char> attrs = makeAttributes(all);
        rememberDsn(dsn);
        const BOOL ok = configDsn(NULL, request, RS_SETUP_DRIVER_DESC, attrs.data());
        globalAfterWrite = readGlobalDriverValues();
        globalWritten = true;
        EXPECT_TRUE(ok) << "ConfigDSN(request " << request << ") refused DSN " << dsn
                        << ":" << installerErrors();
        return ok != FALSE;
    }

    /** @brief Turns one shape into the attribute list ConfigDSN is handed. */
    static std::vector<std::string> shapeAttributes(const ConfigDsnShape &shape) {
        std::vector<std::string> pairs = baseAttributes();
        if (shape.authType != NULL) {
            pairs.push_back(std::string(RS_AUTH_TYPE) + "=" + shape.authType);
        }
        if (shape.key1 != NULL) {
            pairs.push_back(std::string(shape.key1) + "=" + shape.val1);
        }
        if (shape.key2 != NULL) {
            pairs.push_back(std::string(shape.key2) + "=" + shape.val2);
        }
        pairs.push_back(std::string(RS_SSL_MODE) + "=" + shape.requestedSslMode);
        return pairs;
    }

    /** @brief Adds each shape as a new DSN and checks the SSLMode it persisted. */
    void expectAddedShapesPersist(const ConfigDsnShape *shapes, size_t count) {
        for (size_t i = 0; i < count; i++) {
            const ConfigDsnShape &shape = shapes[i];
            const std::string label =
                std::string(shape.tag) + ": AuthType=" +
                (shape.authType ? shape.authType : "<none>") + " " +
                (shape.key1 ? shape.key1 : "<no other key>") + " requested SSLMode=" +
                shape.requestedSslMode;
            const std::string dsn = dsnName(shape.tag);
            if (!runConfigDsn(ODBC_ADD_DSN, dsn, shapeAttributes(shape))) {
                continue;
            }
            EXPECT_EQ(readDsnKey(dsn, RS_SSL_MODE), shape.persistedSslMode) << label;
        }
    }
};

// The silent failure mode: lpszAttributes is NUL-separated with an extra trailing NUL. A harness that
// joins pairs with spaces or semicolons leaves an '=' in the name rs_dsn_get_attr hands to SQLValidDSN,
// so ConfigDSN returns FALSE with no installer error. Asserting the byte layout catches that.
TEST_F(ConfigDsnPersistenceTest, the_attribute_buffer_is_nul_separated_and_double_terminated) {
    const std::string dsn = dsnName("bytes");
    // The same list runConfigDsn() builds below, so the bytes asserted here are the
    // bytes ConfigDSN is handed.
    std::vector<std::string> sslModeOnly;
    sslModeOnly.push_back(std::string(RS_SSL_MODE) + "=allow");
    std::vector<std::string> pairs;
    pairs.push_back(std::string("DSN=") + dsn);
    pairs.push_back(sslModeOnly[0]);
    const std::vector<char> attrs = makeAttributes(pairs);

    ASSERT_EQ(attrs.size(), pairs[0].size() + 1 + pairs[1].size() + 1 + 1)
        << "one NUL after each pair and one more after the last";
    EXPECT_EQ(std::string(&attrs[0]), pairs[0]);
    EXPECT_EQ(attrs[pairs[0].size()], '\0') << "the first pair must be NUL-terminated";
    EXPECT_EQ(std::string(&attrs[pairs[0].size() + 1]), pairs[1])
        << "the second pair must start right after that NUL";
    EXPECT_EQ(attrs[attrs.size() - 2], '\0') << "the last pair must be NUL-terminated";
    EXPECT_EQ(attrs[attrs.size() - 1], '\0') << "the buffer must end with a second NUL";

    // And that this is the layout ConfigDSN accepts, not only the documented one.
    ASSERT_TRUE(runConfigDsn(ODBC_ADD_DSN, dsn, sslModeOnly));
    EXPECT_EQ(readDsnKey(dsn, RS_SSL_MODE), "allow");
}

// The DSN shapes the IAM path holds to the "require" floor: an AuthType the Connect tab resolves to IAM,
// the IAM flag, and an IdC plugin_name (routed there by handleFederatedNonIamConnection()). Saving
// disable, allow or prefer on any of them would leave a DSN that fails RsIamClient::ValidateConnectionAttributes.
TEST_F(ConfigDsnPersistenceTest, an_iam_dsn_persists_the_require_floor) {
    const ConfigDsnShape shapes[] = {
        /* Explicit IAM: the auth type is named and carries the IAM flag the dialog
         * writes for it, which is what admits the connection to the IAM path. */
        {"plugin", RS_AUTH_TYPE_PLUGIN, RS_IAM, "1", RS_PLUGIN_NAME,
         IAM_PLUGIN_BROWSER_AZURE, "allow", "require"},
        {"static", RS_AUTH_TYPE_STATIC, RS_IAM, "1", NULL, NULL, "prefer",
         "require"},
        {"iamprof", RS_AUTH_TYPE_PROFILE, RS_IAM, "1", RS_PROFILE, "default",
         "allow", "require"},
        /* disable is rejected on the IAM path too, so the floor lifts it as well. */
        {"stadis", RS_AUTH_TYPE_STATIC, RS_IAM, "1", NULL, NULL, "disable",
         "require"},
        /* No AuthType, so the Connect tab reads these as Standard, but the stored
         * attributes still reach the IAM path. */
        {"infiam", NULL, RS_IAM, "1", NULL, NULL, "allow", "require"},
        {"infdis", NULL, RS_IAM, "1", NULL, NULL, "disable", "require"},
        {"infidc", NULL, RS_PLUGIN_NAME, PLUGIN_BROWSER_IDC_AUTH, NULL, NULL,
         "allow", "require"},
        {"infidp", NULL, RS_PLUGIN_NAME, PLUGIN_IDP_TOKEN_AUTH, NULL, NULL, "prefer",
         "require"},
    };
    expectAddedShapesPersist(shapes, sizeof(shapes) / sizeof(shapes[0]));
}

// A DSN that cannot reach the IAM path keeps the mode it asked for, whatever credential attributes
// an older auth type left behind: without the IAM flag or an IdC plugin there is no floor to apply.
TEST_F(ConfigDsnPersistenceTest, a_standard_dsn_keeps_the_mode_it_asked_for) {
    const ConfigDsnShape shapes[] = {
        /* Standard user and password auth, the ordinary DSN. */
        {"stdusr", NULL, RS_LOGON_ID, "awsuser", RS_PASSWORD, "Secret123", "allow",
         "allow"},
        /* Nothing but the server coordinates. */
        {"stdbare", NULL, NULL, NULL, NULL, NULL, "allow", "allow"},
        {"stdpref", NULL, NULL, NULL, NULL, NULL, "prefer", "prefer"},
        /* disable included: only the IAM path refuses it. */
        {"stddis", NULL, NULL, NULL, NULL, NULL, "disable", "disable"},
        /* Stale credential attributes, none of which reach the IAM path. */
        {"stdprof", NULL, RS_PROFILE, "default", NULL, NULL, "allow", "allow"},
        {"stdkeys", NULL, RS_ACCESS_KEY_ID, "AKIAIOSFODNN7EXAMPLE",
         RS_SECRET_ACCESS_KEY, "wJalrXUtnFEMIK7MDENGEXAMPLEKEY", "allow", "allow"},
        {"stdinst", NULL, RS_INSTANCE_PROFILE, "1", NULL, NULL, "prefer", "prefer"},
        /* A non-IdC plugin_name needs the IAM flag to reach the IAM path. */
        {"stdplug", NULL, RS_PLUGIN_NAME, IAM_PLUGIN_BROWSER_AZURE, NULL, NULL,
         "allow", "allow"},
    };
    expectAddedShapesPersist(shapes, sizeof(shapes) / sizeof(shapes[0]));
}

// The same rule on the save path: a non-Standard AuthType with no IAM flag or IdC plugin_name keeps its mode.
TEST_F(ConfigDsnPersistenceTest, an_auth_type_without_the_iam_flag_keeps_its_mode) {
    const ConfigDsnShape shapes[] = {
        {"atprof", RS_AUTH_TYPE_PROFILE, RS_PROFILE, "default", NULL, NULL, "prefer",
         "prefer"},
        {"atstat", RS_AUTH_TYPE_STATIC, NULL, NULL, NULL, NULL, "allow", "allow"},
        {"atplug", RS_AUTH_TYPE_PLUGIN, RS_PLUGIN_NAME, IAM_PLUGIN_BROWSER_AZURE,
         NULL, NULL, "prefer", "prefer"},
        /* An explicit IAM=0 says the same thing as the absent key. */
        {"atprof0", RS_AUTH_TYPE_PROFILE, RS_IAM, "0", RS_PROFILE, "default",
         "prefer", "prefer"},
    };
    expectAddedShapesPersist(shapes, sizeof(shapes) / sizeof(shapes[0]));
}

// A DSN whose mode comes only from EncryptionMethod. Both keys are written when the
// floor lifts, so neither can be left naming the weaker mode.
TEST_F(ConfigDsnPersistenceTest,
       an_encryption_method_only_iam_dsn_persists_require_in_both_keys) {
    struct EmOnlyCase {
        const char *tag;
        const char *encryptionMethod;
        const char *persistedSslMode;
        const char *persistedEncryptionMethod;
    };
    const EmOnlyCase cases[] = {
        /* disable: refused on the IAM path, so both keys are lifted to require. */
        {"emzero", "0", "require", "3"},
        /* The three the IAM path accepts are left exactly as stored, and SSLMode
         * stays unwritten because nobody named it. */
        {"emvca", "1", RS_DSN_KEY_ABSENT, "1"},
        {"emvfull", "2", RS_DSN_KEY_ABSENT, "2"},
        {"emreq", "3", RS_DSN_KEY_ABSENT, "3"},
    };

    for (const EmOnlyCase &c : cases) {
        const std::string label =
            std::string(c.tag) + ": stored EncryptionMethod=" + c.encryptionMethod;
        const std::string dsn = dsnName(c.tag);
        std::vector<std::string> pairs = baseAttributes();

        pairs.push_back(std::string(RS_AUTH_TYPE) + "=" + RS_AUTH_TYPE_STATIC);
        pairs.push_back(std::string(RS_IAM) + "=1");
        pairs.push_back(std::string(RS_DSN_ENCRYPTION_METHOD_KEY) + "=" +
                        c.encryptionMethod);
        if (!runConfigDsn(ODBC_ADD_DSN, dsn, pairs)) {
            continue;
        }

        EXPECT_EQ(readDsnKey(dsn, RS_SSL_MODE), c.persistedSslMode) << label;
        EXPECT_EQ(readDsnKey(dsn, RS_DSN_ENCRYPTION_METHOD_KEY),
                  c.persistedEncryptionMethod)
            << label;
    }
}

// verify-ca, verify-full and require are the modes the IAM path accepts, so an IAM
// DSN keeps them untouched. The floor lifts the other three and nothing else.
TEST_F(ConfigDsnPersistenceTest, verify_modes_pass_through_for_an_iam_dsn) {
    const ConfigDsnShape shapes[] = {
        {"iamvca", RS_AUTH_TYPE_STATIC, NULL, NULL, NULL, NULL, "verify-ca",
         "verify-ca"},
        {"iamvfull", RS_AUTH_TYPE_STATIC, NULL, NULL, NULL, NULL, "verify-full",
         "verify-full"},
        {"iamreq", RS_AUTH_TYPE_STATIC, NULL, NULL, NULL, NULL, "require", "require"},
    };
    expectAddedShapesPersist(shapes, sizeof(shapes) / sizeof(shapes[0]));
}

// A save writes every attribute in the table, so an unnamed key appears with an empty value. Only SSLMode
// and EncryptionMethod are gated on being named, so a save never pins the driver's default.
TEST_F(ConfigDsnPersistenceTest, an_unnamed_auth_type_persists_empty_and_an_unnamed_ssl_key_is_not_written) {
    const std::string dsn = dsnName("keys");
    std::vector<std::string> pairs = baseAttributes();
    pairs.push_back(std::string(RS_SSL_MODE) + "=allow");
    ASSERT_TRUE(runConfigDsn(ODBC_ADD_DSN, dsn, pairs));

    EXPECT_EQ(readDsnKey(dsn, RS_AUTH_TYPE), "")
        << "AuthType is written whatever its value, so it reads back empty rather "
           "than missing";
    EXPECT_EQ(readDsnKey(dsn, RS_DSN_ENCRYPTION_METHOD_KEY), RS_DSN_KEY_ABSENT)
        << "nobody named EncryptionMethod, so the save must not add it";
    EXPECT_EQ(readDsnKey(dsn, RS_SSL_MODE), "allow");
}

// A DSN an older release left with a mode the auth type rejects, reopened with ODBC_CONFIG_DSN (the modify
// path, which reads stored values first and lets them win). The floor must be settled on this path too.
// The attribute list carries only the DSN name, as the dialog's Save button does.
TEST_F(ConfigDsnPersistenceTest, reopening_a_stored_dsn_settles_the_floor_on_the_modify_path) {
    const ConfigDsnShape shapes[] = {
        /* Explicit IAM: a plugin auth type carrying the IAM flag the dialog
         * writes for it. */
        {"modplug", RS_AUTH_TYPE_PLUGIN, RS_IAM, "1", NULL, NULL, "allow",
         "require"},
        /* The same shape holding disable, which the IAM path rejects too. */
        {"moddis", RS_AUTH_TYPE_PLUGIN, RS_IAM, "1", NULL, NULL, "disable",
         "require"},
        /* A non-Standard AuthType with no IAM flag cannot reach the IAM path, so
         * the modify path must leave its mode alone too. */
        {"modauth", RS_AUTH_TYPE_PROFILE, RS_PROFILE, "default", NULL, NULL,
         "prefer", "prefer"},
        /* An IdC plugin with no AuthType stored at all, which still reaches the
         * IAM path. */
        {"modidc", NULL, RS_PLUGIN_NAME, PLUGIN_BROWSER_IDC_AUTH, NULL, NULL,
         "allow", "require"},
        /* And the ordinary DSN, which the modify path must leave alone. */
        {"modstd", NULL, NULL, NULL, NULL, NULL, "allow", "allow"},
        {"modsdis", NULL, NULL, NULL, NULL, NULL, "disable", "disable"},
        /* As must a Standard DSN carrying a stale Profile. */
        {"modprof", NULL, RS_PROFILE, "default", NULL, NULL, "allow", "allow"},
    };
    for (const ConfigDsnShape &shape : shapes) {
        const std::string label = std::string(shape.tag) + ": stored AuthType=" +
                                  (shape.authType ? shape.authType : "<none>") +
                                  " SSLMode=" + shape.requestedSslMode;
        const std::string dsn = dsnName(shape.tag);
        rememberDsn(dsn);

        // Write the stored DSN directly, as an older release left it.
        ASSERT_TRUE(SQLWritePrivateProfileString(dsn.c_str(), "Driver",
                                                 RS_SETUP_DRIVER_DESC, ODBC_INI))
            << label;
        if (shape.authType != NULL) {
            ASSERT_TRUE(SQLWritePrivateProfileString(dsn.c_str(), RS_AUTH_TYPE,
                                                     shape.authType, ODBC_INI))
                << label;
        }
        if (shape.key1 != NULL) {
            ASSERT_TRUE(SQLWritePrivateProfileString(dsn.c_str(), shape.key1,
                                                     shape.val1, ODBC_INI))
                << label;
        }
        ASSERT_TRUE(SQLWritePrivateProfileString(dsn.c_str(), RS_SSL_MODE,
                                                 shape.requestedSslMode, ODBC_INI))
            << label;
        ASSERT_EQ(readDsnKey(dsn, RS_SSL_MODE), shape.requestedSslMode)
            << "the stored DSN was not set up: " << label;

        if (!runConfigDsn(ODBC_CONFIG_DSN, dsn, std::vector<std::string>())) {
            continue;
        }

        EXPECT_EQ(readDsnKey(dsn, RS_SSL_MODE), shape.persistedSslMode) << label;
        EXPECT_EQ(readDsnKey(dsn, RS_AUTH_TYPE),
                  shape.authType ? shape.authType : "")
            << "the save must not change the stored auth type: " << label;
    }
}
#endif /* WIN32 */

// A reused HDBC must not keep the previous connection's SSL mode.

class SslModeResetTest : public ::testing::Test {
protected:
    RS_ENV_INFO envInfo;
    RS_CONN_INFO *pConn;

    /** @brief Creates the connection. */
    void SetUp() override {
        pConn = new RS_CONN_INFO(&envInfo);
        pConn->pConnectProps = new RS_CONNECT_PROPS_INFO();
        pConn->pConnAttr = new RS_CONN_ATTR_INFO();
    }

    /** @brief Frees the connection. */
    void TearDown() override { freeTestConnection(envInfo, pConn); }
};

TEST_F(SslModeResetTest, reset_clears_a_previous_connections_mode) {
    // Leave a disabled mode behind from both a DSN and a connection string.
    strncpy(pConn->pConnectProps->szSslMode, "disable",
            sizeof(pConn->pConnectProps->szSslMode) - 1);
    pConn->pConnectProps->dsnSslMode = std::string("disable");
    pConn->pConnectProps->connEncryptionMethod = 0;
    pConn->pConnectProps->iSslMismatchPolicy = PQ_SSL_MISMATCH_STRICT;

    pConn->resetConnectProps();

    EXPECT_STREQ(pConn->pConnectProps->szSslMode, "")
        << "A reused handle must not inherit the previous connection's mode";
    EXPECT_EQ(pConn->pConnectProps->iEncryptionMethod, 2)
        << "After the reset the re-derived mode must be verify-full";
    EXPECT_FALSE(pConn->pConnectProps->dsnSslMode.has_value());
    EXPECT_FALSE(pConn->pConnectProps->connSslMode.has_value());
    EXPECT_FALSE(pConn->pConnectProps->dsnEncryptionMethod.has_value());
    EXPECT_FALSE(pConn->pConnectProps->connEncryptionMethod.has_value());
    EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, PQ_SSL_MISMATCH_STRICT)
        << "Nothing has been resolved yet on a fresh handle, so the strict "
           "policy is what a mismatch would get";
}

TEST_F(UseDeclareFetchConnStringTest, UseDeclareFetch_default_is_zero) {
    parseConnStr("Driver=Amazon Redshift ODBC Driver;");
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 0);
}

TEST_F(UseDeclareFetchConnStringTest, UseDeclareFetch_enabled_with_1) {
    parseConnStr("UseDeclareFetch=1;");
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
}

TEST_F(UseDeclareFetchConnStringTest, UseDeclareFetch_enabled_with_true) {
    parseConnStr("UseDeclareFetch=true;");
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
}

TEST_F(UseDeclareFetchConnStringTest, UseDeclareFetch_disabled_with_0) {
    parseConnStr("UseDeclareFetch=0;");
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 0);
}

TEST_F(UseDeclareFetchConnStringTest, UseDeclareFetch_disabled_with_false) {
    parseConnStr("UseDeclareFetch=false;");
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 0);
}

TEST_F(UseDeclareFetchConnStringTest, UseDeclareFetch_short_alias_UDF) {
    parseConnStr("UDF=1;");
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
}

TEST_F(UseDeclareFetchConnStringTest, UseDeclareFetch_case_insensitive) {
    parseConnStr("usedeclarefetch=1;");
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
}

TEST_F(UseDeclareFetchConnStringTest, Fetch_positive_value) {
    parseConnStr("Fetch=20000;");
    EXPECT_EQ(pConn->pConnectProps->iFetchSize, 20000);
}

TEST_F(UseDeclareFetchConnStringTest, Fetch_zero_value) {
    parseConnStr("Fetch=0;");
    EXPECT_EQ(pConn->pConnectProps->iFetchSize, 0);
}

TEST_F(UseDeclareFetchConnStringTest, Fetch_negative_clamped_to_zero) {
    parseConnStr("Fetch=-100;");
    EXPECT_EQ(pConn->pConnectProps->iFetchSize, 0);
}

TEST_F(UseDeclareFetchConnStringTest, Fetch_combined_with_UseDeclareFetch) {
    parseConnStr("UseDeclareFetch=1;Fetch=5000;");
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iFetchSize, 5000);
}

TEST_F(UseDeclareFetchConnStringTest, Fetch_combined_with_other_properties) {
    parseConnStr("Server=myhost;Port=5439;Database=dev;UseDeclareFetch=1;Fetch=10000;");
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iFetchSize, 10000);
}

// ===========================================================================
// Mutual exclusivity tests: UseDeclareFetch disables SCR and CSC.
// ===========================================================================

TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_UseDeclareFetch_disables_StreamingCursorRows) {
    parseConnStr("UseDeclareFetch=1;StreamingCursorRows=5000;");
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iStreamingCursorRows, 0);
}

TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_UseDeclareFetch_disables_CscEnable) {
    parseConnStr("UseDeclareFetch=1;CscEnable=1;");
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iCscEnable, 0);
}

TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_default_fetch_size_applied) {
    parseConnStr("UseDeclareFetch=1;");
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iFetchSize, RS_DEFAULT_FETCH_SIZE);
}

TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_order_independent_SCR_before_UDF) {
    parseConnStr("StreamingCursorRows=5000;UseDeclareFetch=1;");
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iStreamingCursorRows, 0);
}

TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_order_independent_CSC_before_UDF) {
    parseConnStr("CscEnable=1;UseDeclareFetch=1;Fetch=1000;");
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iCscEnable, 0);
    EXPECT_EQ(pConn->pConnectProps->iFetchSize, 1000);
}

TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_negative_fetch_gets_default) {
    parseConnStr("UseDeclareFetch=1;Fetch=-50;");
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iFetchSize, RS_DEFAULT_FETCH_SIZE);
}

TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_UDF_disabled_does_not_affect_SCR) {
    parseConnStr("UseDeclareFetch=0;StreamingCursorRows=5000;");
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 0);
    EXPECT_EQ(pConn->pConnectProps->iStreamingCursorRows, 5000);
}

TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_disables_CSC_and_SCR_together) {
    parseConnStr("CscEnable=1;StreamingCursorRows=5000;UseDeclareFetch=1;");
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iCscEnable, 0);
    EXPECT_EQ(pConn->pConnectProps->iStreamingCursorRows, 0);
}

TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_UDFFirst_disables_CSC_and_SCR) {
    parseConnStr("UseDeclareFetch=1;CscEnable=1;StreamingCursorRows=5000;");
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iCscEnable, 0);
    EXPECT_EQ(pConn->pConnectProps->iStreamingCursorRows, 0);
}

TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_zero_fetch_gets_default) {
    parseConnStr("UseDeclareFetch=1;Fetch=0;");
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iFetchSize, RS_DEFAULT_FETCH_SIZE);
}

// UseDeclareFetch must NOT change FetchRefCursor. Refcursor auto-expansion is a
// connection-wide behavior that portal fetch leaves untouched; portal eligibility
// is decided per statement in the execution path, not by this flag. So enabling
// UseDeclareFetch preserves whatever FetchRefCursor the connection had.
TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_UseDeclareFetch_preserves_default_FetchRefCursor) {
    parseConnStr("UseDeclareFetch=1;");
    // Simulate the runtime default of FetchRefCursor=1 (set in resetConnectProps).
    pConn->pConnectProps->iFetchRefCursor = 1;
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iFetchRefCursor, 1);
}

TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_UseDeclareFetch_preserves_explicit_FetchRefCursor) {
    parseConnStr("FetchRefCursor=1;UseDeclareFetch=1;");
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iFetchRefCursor, 1);
}

TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_UDF_disabled_does_not_affect_FetchRefCursor) {
    parseConnStr("UseDeclareFetch=0;");
    pConn->pConnectProps->iFetchRefCursor = 1;
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 0);
    EXPECT_EQ(pConn->pConnectProps->iFetchRefCursor, 1);
}

TEST(DriverConnectInputCopy, CopiesSqlNtsInputVerbatim) {
    const SQLCHAR input[] =
        "Driver=Redshift;Database=dev;StreamingCursorRows=0;CustomKey=value;";
    SQLCHAR output[sizeof(input)] = {0};
    bool truncated = true;

    size_t length = RS_copyDriverConnectInput(input, SQL_NTS, output,
                                              sizeof(output), &truncated);

    EXPECT_EQ(strlen((const char *)input), length);
    EXPECT_FALSE(truncated);
    EXPECT_STREQ((const char *)input, (const char *)output);
}

TEST(DriverConnectInputCopy, CopiesExplicitLengthWithoutReadingPastInput) {
    const SQLCHAR input[] = {'A', '=', '1', ';'};
    SQLCHAR output[5] = {0};
    bool truncated = true;

    size_t length = RS_copyDriverConnectInput(input, sizeof(input), output,
                                              sizeof(output), &truncated);

    EXPECT_EQ(sizeof(input), length);
    EXPECT_FALSE(truncated);
    EXPECT_EQ(0, memcmp(input, output, sizeof(input)));
    EXPECT_EQ('\0', output[sizeof(input)]);
}

TEST(DriverConnectInputCopy, ReportsTruncationAndFullLength) {
    const SQLCHAR input[] = "ExtraKeyword=preserved;";
    SQLCHAR output[8] = {0};
    bool truncated = false;

    size_t length = RS_copyDriverConnectInput(input, SQL_NTS, output,
                                              sizeof(output), &truncated);

    EXPECT_EQ(strlen((const char *)input), length);
    EXPECT_TRUE(truncated);
    EXPECT_EQ(sizeof(output) - 1, strlen((const char *)output));
    EXPECT_EQ('\0', output[sizeof(output) - 1]);
}

TEST(DriverConnectInputCopy, NullOutputIsLengthInquiryWithoutTruncation) {
    const SQLCHAR input[] = "Database=dev;";
    bool truncated = true;

    size_t length =
        RS_copyDriverConnectInput(input, SQL_NTS, NULL, 0, &truncated);

    EXPECT_EQ(strlen((const char *)input), length);
    EXPECT_FALSE(truncated);
}

TEST(DriverConnectInputCopy, ZeroLengthOutputReportsLengthAndTruncation) {
    const SQLCHAR input[] = "Database=dev;";
    SQLCHAR output = 'x';
    bool truncated = false;

    size_t length =
        RS_copyDriverConnectInput(input, SQL_NTS, &output, 0, &truncated);

    EXPECT_EQ(strlen((const char *)input), length);
    EXPECT_TRUE(truncated);
    EXPECT_EQ('x', output);
}

TEST(DriverConnectInputCopy, NullInputYieldsEmptyOutput) {
    SQLCHAR output[4] = {'x', 'x', 'x', 'x'};
    bool truncated = true;

    size_t length = RS_copyDriverConnectInput(NULL, SQL_NTS, output,
                                              sizeof(output), &truncated);

    EXPECT_EQ(0u, length);
    EXPECT_FALSE(truncated);
    EXPECT_EQ('\0', output[0]);
}

TEST(DriverConnectInputCopy, NegativeLengthOtherThanSqlNtsCopiesNothing) {
    const SQLCHAR input[] = "DSN=a;";
    SQLCHAR output[8] = {'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x'};
    bool truncated = true;

    size_t length = RS_copyDriverConnectInput(input, (SQLSMALLINT)-2, output,
                                              sizeof(output), &truncated);

    EXPECT_EQ(0u, length);
    EXPECT_FALSE(truncated);
    EXPECT_EQ('\0', output[0]);
}

// Wide copy: SQLWCHAR units, exact bytes, lengths in characters.

static std::vector<SQLWCHAR> makeWide(const std::vector<unsigned int> &cps) {
    std::vector<SQLWCHAR> out(cps.begin(), cps.end());
    out.push_back(0);
    return out;
}

TEST(DriverConnectInputCopyW, CopiesSqlNtsInputVerbatim) {
    std::vector<SQLWCHAR> input =
        makeWide({'D', 'S', 'N', '=', 'a', ';', 'K', '=', 'v', ';'});
    std::vector<SQLWCHAR> output(input.size(), 0xFFFF);
    bool truncated = true;

    size_t length = RS_copyDriverConnectInputW(
        input.data(), SQL_NTS, output.data(), (SQLSMALLINT)output.size(),
        &truncated, SQL_DD_CP_UTF16);

    EXPECT_EQ(input.size() - 1, length);
    EXPECT_FALSE(truncated);
    EXPECT_EQ(0, memcmp(input.data(), output.data(),
                        input.size() * sizeof(SQLWCHAR)));
}

TEST(DriverConnectInputCopyW, LengthIsInCharactersNotUtf8Bytes) {
    // Each character below encodes to multiple UTF-8 bytes.
    std::vector<SQLWCHAR> input = makeWide({'U', '=', 0x00E9, 0x4E2D, ';'});
    std::vector<SQLWCHAR> output(input.size(), 0);
    bool truncated = true;

    size_t length = RS_copyDriverConnectInputW(
        input.data(), SQL_NTS, output.data(), (SQLSMALLINT)output.size(),
        &truncated, SQL_DD_CP_UTF16);

    EXPECT_EQ(5u, length);
    EXPECT_FALSE(truncated);
    EXPECT_EQ(0, memcmp(input.data(), output.data(),
                        input.size() * sizeof(SQLWCHAR)));
}

TEST(DriverConnectInputCopyW, CopiesExplicitLengthWithoutReadingPastInput) {
    const SQLWCHAR input[] = {'A', '=', '1', ';'};
    SQLWCHAR output[5] = {1, 1, 1, 1, 1};
    bool truncated = true;

    size_t length = RS_copyDriverConnectInputW(
        input, (SQLSMALLINT)(sizeof(input) / sizeof(input[0])), output,
        (SQLSMALLINT)(sizeof(output) / sizeof(output[0])), &truncated,
        SQL_DD_CP_UTF16);

    EXPECT_EQ(4u, length);
    EXPECT_FALSE(truncated);
    EXPECT_EQ(0, memcmp(input, output, sizeof(input)));
    EXPECT_EQ(0, output[4]);
}

TEST(DriverConnectInputCopyW, ReportsTruncationAndFullLength) {
    std::vector<SQLWCHAR> input =
        makeWide({'E', 'x', 't', 'r', 'a', '=', 'k', 'e', 'p', 't', ';'});
    SQLWCHAR output[4] = {0};
    bool truncated = false;

    size_t length = RS_copyDriverConnectInputW(
        input.data(), SQL_NTS, output,
        (SQLSMALLINT)(sizeof(output) / sizeof(output[0])), &truncated,
        SQL_DD_CP_UTF16);

    EXPECT_EQ(input.size() - 1, length);
    EXPECT_TRUE(truncated);
    EXPECT_EQ(0, memcmp(input.data(), output, 3 * sizeof(SQLWCHAR)));
    EXPECT_EQ(0, output[3]);
}

TEST(DriverConnectInputCopyW, NullOutputIsLengthInquiryWithoutTruncation) {
    std::vector<SQLWCHAR> input = makeWide({'D', 'B', '=', 'd', ';'});
    bool truncated = true;

    size_t length = RS_copyDriverConnectInputW(input.data(), SQL_NTS, NULL, 0,
                                               &truncated, SQL_DD_CP_UTF16);

    EXPECT_EQ(5u, length);
    EXPECT_FALSE(truncated);
}

TEST(DriverConnectInputCopyW, ZeroLengthOutputReportsLengthAndTruncation) {
    std::vector<SQLWCHAR> input = makeWide({'D', 'B', '=', 'd', ';'});
    SQLWCHAR output = 'x';
    bool truncated = false;

    size_t length = RS_copyDriverConnectInputW(input.data(), SQL_NTS, &output,
                                               0, &truncated, SQL_DD_CP_UTF16);

    EXPECT_EQ(5u, length);
    EXPECT_TRUE(truncated);
    EXPECT_EQ('x', output);
}

TEST(DriverConnectInputCopyW, NullInputYieldsEmptyOutput) {
    SQLWCHAR output[4] = {'x', 'x', 'x', 'x'};
    bool truncated = true;

    size_t length = RS_copyDriverConnectInputW(NULL, SQL_NTS, output, 4,
                                               &truncated, SQL_DD_CP_UTF16);

    EXPECT_EQ(0u, length);
    EXPECT_FALSE(truncated);
    EXPECT_EQ(0, output[0]);
}

TEST(DriverConnectInputCopyW, NegativeLengthOtherThanSqlNtsCopiesNothing) {
    std::vector<SQLWCHAR> input = makeWide({'D', 'S', 'N', '=', 'a', ';'});
    SQLWCHAR output[8] = {'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x'};
    bool truncated = true;

    size_t length = RS_copyDriverConnectInputW(
        input.data(), (SQLSMALLINT)-2, output,
        (SQLSMALLINT)(sizeof(output) / sizeof(output[0])), &truncated,
        SQL_DD_CP_UTF16);

    EXPECT_EQ(0u, length);
    EXPECT_FALSE(truncated);
    EXPECT_EQ(0, output[0]);
}

// Wide copy at the 4 byte unit width. Buffers are uint32_t vectors so the
// tests are independent of the compile time SQLWCHAR width and the storage
// satisfies the 4 byte alignment the copy primitive requires.

static std::vector<uint32_t> makeWide32(const std::vector<char32_t> &cps) {
    std::vector<uint32_t> out(cps.begin(), cps.end());
    out.push_back(0);
    return out;
}

static const SQLWCHAR *asSqlwchar(const std::vector<uint32_t> &v) {
    return reinterpret_cast<const SQLWCHAR *>(v.data());
}

static SQLWCHAR *asSqlwchar(std::vector<uint32_t> &v) {
    return reinterpret_cast<SQLWCHAR *>(v.data());
}

TEST(DriverConnectInputCopyW, Utf32SqlNtsLengthCountsFourByteUnits) {
    // Read in 2 byte units, this string ends after the first character
    // because the upper half of each 4 byte unit is zero.
    std::vector<uint32_t> input =
        makeWide32({'D', 'S', 'N', '=', 'a', ';', 'K', '=', 'v', ';'});
    std::vector<uint32_t> output(input.size(), 0xFFFFFFFFu);
    bool truncated = true;

    size_t length = RS_copyDriverConnectInputW(
        asSqlwchar(input), SQL_NTS, asSqlwchar(output),
        (SQLSMALLINT)output.size(), &truncated, SQL_DD_CP_UTF32);

    EXPECT_EQ(10u, length);
    EXPECT_FALSE(truncated);
    EXPECT_EQ(input, output);
}

TEST(DriverConnectInputCopyW, Utf32ExplicitLengthCopiesFourByteUnits) {
    std::vector<uint32_t> input = makeWide32({'A', '=', '1', ';'});
    std::vector<uint32_t> output(5, 0xFFFFFFFFu);
    bool truncated = true;

    size_t length = RS_copyDriverConnectInputW(
        asSqlwchar(input), (SQLSMALLINT)4, asSqlwchar(output), (SQLSMALLINT)5,
        &truncated, SQL_DD_CP_UTF32);

    EXPECT_EQ(4u, length);
    EXPECT_FALSE(truncated);
    // Four characters copied, then a full 4 byte terminator.
    EXPECT_EQ(input, output);
}

TEST(DriverConnectInputCopyW, Utf32TruncationTerminatesWithFourByteUnit) {
    std::vector<uint32_t> input =
        makeWide32({'E', 'x', 't', 'r', 'a', '=', 'k', 'e', 'p', 't', ';'});
    std::vector<uint32_t> output(4, 0xFFFFFFFFu);
    bool truncated = false;

    size_t length = RS_copyDriverConnectInputW(
        asSqlwchar(input), SQL_NTS, asSqlwchar(output), (SQLSMALLINT)4,
        &truncated, SQL_DD_CP_UTF32);

    EXPECT_EQ(11u, length);
    EXPECT_TRUE(truncated);
    const std::vector<uint32_t> expected = {'E', 'x', 't', 0};
    EXPECT_EQ(expected, output);
}

// Negative control: the same 4 byte unit bytes measured as 2 byte units stop
// at the zero high half of the first character, so a caller that passes the
// wrong width sees length 1 instead of the full string.
TEST(DriverConnectInputCopyW, Utf32BytesMeasuredAsUtf16StopAtFirstUnit) {
    std::vector<uint32_t> input =
        makeWide32({'D', 'S', 'N', '=', 'a', ';', 'K', '=', 'v', ';'});
    bool truncated = true;

    size_t length = RS_copyDriverConnectInputW(asSqlwchar(input), SQL_NTS,
                                               NULL, 0, &truncated,
                                               SQL_DD_CP_UTF16);

    EXPECT_EQ(1u, length);
    EXPECT_FALSE(truncated);
}

// SQLDriverConnectW diagnostics: each call starts with an empty list.

TEST(DriverConnectDiagnostics, WideLengthErrorReplacesPriorRecords) {
    SQLHENV env = SQL_NULL_HANDLE;
    SQLHDBC dbc = SQL_NULL_HANDLE;
    ASSERT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env));
    ASSERT_EQ(SQL_SUCCESS, SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION,
                                         (SQLPOINTER)SQL_OV_ODBC3, 0));
    ASSERT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc));

    std::vector<SQLWCHAR> input = makeWide({'D', 'S', 'N', '=', 'a', ';', 0});
    SQLWCHAR output[16] = {0};
    SQLSMALLINT outputLength = 0;

    // Two consecutive calls rejected before any connection attempt.
    for (int call = 0; call < 2; ++call) {
        EXPECT_EQ(SQL_ERROR,
                  SQLDriverConnectW(dbc, NULL, input.data(), (SQLSMALLINT)-999,
                                    output, 16, &outputLength,
                                    SQL_DRIVER_NOPROMPT));
    }

    SQLINTEGER recordCount = -1;
    ASSERT_EQ(SQL_SUCCESS,
              SQLGetDiagField(SQL_HANDLE_DBC, dbc, 0, SQL_DIAG_NUMBER,
                              &recordCount, SQL_IS_INTEGER, NULL));
    EXPECT_EQ(1, recordCount);

    SQLCHAR sqlState[6] = {0};
    SQLINTEGER nativeError = 0;
    SQLCHAR message[256] = {0};
    SQLSMALLINT messageLength = 0;
    ASSERT_EQ(SQL_SUCCESS,
              SQLGetDiagRec(SQL_HANDLE_DBC, dbc, 1, sqlState, &nativeError,
                            message, sizeof(message), &messageLength));
    EXPECT_STREQ("HY090", (const char *)sqlState);
    EXPECT_EQ(SQL_NO_DATA,
              SQLGetDiagRec(SQL_HANDLE_DBC, dbc, 2, sqlState, &nativeError,
                            message, sizeof(message), &messageLength));

    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
}

// RS_appendSuppliedCredentialsToConnStr emits only pre-authentication credentials.

TEST(SuppliedCredentialsConnStr, OmitsCredentialsResolvedDuringConnect) {
    RS_CONNECT_PROPS_INFO props;
    props.iUserKeyWordType = SHORT_NAME_KEYWORD;
    props.iPasswordKeyWordType = SHORT_NAME_KEYWORD;

    // Auth resolved the credentials; the application supplied none.
    strncpy(props.szUser, "IAMA:resolved_user", sizeof(props.szUser) - 1);
    strncpy(props.szPassword, "temporary-secret", sizeof(props.szPassword) - 1);
    props.szOrigUser[0] = '\0';
    props.szOrigPassword[0] = '\0';

    char out[1024];
    out[0] = '\0';
    RS_appendSuppliedCredentialsToConnStr(&props, out, sizeof(out));

    std::string result(out);
    EXPECT_EQ(result.find("UID="), std::string::npos);
    EXPECT_EQ(result.find("PWD="), std::string::npos);
    EXPECT_EQ(result.find("resolved_user"), std::string::npos);
    EXPECT_EQ(result.find("temporary-secret"), std::string::npos);
}

TEST(SuppliedCredentialsConnStr, EchoesCredentialsSuppliedByApplication) {
    RS_CONNECT_PROPS_INFO props;
    props.iUserKeyWordType = SHORT_NAME_KEYWORD;
    props.iPasswordKeyWordType = SHORT_NAME_KEYWORD;

    // Supplied and resolved values differ; only supplied ones are echoed.
    strncpy(props.szUser, "IAMA:resolved_user", sizeof(props.szUser) - 1);
    strncpy(props.szOrigUser, "appuser", sizeof(props.szOrigUser) - 1);
    strncpy(props.szPassword, "resolved-temp-secret", sizeof(props.szPassword) - 1);
    strncpy(props.szOrigPassword, "apppw", sizeof(props.szOrigPassword) - 1);

    char out[1024];
    out[0] = '\0';
    RS_appendSuppliedCredentialsToConnStr(&props, out, sizeof(out));

    std::string result(out);
    EXPECT_STREQ(out, "UID=appuser;PWD=apppw;");
    EXPECT_EQ(result.find("resolved_user"), std::string::npos);
    EXPECT_EQ(result.find("resolved-temp-secret"), std::string::npos);
}

TEST(SuppliedCredentialsConnStr, UsesLongKeywordFormWhenConfigured) {
    RS_CONNECT_PROPS_INFO props;
    props.iUserKeyWordType = SHORT_NAME_KEYWORD + 1;      // long-name form
    props.iPasswordKeyWordType = SHORT_NAME_KEYWORD + 1;  // long-name form

    strncpy(props.szOrigUser, "appuser", sizeof(props.szOrigUser) - 1);
    strncpy(props.szOrigPassword, "apppw", sizeof(props.szOrigPassword) - 1);

    char out[1024];
    out[0] = '\0';
    RS_appendSuppliedCredentialsToConnStr(&props, out, sizeof(out));

    EXPECT_STREQ(out, "LogonID=appuser;Password=apppw;");
}

TEST(SuppliedCredentialsConnStr, ResetScrubsRetainedCredentials) {
    RS_ENV_INFO env;
    RS_CONN_INFO conn(&env);
    RS_CONNECT_PROPS_INFO props;
    conn.pConnectProps = &props;

    const char user[] = "appuser";
    const char password[] = "app-durable-secret";
    strncpy(props.szOrigUser, user, sizeof(props.szOrigUser) - 1);
    strncpy(props.szOrigPassword, password, sizeof(props.szOrigPassword) - 1);
    strncpy(props.szPassword, password, sizeof(props.szPassword) - 1);

    conn.resetConnectProps();

    // Full buffers must be scrubbed, not just the first byte.
    for (size_t i = 0; i < sizeof(user); i++) {
        EXPECT_EQ('\0', props.szOrigUser[i]) << "szOrigUser byte " << i;
    }
    for (size_t i = 0; i < sizeof(password); i++) {
        EXPECT_EQ('\0', props.szOrigPassword[i]) << "szOrigPassword byte " << i;
        EXPECT_EQ('\0', props.szPassword[i]) << "szPassword byte " << i;
    }

    // props is stack-owned; detach it before conn goes out of scope.
    conn.pConnectProps = NULL;
}

TEST(SuppliedCredentialsConnStr, DestructorScrubsCredentials) {
    const char password[] = "app-durable-secret";
    alignas(RS_CONNECT_PROPS_INFO) unsigned char
        storage[sizeof(RS_CONNECT_PROPS_INFO)];

    RS_CONNECT_PROPS_INFO *props = new (storage) RS_CONNECT_PROPS_INFO();
    strncpy(props->szOrigPassword, password, sizeof(props->szOrigPassword) - 1);
    strncpy(props->szPassword, password, sizeof(props->szPassword) - 1);

    // Record the buffer offsets so the raw storage can be inspected afterwards.
    size_t origOffset =
        reinterpret_cast<unsigned char *>(props->szOrigPassword) - storage;
    size_t passwordOffset =
        reinterpret_cast<unsigned char *>(props->szPassword) - storage;

    props->~RS_CONNECT_PROPS_INFO();

    // The destructor must scrub the buffers on every teardown path.
    for (size_t i = 0; i < sizeof(password); i++) {
        EXPECT_EQ(0, storage[origOffset + i]) << "szOrigPassword byte " << i;
        EXPECT_EQ(0, storage[passwordOffset + i]) << "szPassword byte " << i;
    }
}

/*====================================================================================================================================================*/

// SSLMode/EncryptionMethod logic of the Windows DSN dialog, which builds on every platform.
namespace {

// Combo index equals the EncryptionMethod value.
const int EM_DISABLE = 0;
const int EM_VERIFY_CA = 1;
const int EM_VERIFY_FULL = 2;
const int EM_REQUIRE = 3;
/* Combo indices for the two modes with no EncryptionMethod code. */
const int COMBO_ALLOW = 4;
const int COMBO_PREFER = 5;
/* What the combo shows for an EncryptionMethod it cannot spell: nothing. */
const int COMBO_NO_SELECTION = -1;

// What the setup table reads for EncryptionMethod when the DSN lacks it.
const int EM_TABLE_DEFAULT = EM_VERIFY_FULL;

struct OpenCase {
    const char *name;
    bool sslModeNamed;
    const char *sslMode;
    int encryptionMethod;
    int expectedIndex;
};

const OpenCase kOpenCases[] = {
    {"names_neither_key", false, "verify-full", EM_TABLE_DEFAULT, EM_VERIFY_FULL},
    {"names_only_encryption_method_3", false, "verify-full", EM_REQUIRE, EM_REQUIRE},
    {"names_only_sslmode_require", true, "require", EM_TABLE_DEFAULT, EM_REQUIRE},
    {"names_only_sslmode_disable", true, "DISABLE", EM_TABLE_DEFAULT, EM_DISABLE},
    {"sslmode_outranks_encryption_method", true, "verify-full", EM_REQUIRE, EM_VERIFY_FULL},
    // allow/prefer now have their own combo entries and open on them, outranking
    // any EncryptionMethod the DSN also carries.
    {"sslmode_prefer_opens_on_its_own_entry", true, "prefer", EM_REQUIRE, COMBO_PREFER},
    {"sslmode_allow_opens_on_its_own_entry", true, "allow", EM_DISABLE, COMBO_ALLOW},
    // A named SSLMode the combo has no entry for falls back to EncryptionMethod.
    {"sslmode_the_combo_cannot_show", true, "verify_full", EM_REQUIRE, EM_REQUIRE},
    // 4 and 5 are combo indices, not EncryptionMethod codes. The driver rejects
    // them and falls back to verify-full, so the dialog must not show allow/prefer
    // while a strict mode is in effect.
    {"encryption_method_4_selects_nothing", false, NULL, 4, COMBO_NO_SELECTION},
    {"encryption_method_5_selects_nothing", false, NULL, 5, COMBO_NO_SELECTION},
    // DSNs saved by older releases open on the mode they carry.
    {"old_dsn_encryption_method_1", false, "verify-full", EM_VERIFY_CA, EM_VERIFY_CA},
    {"old_dsn_sslmode_verify_ca", true, "verify-ca", EM_TABLE_DEFAULT, EM_VERIFY_CA},
    {"old_windows_dsn_with_both_keys", true, "verify-ca", EM_VERIFY_CA, EM_VERIFY_CA},
};

/**
 * @brief Prints the case name on failure instead of raw bytes.
 * @param c The case.
 * @param os Stream gtest prints to.
 */
void PrintTo(const OpenCase &c, std::ostream *os) { *os << c.name; }

class DsnSetupSslTabOpen : public ::testing::TestWithParam<OpenCase> {};

} // namespace

/*====================================================================================================================================================*/

// The SSL tab opens on the mode the DSN resolves to, so an untouched save
// doesn't change it.
TEST_P(DsnSetupSslTabOpen, shows_the_mode_the_dsn_resolves_to)
{
    const OpenCase &c = GetParam();

    EXPECT_EQ(rs_dsn_em_index_on_open(c.sslModeNamed, c.sslMode, c.encryptionMethod),
              c.expectedIndex)
        << c.name << ": SSLMode " << (c.sslModeNamed ? c.sslMode : "(absent)")
        << ", EncryptionMethod " << c.encryptionMethod;
}

INSTANTIATE_TEST_SUITE_P(DsnSetup, DsnSetupSslTabOpen, ::testing::ValuesIn(kOpenCases),
                         [](const ::testing::TestParamInfo<OpenCase> &info) {
                             return std::string(info.param.name);
                         });

/*====================================================================================================================================================*/

// Adding an SSLMode here would outrank EncryptionMethod=1 and change the mode.
TEST(DsnSetupSslKeys, a_single_key_dsn_keeps_only_its_own_key)
{
    const int sslModeNamed = 0;
    const int encryptionMethodNamed = 1;

    EXPECT_FALSE(rs_dsn_ssl_key_is_named(RS_DSN_SSL_MODE_KEY, sslModeNamed,
                                         encryptionMethodNamed))
        << "SSLMode was not in the DSN, so it must not be written or tested";
    EXPECT_TRUE(rs_dsn_ssl_key_is_named(RS_DSN_ENCRYPTION_METHOD_KEY, sslModeNamed,
                                        encryptionMethodNamed))
        << "EncryptionMethod was in the DSN, so it must be kept";
}

// Writing either key would pin verify-full and drop the mismatch allowance.
TEST(DsnSetupSslKeys, a_dsn_naming_neither_key_gains_neither)
{
    EXPECT_FALSE(rs_dsn_ssl_key_is_named(RS_DSN_SSL_MODE_KEY, 0, 0))
        << "an unnamed SSLMode must not be written or tested at its default";
    EXPECT_FALSE(rs_dsn_ssl_key_is_named(RS_DSN_ENCRYPTION_METHOD_KEY, 0, 0))
        << "an unnamed EncryptionMethod must not be written or tested at its default";
}

TEST(DsnSetupSslKeys, keys_are_matched_without_regard_to_case)
{
    EXPECT_FALSE(rs_dsn_ssl_key_is_named("sslmode", 0, 1))
        << "the setup table compares attribute names case-insensitively";
    EXPECT_FALSE(rs_dsn_ssl_key_is_named("ENCRYPTIONMETHOD", 1, 0))
        << "the setup table compares attribute names case-insensitively";
}

TEST(DsnSetupSslKeys, other_attributes_are_unaffected)
{
    EXPECT_TRUE(rs_dsn_ssl_key_is_named("HostName", 0, 0))
        << "only the two SSL mode keys are gated";
    EXPECT_TRUE(rs_dsn_ssl_key_is_named("TLSMethod", 0, 0))
        << "only the two SSL mode keys are gated";
}
