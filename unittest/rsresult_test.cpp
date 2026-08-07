#include "common.h"
#include "rsodbc.h"
#include <limits>

TEST(RSGetDataTest, PublicNullTargetValuePtrReturnsHY009) {
    RS_STMT_INFO stmt(nullptr);
    SQLLEN indicator = 123;
    SQLLEN internal = (std::numeric_limits<SQLLEN>::min)();
    
    SQLRETURN rc = RS_STMT_INFO::RS_SQLGetData(
        &stmt, 1, SQL_C_CHAR, nullptr, 0, &indicator, FALSE, internal);

    EXPECT_EQ(rc, SQL_ERROR);
    ASSERT_NE(stmt.pErrorList, nullptr);
    // As per spec https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetdata-function?view=sql-server-ver17, 
    // it should return SQL_ERROR with SQLSTATE HY009 indicating invalid use of null pointer.
    EXPECT_STREQ(stmt.pErrorList->szSqlState, "HY009");
    EXPECT_EQ(indicator, 0);
}


// Unit tests for the streaming cursor result lifetime handling introduced
// for the batch refill error path (use after free fix). The PGconn objects
// are allocated by libpq itself (PQconnectStart against a closed port) so
// the tests never depend on the internal struct layout.
extern "C" {
#include "libpq-fe.h"
}

TEST(RSStreamingResultLifetimeTest, ResultReplacedNullConnReportsReplaced) {
    EXPECT_EQ(pqIsConnectionResultReplaced(nullptr, nullptr), 1);
}

TEST(RSStreamingResultLifetimeTest, ResultReplacedDetectsMatchAndDetach) {
    // A freshly started (never completed) connection holds no result.
    PGconn *pgConn = PQconnectStart("host=127.0.0.1 port=1 connect_timeout=1");
    ASSERT_NE(pgConn, nullptr);

    // Match: the caller's pointer equals conn's (NULL) result.
    EXPECT_EQ(pqIsConnectionResultReplaced(pgConn, NULL), 0);

    // Clean detach: at end of stream libpq sets conn->result to NULL
    // without freeing, and the caller's pointer stays live. A NULL
    // connection result is therefore NOT a replacement.
    PGresult *held = PQmakeEmptyPGresult(NULL, PGRES_TUPLES_OK);
    ASSERT_NE(held, nullptr);
    EXPECT_EQ(pqIsConnectionResultReplaced(pgConn, held), 0);

    // The replaced case (conn->result non NULL and different from the
    // held pointer) requires a live server error and is covered by the
    // integration test test_streaming_cursor_mid_result_server_error.

    PQclear(held);
    PQfinish(pgConn);
}

TEST(RSStreamingResultLifetimeTest, ClearAsyncResultOnEmptyConnIsSafe) {
    PGconn *pgConn = PQconnectStart("host=127.0.0.1 port=1 connect_timeout=1");
    ASSERT_NE(pgConn, nullptr);

    // No result attached: the clear must be a harmless no-op, twice.
    pqClearAsyncResult(pgConn);
    pqClearAsyncResult(pgConn);
    EXPECT_EQ(pqIsConnectionResultReplaced(pgConn, NULL), 0);

    PQfinish(pgConn);
}
