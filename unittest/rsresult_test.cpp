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