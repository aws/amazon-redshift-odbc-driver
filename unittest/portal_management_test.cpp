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

// ============================================================================
// Protocol Version Rejection Tests
// Forces CONNECTION_OK with pversion < 3 to test the protocol guard.
// ============================================================================

// ============================================================================
// NOTE: Protocol version rejection (PG_PROTOCOL_MAJOR < 3 guard) requires
// CONNECTION_OK state which cannot be safely faked in unit tests without
// fully initializing PGconn internals. This guard is exercised by the
// integration tests against a live Redshift cluster.
// ============================================================================

// ============================================================================
// Query Eligibility Tests for Portal Fetch (isQueryEligibleForPortalFetch)
// ============================================================================

// Declared in rsodbc.h, implemented in rslibpq.c (C linkage)
extern "C" int isQueryEligibleForPortalFetch(const char *pszCmd);

TEST(PortalEligibilityTest, NullQuery_NotEligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch(NULL), 0);
}

TEST(PortalEligibilityTest, EmptyQuery_NotEligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch(""), 0);
}

TEST(PortalEligibilityTest, SimpleSelect_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT * FROM t"), 1);
}

TEST(PortalEligibilityTest, SelectWithLeadingWhitespace_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("   SELECT 1"), 1);
    EXPECT_EQ(isQueryEligibleForPortalFetch("\t\nSELECT 1"), 1);
}

TEST(PortalEligibilityTest, SelectCaseInsensitive_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("select * from t"), 1);
    EXPECT_EQ(isQueryEligibleForPortalFetch("Select * from t"), 1);
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT * from t"), 1);
}

TEST(PortalEligibilityTest, SelectWithTrailingSemicolon_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT 1;"), 1);
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT 1;  "), 1);
}

TEST(PortalEligibilityTest, SelectWithSubquery_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT(1)"), 1);
}

TEST(PortalEligibilityTest, InsertQuery_NotEligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("INSERT INTO t VALUES(1)"), 0);
}

TEST(PortalEligibilityTest, UpdateQuery_NotEligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("UPDATE t SET x=1"), 0);
}

TEST(PortalEligibilityTest, DeleteQuery_NotEligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("DELETE FROM t"), 0);
}

TEST(PortalEligibilityTest, CreateQuery_NotEligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("CREATE TABLE t(x int)"), 0);
}

TEST(PortalEligibilityTest, MultiStatement_NotEligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT 1; SELECT 2"), 0);
}

TEST(PortalEligibilityTest, SemicolonInsideQuotes_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT 'a;b' FROM t"), 1);
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT \"col;name\" FROM t"), 1);
}

TEST(PortalEligibilityTest, CallStatement_NotEligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("CALL my_proc()"), 0);
}

TEST(PortalEligibilityTest, SelectInto_NotEligible) {
    // Both forms of SELECT INTO should be excluded
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT id, val INTO newtab FROM t"), 0);
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT INTO newtab id, val FROM t"), 0);
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT a, b INTO TEMPORARY TABLE tmp FROM src"), 0);
    EXPECT_EQ(isQueryEligibleForPortalFetch("select col into temp_table from source"), 0);
}

TEST(PortalEligibilityTest, SelectInto_InSubquery_Eligible) {
    // INTO after top-level FROM is not SELECT INTO — it's a normal SELECT
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT * FROM t WHERE id IN (SELECT 1)"), 1);
    // Subquery with FROM before the real INTO — paren depth should prevent false match
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT (SELECT max(x) FROM t2) INTO newtab FROM t1"), 0);
}

TEST(PortalEligibilityTest, SelectInto_InsideComment_Eligible) {
    // INTO inside a comment should be ignored — query is a normal SELECT
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT /* INTO */ col FROM t"), 1);
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT col -- INTO\n FROM t"), 1);
}

TEST(PortalEligibilityTest, SelectInto_WordBoundary_Eligible) {
    // "selectinto" or "PINTO" — not a keyword match
    EXPECT_EQ(isQueryEligibleForPortalFetch("selectinto"), 0);  // fails SELECT prefix check
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT PINTO FROM t"), 1);  // INTO not at word boundary
}

TEST(PortalEligibilityTest, WithCTE_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("WITH cte AS (SELECT 1) SELECT * FROM cte"), 1);
    EXPECT_EQ(isQueryEligibleForPortalFetch("with tmp as (select id from t) select * from tmp"), 1);
    EXPECT_EQ(isQueryEligibleForPortalFetch("  WITH a AS (SELECT 1), b AS (SELECT 2) SELECT * FROM a, b"), 1);
}

