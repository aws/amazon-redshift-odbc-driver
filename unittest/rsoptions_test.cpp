#include "common.h"
#include "rsoptions.h"
#include "rsodbc.h"
#include "rsutil.h"
#include <sql.h>
#include <sqlext.h>
#include <string>

// Test isStrConnectAttr function
class IsStrConnectAttrTest : public ::testing::Test {
};

TEST_F(IsStrConnectAttrTest, ReturnsTrue_ForStringAttributes) {
    // Test string-type connection attributes
    EXPECT_TRUE(RsOptions::isStrConnectAttr(SQL_ATTR_CURRENT_CATALOG));
    EXPECT_TRUE(RsOptions::isStrConnectAttr(SQL_ATTR_TRACEFILE));
    EXPECT_TRUE(RsOptions::isStrConnectAttr(SQL_ATTR_TRANSLATE_LIB));
}

TEST_F(IsStrConnectAttrTest, ReturnsFalse_ForIntegerAttributes) {
    // Test integer-type connection attributes
    EXPECT_FALSE(RsOptions::isStrConnectAttr(SQL_ATTR_ACCESS_MODE));
    EXPECT_FALSE(RsOptions::isStrConnectAttr(SQL_ATTR_AUTOCOMMIT));
    EXPECT_FALSE(RsOptions::isStrConnectAttr(SQL_ATTR_CONNECTION_TIMEOUT));
    EXPECT_FALSE(RsOptions::isStrConnectAttr(SQL_ATTR_LOGIN_TIMEOUT));
    EXPECT_FALSE(RsOptions::isStrConnectAttr(SQL_ATTR_PACKET_SIZE));
    EXPECT_FALSE(RsOptions::isStrConnectAttr(SQL_ATTR_TXN_ISOLATION));
    EXPECT_FALSE(RsOptions::isStrConnectAttr(SQL_ATTR_QUIET_MODE));
    EXPECT_FALSE(RsOptions::isStrConnectAttr(SQL_ATTR_TRANSLATE_OPTION));
}

TEST_F(IsStrConnectAttrTest, ReturnsFalse_ForInvalidAttributes) {
    // Test invalid/unknown attributes
    EXPECT_FALSE(RsOptions::isStrConnectAttr(-1));
    EXPECT_FALSE(RsOptions::isStrConnectAttr(0));
    EXPECT_FALSE(RsOptions::isStrConnectAttr(99999));
}

// Two connection attributes private to the Progress DataDirect driver
// manager, both defined in its qesqlext.h and neither defined by ODBC:
//
//   SQL_ATTR_IGNORE_UNICODE_FUNCTIONS  SQL_CONOPT_START+23  "treat me as ANSI"
//   SQL_ATTR_DRIVER_UNICODE_TYPE       SQL_CONOPT_START+25  driver's encoding
//
// DataDirect asks for both before connecting, on a connection handle it
// allocates and frees without ever connecting, which is why these tests probe
// an unconnected handle. Each test opens its own environment because the
// declared ODBC version decides which spelling of the SQLSTATE comes back.
class UnicodeHandshakeAttrTest : public ::testing::Test {
  protected:
    // Allocates an environment declaring the given ODBC version, plus the
    // connection handle the driver manager would probe. No connect: these
    // attributes never reach the server.
    //
    // ASSERT_* here returns from this helper, not from the test, so callers
    // must wrap the call in ASSERT_NO_FATAL_FAILURE to stop a test that would
    // otherwise go on to use unallocated handles.
    void openHandles(SQLINTEGER odbcVersion) {
        ASSERT_EQ(SQL_SUCCESS,
                  SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_env));
        ASSERT_EQ(SQL_SUCCESS,
                  SQLSetEnvAttr(m_env, SQL_ATTR_ODBC_VERSION,
                                (SQLPOINTER)(intptr_t)odbcVersion, 0));
        ASSERT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_DBC, m_env, &m_dbc));
    }

    void TearDown() override {
        if (m_dbc != SQL_NULL_HANDLE)
            SQLFreeHandle(SQL_HANDLE_DBC, m_dbc);
        if (m_env != SQL_NULL_HANDLE)
            SQLFreeHandle(SQL_HANDLE_ENV, m_env);
    }

    // SQLSTATE of the first diagnostic record on the DBC handle. The driver
    // stores ODBC 3.x states and maps them down for SQL_OV_ODBC2 callers.
    std::string dbcSqlState() {
        SQLCHAR state[6] = {0};
        SQLINTEGER native = 0;
        SQLCHAR msg[SQL_MAX_MESSAGE_LENGTH] = {0};
        SQLSMALLINT msgLen = 0;
        if (SQLGetDiagRec(SQL_HANDLE_DBC, m_dbc, 1, state, &native, msg,
                          sizeof(msg), &msgLen) == SQL_NO_DATA) {
            return "";
        }
        return std::string(reinterpret_cast<char *>(state));
    }

    // Probes SQL_ATTR_IGNORE_UNICODE_FUNCTIONS, leaving a sentinel in value
    // so a caller can assert the driver did not write it.
    SQLRETURN probeIgnoreUnicodeFunctions(SQLUINTEGER *value) {
        *value = 0xDEADBEEF;
        return SQLGetConnectAttr(m_dbc, SQL_ATTR_IGNORE_UNICODE_FUNCTIONS,
                                 value, 0, nullptr);
    }

    SQLHENV m_env = SQL_NULL_HANDLE;
    SQLHDBC m_dbc = SQL_NULL_HANDLE;
};

