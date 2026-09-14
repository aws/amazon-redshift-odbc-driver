#include "common.h"
#include <rsdesc.h>
#include <rsodbc.h>
#include <rsutil.h>
#include <sql.h>
#include <cstring>

// Unit tests for connection property default values and parsing.

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
// The enforcement code is currently commented out pending the execution path
// implementation . These tests document the intended
// behavior and will pass once the code is uncommented.
// ===========================================================================

TEST_F(UseDeclareFetchConnStringTest, DISABLED_MutualExclusivity_UseDeclareFetch_disables_StreamingCursorRows) {
    // When UseDeclareFetch=1, StreamingCursorRows should be forced to 0
    // TODO: Enable when mutual exclusivity code is uncommented
    parseConnStr("UseDeclareFetch=1;StreamingCursorRows=5000;");
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iStreamingCursorRows, 0);
}

TEST_F(UseDeclareFetchConnStringTest, DISABLED_MutualExclusivity_UseDeclareFetch_disables_CscEnable) {
    // When UseDeclareFetch=1, CscEnable should be forced to 0
    // TODO: Enable when mutual exclusivity code is uncommented
    parseConnStr("UseDeclareFetch=1;CscEnable=1;");
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iCscEnable, 0);
}

TEST_F(UseDeclareFetchConnStringTest, DISABLED_MutualExclusivity_default_fetch_size_applied) {
    // When UseDeclareFetch=1 and Fetch=0, the driver should apply the default batch size
    // TODO: Enable when mutual exclusivity code is uncommented
    parseConnStr("UseDeclareFetch=1;");
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iFetchSize, RS_DEFAULT_FETCH_SIZE);
}
