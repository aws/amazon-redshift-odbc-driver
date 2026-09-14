/*-------------------------------------------------------------------------
 *
 * Unit tests for RS_CONN_INFO::parseConnectString() (rsconnect.cpp).
 *
 * Current coverage is password handling; the fixture is intended to be reused
 * for future connection-string parsing cases.
 *
 * A password supplied via the connection-string PWD/Password keyword must be
 * stored whole, however long it is. IAM temporary passwords run to roughly 1900
 * characters, well past the MAX_IDEN_LEN (1025) sizing used for ordinary
 * identifiers, which is why the szPassword buffer is sized
 * PADB_MAX_PARAMETERS (32767); parseConnectString() copies up to the full
 * buffer, matching the SQLConnect path. A password that does not survive
 * parsing intact is rejected by the backend with
 * "28000 FATAL: IAM authentication failed".
 *
 * Runs entirely offline (no database connection required).
 *
 *-------------------------------------------------------------------------
 */

#include "common.h"
#include <rsodbc.h>
#include <rsutil.h>
#include <string>

/*
 * Fixture that builds a minimal RS_CONN_INFO with allocated connection
 * property/attribute structs, enough for parseConnectString() to run.
 */
class ParseConnectStringTest : public ::testing::Test {
  protected:
    RS_ENV_INFO env;
    RS_CONN_INFO *conn = nullptr;

    void SetUp() override {
        conn = new RS_CONN_INFO(&env);
        conn->pConnectProps = new RS_CONNECT_PROPS_INFO();
        conn->pConnAttr = new RS_CONN_ATTR_INFO();
    }

    void TearDown() override {
        // parseConnectString() heap-allocates these sub-structs on demand;
        // RS_CONNECT_PROPS_INFO has no destructor, so free them here to match
        // the disconnect cleanup path and keep the test leak-free under ASAN.
        // RsFree (not rs_free) is used so the allocation is released by the
        // driver DLL's own CRT, matching the other unit tests.
        RS_CONNECT_PROPS_INFO *props = conn->pConnectProps;
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

        delete conn->pConnAttr;
        delete conn->pConnectProps;
        delete conn;

        // RS_ENV_INFO allocates pEnvAttr with new in its constructor and has no
        // destructor, so the fixture's env member would leak it. Free it the
        // way the driver does (delete, see RS_SQLFreeEnv) rather than RsFree,
        // which is only for the buffers parseConnectString() allocates.
        delete env.pEnvAttr;
        env.pEnvAttr = NULL;
    }
};

// A long IAM-style password must be stored in full, not truncated.
TEST_F(ParseConnectStringTest, LongPasswordNotTruncated) {
    const size_t pwdLen = 1900; // representative IAM temp-password length
    std::string longPwd(pwdLen, 'A');
    std::string connStr = "PWD=" + longPwd;

    conn->parseConnectString(const_cast<char *>(connStr.c_str()), SQL_NTS,
                             /*append*/ FALSE, /*onlyDSN*/ FALSE);

    EXPECT_EQ(strlen(conn->pConnectProps->szPassword), pwdLen);
    EXPECT_STREQ(conn->pConnectProps->szPassword, longPwd.c_str());
}

// The "Password" long-form keyword must behave identically.
TEST_F(ParseConnectStringTest, LongPasswordLongKeywordNotTruncated) {
    const size_t pwdLen = 1900;
    std::string longPwd(pwdLen, 'B');
    std::string connStr = "Password=" + longPwd;

    conn->parseConnectString(const_cast<char *>(connStr.c_str()), SQL_NTS,
                             FALSE, FALSE);

    EXPECT_EQ(strlen(conn->pConnectProps->szPassword), pwdLen);
    EXPECT_STREQ(conn->pConnectProps->szPassword, longPwd.c_str());
}

// Boundary case: a password at the MAX_IDEN_LEN identifier sizing, which is
// unrelated to how much password the buffer holds, must not be clipped.
TEST_F(ParseConnectStringTest, PasswordAtIdentifierLengthBoundary) {
    const size_t pwdLen = MAX_IDEN_LEN; // 1025
    std::string pwd(pwdLen, 'C');
    std::string connStr = "PWD=" + pwd;

    conn->parseConnectString(const_cast<char *>(connStr.c_str()), SQL_NTS,
                             FALSE, FALSE);

    EXPECT_EQ(strlen(conn->pConnectProps->szPassword), pwdLen);
    EXPECT_STREQ(conn->pConnectProps->szPassword, pwd.c_str());
}

// An ordinary short password must be stored correctly too.
TEST_F(ParseConnectStringTest, ShortPasswordPreserved) {
    const char *shortPwd = "Testing1234";
    std::string connStr = std::string("PWD=") + shortPwd;

    conn->parseConnectString(const_cast<char *>(connStr.c_str()), SQL_NTS,
                             FALSE, FALSE);

    EXPECT_STREQ(conn->pConnectProps->szPassword, shortPwd);
}

// Guard at the type level: the destination buffer must be large enough to hold
// long passwords in the first place.
TEST_F(ParseConnectStringTest, PasswordBufferSizedForLongPasswords) {
    EXPECT_EQ(sizeof(conn->pConnectProps->szPassword),
              (size_t)PADB_MAX_PARAMETERS);
    EXPECT_GT(sizeof(conn->pConnectProps->szPassword), (size_t)MAX_IDEN_LEN);
}