/// INTO followed immediately by a quoted destination identifier is SELECT INTO.
TEST(PortalEligibilityTest, SelectInto_QuotedDestination_NotEligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch(
        "SELECT id INTO\"portal_select_into_dest\" FROM source"), 0);
}

TEST(PortalEligibilityTest, WithCTE_MultiStatement_NotEligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("WITH cte AS (SELECT 1) SELECT * FROM cte; SELECT 2"), 0);
}

// ============================================================================
// PostgreSQL/Redshift string literal forms
// ============================================================================

/// Dollar-quoted bodies may contain SQL punctuation and keywords.
TEST(PortalEligibilityTest, DollarQuotedLiteral_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch(
        "SELECT $$; INTO UPDATE DELETE INSERT$$ AS value FROM t"), 1);
    EXPECT_EQ(isQueryEligibleForPortalFetch(
        "SELECT $tag$; INTO UPDATE DELETE INSERT$tag$ AS value FROM t"), 1);
}

/// SQL after a dollar-quoted literal remains a separate statement.
TEST(PortalEligibilityTest, DollarQuotedLiteral_MultiStatement_NotEligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch(
        "SELECT $$text; INTO$$ AS value FROM t; SELECT 2"), 0);
}

/// A backslash-escaped quote in an E-string does not end the literal early.
TEST(PortalEligibilityTest, EscapeStringLiteral_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch(
        "SELECT E'\\'; INTO UPDATE' AS value FROM t"), 1);
}

/// A real semicolon after an E-string still makes the query multi-statement.
TEST(PortalEligibilityTest, EscapeStringLiteral_MultiStatement_NotEligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch(
        "SELECT E'\\''; INTO' AS value FROM t; SELECT 2"), 0);
}

TEST(PortalEligibilityTest, WithCTE_SemicolonInComment_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("WITH cte AS (SELECT 1 /* ; */) SELECT * FROM cte"), 1);
}

// ============================================================================
// WITH data-modifying CTE detection
// ============================================================================

/// Verifies UPDATE inside a CTE is rejected.
TEST(PortalEligibilityTest, WithCTE_DataModifyingUpdate_NotEligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch(
        "WITH x AS (UPDATE t SET col=1 RETURNING *) SELECT * FROM x"), 0);
}

/// Verifies DELETE inside a CTE is rejected.
TEST(PortalEligibilityTest, WithCTE_DataModifyingDelete_NotEligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch(
        "WITH x AS (DELETE FROM t RETURNING *) SELECT * FROM x"), 0);
}

/// Verifies INSERT inside a CTE is rejected.
TEST(PortalEligibilityTest, WithCTE_DataModifyingInsert_NotEligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch(
        "WITH x AS (INSERT INTO t VALUES(1) RETURNING *) SELECT * FROM x"), 0);
}

/// Verifies SELECT INTO after a WITH clause is rejected.
TEST(PortalEligibilityTest, WithCTE_SelectInto_NotEligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch(
        "WITH x AS (SELECT 1) SELECT * INTO newtab FROM x"), 0);
}

/// Verifies a DML keyword nested in a subquery within a CTE does not false-positive.
TEST(PortalEligibilityTest, WithCTE_KeywordInSubquery_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch(
        "WITH x AS (SELECT * FROM t WHERE id IN (SELECT 1)) SELECT * FROM x"), 1);
}

/// Verifies a table named with a DML keyword substring does not false-positive.
TEST(PortalEligibilityTest, WithCTE_TableNameContainsKeyword_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch(
        "WITH x AS (SELECT * FROM updates_log) SELECT * FROM x"), 1);
}

// ============================================================================
// INTO inside string literal (should not trigger SELECT INTO exclusion)
// ============================================================================

/// INTO inside a single-quoted string is not a keyword.
TEST(PortalEligibilityTest, IntoInsideSingleQuotedString_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT 'INSERT INTO t' AS label FROM t"), 1);
}

/// INTO inside a double-quoted identifier is not a keyword.
TEST(PortalEligibilityTest, IntoInsideDoubleQuotedIdentifier_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT \"into\" FROM t"), 1);
}

// ============================================================================
// Semicolons inside comments (should not trigger multi-statement rejection)
// ============================================================================

/// Semicolon inside a line comment is not a statement separator.
TEST(PortalEligibilityTest, SemicolonInLineComment_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT 1 -- x;y\n FROM t"), 1);
}

/// Semicolon inside a block comment is not a statement separator.
TEST(PortalEligibilityTest, SemicolonInBlockComment_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT 1 /* ; */ FROM t"), 1);
}

/// Only comments after trailing semicolon is not multi-statement.
TEST(PortalEligibilityTest, CommentOnlyAfterSemicolon_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT 1; /* done */"), 1);
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT 1; -- end"), 1);
}