// Answering SQL_ATTR_IGNORE_UNICODE_FUNCTIONS with 1 told DataDirect to
// ignore this driver's W functions, which made it narrow SQL_C_WCHAR binds
// through IANAAppCodePage. Reject SQL_ATTR_IGNORE_UNICODE_FUNCTIONS instead.
TEST_F(UnicodeHandshakeAttrTest, IgnoreUnicodeFunctions_IsRejected_Odbc3) {
    // 1) An ODBC 3.x application, which is also the driver's default.
    ASSERT_NO_FATAL_FAILURE(openHandles(SQL_OV_ODBC3));
    SQLUINTEGER value = 0;
    // 2) SQL_ATTR_IGNORE_UNICODE_FUNCTIONS must fail rather than answer
    //    "ignore my W functions".
    EXPECT_EQ(SQL_ERROR, probeIgnoreUnicodeFunctions(&value));
    // 3) HY092 "invalid attribute/option identifier", which is what ODBC
    //    1.4.65 returns for SQL_ATTR_IGNORE_UNICODE_FUNCTIONS.
    EXPECT_EQ("HY092", dbcSqlState());
    // 4) Nothing was written, so the DM cannot read a stale 1.
    EXPECT_EQ((SQLUINTEGER)0xDEADBEEF, value);
}

// Informatica and other ODBC 2 applications must see the ODBC 2 spelling of
// the SQL_ATTR_IGNORE_UNICODE_FUNCTIONS error, which the driver maps from
// HY092 on retrieval.
TEST_F(UnicodeHandshakeAttrTest, IgnoreUnicodeFunctions_IsRejected_Odbc2) {
    // 1) An ODBC 2.x application.
    ASSERT_NO_FATAL_FAILURE(openHandles(SQL_OV_ODBC2));
    SQLUINTEGER value = 0;
    // 2) SQL_ATTR_IGNORE_UNICODE_FUNCTIONS is rejected here too, so the DM
    //    still keeps us on the W path.
    EXPECT_EQ(SQL_ERROR, probeIgnoreUnicodeFunctions(&value));
    // 3) S1092 is the ODBC 2 spelling of HY092.
    EXPECT_EQ("S1092", dbcSqlState());
    // 4) The output buffer is untouched regardless of version.
    EXPECT_EQ((SQLUINTEGER)0xDEADBEEF, value);
}

// DataDirect learns this driver's encoding from SQL_ATTR_DRIVER_UNICODE_TYPE.
// Rejecting SQL_ATTR_IGNORE_UNICODE_FUNCTIONS must not disturb it: it has to
// keep answering UTF-16, or the driver manager would hand the W functions the
// wrong encoding.
TEST_F(UnicodeHandshakeAttrTest, DriverUnicodeType_IsUtf16) {
    // 1) unixODBC and DataDirect are both non-iODBC, so the default applies.
    if (isIODBC()) {
        GTEST_SKIP() << "iODBC defaults to UTF-32";
    }
    ASSERT_NO_FATAL_FAILURE(openHandles(SQL_OV_ODBC3));
    SQLINTEGER value = -1;
    // 2) SQL_ATTR_DRIVER_UNICODE_TYPE is still answered, and is still the
    //    attribute the DM actually reads.
    EXPECT_EQ(SQL_SUCCESS,
              SQLGetConnectAttr(m_dbc, SQL_ATTR_DRIVER_UNICODE_TYPE, &value, 0,
                                nullptr));
    // 3) UTF-16 is what the driver's W entry points consume.
    EXPECT_EQ(SQL_DD_CP_UTF16, value);
}
