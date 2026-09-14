/**
 * Unit tests for libpq portal management API (batched fetch).
 *
 * Tests cover:
 * - PGRES_PORTAL_SUSPENDED enum value correctness
 * - PQsendBindPortal/PQsendExecutePortal/PQsendClosePortal: NULL conn guard
 * - PQsendBindPortal/PQsendExecutePortal/PQsendClosePortal: bad connection guard
 * - PQmakeEmptyPGresult with PGRES_PORTAL_SUSPENDED
 * - PQresStatus returns correct string for PGRES_PORTAL_SUSPENDED
 *
 * Note: Success paths (return 1) and full protocol message construction
 * require a live connection and are covered by integration tests.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>

extern "C" {
#include "libpq-fe.h"
#include "libpq-int.h"
}

// ============================================================================
// PGRES_PORTAL_SUSPENDED Enum Tests
// ============================================================================

TEST(PortalEnumTest, PGRES_PORTAL_SUSPENDED_Exists) {
    ExecStatusType status = PGRES_PORTAL_SUSPENDED;
    EXPECT_NE(status, PGRES_EMPTY_QUERY);
    EXPECT_NE(status, PGRES_COMMAND_OK);
    EXPECT_NE(status, PGRES_TUPLES_OK);
    EXPECT_NE(status, PGRES_BAD_RESPONSE);
    EXPECT_NE(status, PGRES_NONFATAL_ERROR);
    EXPECT_NE(status, PGRES_FATAL_ERROR);
}

TEST(PortalEnumTest, PGRES_PORTAL_SUSPENDED_ValueAfterFatalError) {
    EXPECT_GT((int)PGRES_PORTAL_SUSPENDED, (int)PGRES_FATAL_ERROR);
}

// ============================================================================
// NULL Conn Guard Tests
// ============================================================================

TEST(PQsendBindPortalTest, NullConn_ReturnsZero) {
    int result = PQsendBindPortal(NULL, "stmt", "portal", 0, NULL, NULL, NULL, 0);
    EXPECT_EQ(result, 0);
}

TEST(PQsendExecutePortalTest, NullConn_ReturnsZero) {
    int result = PQsendExecutePortal(NULL, "portal", 100);
    EXPECT_EQ(result, 0);
}

TEST(PQsendClosePortalTest, NullConn_ReturnsZero) {
    int result = PQsendClosePortal(NULL, "portal");
    EXPECT_EQ(result, 0);
}

TEST(PQsendCloseStatementTest, NullConn_ReturnsZero) {
    int result = PQsendCloseStatement(NULL, "stmt");
    EXPECT_EQ(result, 0);
}

// ============================================================================
// PGresult Status Tests for Portal Suspended
// ============================================================================

TEST(PortalResultTest, MakeEmptyResult_PortalSuspended) {
    PGresult *res = PQmakeEmptyPGresult(NULL, PGRES_PORTAL_SUSPENDED);
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(PQresultStatus(res), PGRES_PORTAL_SUSPENDED);
    PQclear(res);
}

TEST(PortalResultTest, ResultStatus_NotTuplesOK) {
    PGresult *res = PQmakeEmptyPGresult(NULL, PGRES_PORTAL_SUSPENDED);
    ASSERT_NE(res, nullptr);
    EXPECT_NE(PQresultStatus(res), PGRES_TUPLES_OK);
    EXPECT_NE(PQresultStatus(res), PGRES_COMMAND_OK);
    PQclear(res);
}

TEST(PortalResultTest, ResStatus_ReturnsString) {
    const char *str = PQresStatus(PGRES_PORTAL_SUSPENDED);
    ASSERT_NE(str, nullptr);
    EXPECT_STREQ(str, "PGRES_PORTAL_SUSPENDED");
}

// ============================================================================
// API Tests: Bad Connection Guard
// Uses PQconnectStart("") to get a properly initialized PGconn in
// CONNECTION_BAD state. Verifies each function rejects a dead connection.
// ============================================================================

class PortalAPITest : public ::testing::Test {
protected:
    PGconn *conn;

    void SetUp() override {
        conn = PQconnectStart("");
        ASSERT_NE(conn, nullptr);
    }

    void TearDown() override {
        if (conn) {
            PQfinish(conn);
        }
    }
};

TEST_F(PortalAPITest, Bind_BadConnection_ReturnsZero) {
    int result = PQsendBindPortal(conn, "stmt", "portal", 0, NULL, NULL, NULL, 0);
    EXPECT_EQ(result, 0);
}

TEST_F(PortalAPITest, Execute_BadConnection_ReturnsZero) {
    int result = PQsendExecutePortal(conn, "portal", 100);
    EXPECT_EQ(result, 0);
}

TEST_F(PortalAPITest, Close_BadConnection_ReturnsZero) {
    int result = PQsendClosePortal(conn, "portal");
    EXPECT_EQ(result, 0);
}

