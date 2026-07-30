/*
 * Unit tests for SQLCancel bWasExecuting detection logic and
 * libpqIsCommandInFlight null-safety.
 *
 * These tests use stack-allocated RS_STMT_INFO / RS_EXEC_THREAD_INFO /
 * RS_CONN_INFO with no live PGconn.
 *
 * Coverage:
 *   - pExecThread->rc == SQL_STILL_EXECUTING: fully testable — SQLCancel
 *     reaches libpqCancelQuery which returns SQL_ERROR (HY000) when pgConn
 *     is NULL, proving bWasExecuting was true.
 *   - libpqIsCommandInFlight NULL-guard (pConn/pgConn == NULL → FALSE):
 *     testable — verified via the LibpqIsCommandInFlight suite below.
 */

#include "common.h"
#include "rsodbc.h"
#include "rsexecute.h"
#include "rsutil.h"
#include <cstring>

// Helper: RS_STMT_INFO with no connection, no results
static RS_STMT_INFO makeIdleStmt() {
    RS_STMT_INFO stmt(nullptr);
    stmt.iStatus = RS_EXECUTE_STMT;
    stmt.pResultHead = nullptr;
    stmt.pExecThread = nullptr;
    return stmt;
}

// ============================================================================
// When pExecThread is NULL (no async thread), SQLCancel should be a no-op
// (bWasExecuting=false). With no connection, libpqIsCommandInFlight=false,
// so SQLCancel falls through to the idle branch and returns SQL_SUCCESS.
// ============================================================================
TEST(SQLCancelUnitTest, IdleStatement_NoExecThread_ReturnsSuccess) {
    RS_STMT_INFO stmt = makeIdleStmt();
    SQLRETURN rc = SQLCancel((SQLHSTMT)&stmt);
    EXPECT_EQ(rc, SQL_SUCCESS);
}

// ============================================================================
// When pExecThread->rc == SQL_STILL_EXECUTING, bWasExecuting should be true
// and SQLCancel should attempt to cancel (reaching libpqCancelQuery).
// With no live connection, libpqCancelQuery returns SQL_ERROR (HY000).
// SQL_ERROR here proves the cancel was attempted — bWasExecuting was true.
// ============================================================================
TEST(SQLCancelUnitTest, ExecThread_StillExecuting_AttemptsCancelAndFails) {
    RS_STMT_INFO stmt = makeIdleStmt();
    RS_EXEC_THREAD_INFO execThread;
    execThread.rc = SQL_STILL_EXECUTING;
    stmt.pExecThread = &execThread;
    stmt.iStatus = RS_EXECUTE_STMT;

    SQLRETURN rc = SQLCancel((SQLHSTMT)&stmt);

    // libpqCancelQuery returns SQL_ERROR when pgConn is NULL —
    // proves bWasExecuting was true (cancel was attempted)
    EXPECT_EQ(rc, SQL_ERROR);
    ASSERT_NE(stmt.pErrorList, nullptr);
    EXPECT_STREQ(stmt.pErrorList->szSqlState, "HY000");
}

// ============================================================================
// When pExecThread->rc != SQL_STILL_EXECUTING (thread finished but unjoined),
// bWasExecuting should be false — SQLCancel is a no-op and returns SQL_SUCCESS.
// ============================================================================
TEST(SQLCancelUnitTest, ExecThread_Finished_NotStillExecuting_NoCancel) {
    RS_STMT_INFO stmt = makeIdleStmt();
    RS_EXEC_THREAD_INFO execThread;
    execThread.rc = SQL_SUCCESS;  // thread finished
    stmt.pExecThread = &execThread;
    stmt.iStatus = RS_EXECUTE_STMT;

    SQLRETURN rc = SQLCancel((SQLHSTMT)&stmt);
    EXPECT_EQ(rc, SQL_SUCCESS);
}

// ============================================================================
// RS_EXECUTE_STMT_NEED_DATA always sets bWasExecuting=true (DAE path).
// With no connection this falls into the DAE reset branch (client-side only),
// which resets state and returns SQL_SUCCESS without touching libpq.
// ============================================================================
TEST(SQLCancelUnitTest, NeedData_SetsWasExecuting_ResetsState) {
    RS_STMT_INFO stmt = makeIdleStmt();
    stmt.iStatus = RS_EXECUTE_STMT_NEED_DATA;
    stmt.iExecutePreparedDataAtExec = 0;

    SQLRETURN rc = SQLCancel((SQLHSTMT)&stmt);
    EXPECT_EQ(rc, SQL_SUCCESS);
    // State should be reset to RS_ALLOCATE_STMT
    EXPECT_EQ(stmt.iStatus, RS_ALLOCATE_STMT);
}

// ============================================================================
// Unit tests for libpqIsCommandInFlight null-safety.
// ============================================================================

TEST(LibpqIsCommandInFlightTest, NullConn_ReturnsFalse) {
    EXPECT_EQ(libpqIsCommandInFlight(nullptr), FALSE);
}

TEST(LibpqIsCommandInFlightTest, NullPgConn_ReturnsFalse) {
    RS_ENV_INFO env;
    RS_CONN_INFO conn(&env);
    conn.pgConn = nullptr;
    EXPECT_EQ(libpqIsCommandInFlight(&conn), FALSE);
}

