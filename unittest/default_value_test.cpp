#include "common.h"
#include <rsdesc.h>
#include <rsodbc.h>
#include <rsutil.h>
#include <sql.h>
#include <cstring>
#include <new>
#include <string>

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