// ============================================================================
// INTO left boundary characters (comma and slash)
// ============================================================================

/// INTO preceded by a comma is detected as SELECT INTO.
TEST(PortalEligibilityTest, IntoAfterComma_NotEligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT a,b INTO newtab FROM t"), 0);
}

/// INTO preceded by a block comment close is detected as SELECT INTO.
TEST(PortalEligibilityTest, IntoAfterBlockComment_NotEligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT a/**/INTO newtab FROM t"), 0);
}

// ============================================================================
// Leading whitespace variations
// ============================================================================

/// Leading carriage return is treated as whitespace.
TEST(PortalEligibilityTest, LeadingCarriageReturn_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("\r\nSELECT 1"), 1);
    EXPECT_EQ(isQueryEligibleForPortalFetch("\rSELECT 1"), 1);
}

// ============================================================================
// PQsendExecutePortalResume Tests
// ============================================================================

extern "C" int PQsendExecutePortalResume(PGconn *conn, const char *portalName, int maxRows);

TEST(PQsendExecutePortalResumeTest, NullConn_ReturnsZero) {
    int result = PQsendExecutePortalResume(NULL, "portal", 100);
    EXPECT_EQ(result, 0);
}

TEST_F(PortalAPITest, Resume_BadConnection_ReturnsZero) {
    int result = PQsendExecutePortalResume(conn, "portal", 100);
    EXPECT_EQ(result, 0);
}

// ============================================================================
// Skip function coverage: paths only exercised indirectly through eligibility
// ============================================================================

/// Doubled single-quote inside a literal does not end the string early.
TEST(PortalEligibilityTest, DoubledSingleQuote_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT 'it''s' AS val FROM t"), 1);
}

/// Unterminated single-quoted literal consumes to end — no false semicolon.
TEST(PortalEligibilityTest, UnterminatedSingleQuote_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT 'unterminated FROM t"), 1);
}

/// Doubled double-quote inside an identifier does not end the identifier early.
TEST(PortalEligibilityTest, DoubledDoubleQuote_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT \"col\"\"name\" FROM t"), 1);
}

/// Unterminated double-quoted identifier consumes to end — no false semicolon.
TEST(PortalEligibilityTest, UnterminatedDoubleQuote_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT \"unterminated FROM t"), 1);
}

/// Backslash inside a non-escape (standard) single-quoted string is literal.
TEST(PortalEligibilityTest, BackslashInStandardString_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT 'path\\;to' FROM t"), 1);
}

/// Dollar-quoted string with digits in the tag is handled.
TEST(PortalEligibilityTest, DollarQuotedTagWithDigits_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch(
        "SELECT $a1$; INTO UPDATE$a1$ AS value FROM t"), 1);
}

/// Unterminated dollar-quoted string consumes to end.
TEST(PortalEligibilityTest, UnterminatedDollarQuoted_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch(
        "SELECT $$unterminated; INTO FROM t"), 1);
}

/// A lone $ that is not a valid dollar-quote opener is not consumed.
TEST(PortalEligibilityTest, LoneDollarSign_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT $1 FROM t"), 1);
}

/// E-string backslash at end of string does not read past buffer.
TEST(PortalEligibilityTest, EscapeStringBackslashAtEnd_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT E'trailing\\' FROM t"), 1);
}

/// Dollar sign followed by invalid tag start char is not a dollar-quote.
TEST(PortalEligibilityTest, DollarSignInvalidTagStart_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT $!foo FROM t"), 1);
}

/// Dollar-quoted tag that doesn't close (e.g. $abc without second $) is not a dollar-quote.
TEST(PortalEligibilityTest, DollarTagNeverClosed_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT $abc FROM t"), 1);
}

/// Line comment at EOF (no trailing newline) is consumed correctly.
TEST(PortalEligibilityTest, LineCommentAtEOF_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT 1 -- comment"), 1);
}

/// Unterminated block comment consumes to end — no false semicolon.
TEST(PortalEligibilityTest, UnterminatedBlockComment_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch("SELECT 1 /* unterminated"), 1);
}

/// WITH query stops scanning at FROM when checking for top-level INTO.
TEST(PortalEligibilityTest, WithCTE_IntoAfterFrom_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch(
        "WITH cte AS (SELECT 1) SELECT * FROM into_table"), 1);
}

/// SELECT with INTO keyword appearing only after FROM is eligible.
TEST(PortalEligibilityTest, SelectIntoAfterFrom_Eligible) {
    EXPECT_EQ(isQueryEligibleForPortalFetch(
        "SELECT * FROM into_results WHERE id > 0"), 1);
}
