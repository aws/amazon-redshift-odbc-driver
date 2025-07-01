#include "common.h"
#include <rsdesc.h>
#include <rsutil.h>
#include <rsodbc.h>
#include <sql.h>
#include <map>


char* name(short value){
#define NAME(TYPE) case TYPE: return #TYPE;
    switch (value) {
        NAME(SQL_CHAR)
        NAME(SQL_VARCHAR)
        NAME(SQL_LONGVARCHAR)
        NAME(SQL_WCHAR)
        NAME(SQL_WVARCHAR)
        NAME(SQL_WLONGVARCHAR)
        NAME(SQL_DECIMAL)
        NAME(SQL_NUMERIC)
        NAME(SQL_BIT)
        NAME(SQL_TINYINT)
        NAME(SQL_SMALLINT)
        NAME(SQL_INTEGER)
        NAME(SQL_BIGINT)
        NAME(SQL_REAL)
        NAME(SQL_FLOAT)
        NAME(SQL_DOUBLE)
        NAME(SQL_BINARY)
        NAME(SQL_VARBINARY)
        NAME(SQL_LONGVARBINARY)
        NAME(SQL_TYPE_DATE)
        NAME(SQL_TYPE_TIME)
        NAME(SQL_TYPE_TIMESTAMP)
    }
    return "unknown";
#undef NAME
}

std::map<short, short> mp = {
    {SQL_CHAR, SQL_C_CHAR},
    {SQL_VARCHAR, SQL_C_CHAR},
    {SQL_LONGVARCHAR, SQL_C_CHAR},
    {SQL_WCHAR, SQL_C_WCHAR},
    {SQL_WVARCHAR, SQL_C_WCHAR},
    {SQL_WLONGVARCHAR, SQL_C_WCHAR},
    {SQL_DECIMAL, SQL_C_CHAR},
    {SQL_NUMERIC, SQL_C_CHAR},
    {SQL_BIT, SQL_C_BIT},
    {SQL_TINYINT, SQL_C_TINYINT},
    {SQL_SMALLINT, SQL_C_SHORT},
    {SQL_INTEGER, SQL_C_LONG},
    {SQL_BIGINT, SQL_C_SBIGINT},
    {SQL_REAL, SQL_C_FLOAT},
    {SQL_FLOAT, SQL_C_DOUBLE},
    {SQL_DOUBLE, SQL_C_DOUBLE},
    {SQL_BINARY, SQL_C_BINARY},
    {SQL_VARBINARY, SQL_C_BINARY},
    {SQL_LONGVARBINARY, SQL_C_BINARY},
    {SQL_TYPE_DATE, SQL_C_TYPE_DATE},
    {SQL_TYPE_TIME, SQL_C_TYPE_TIME},
    {SQL_TYPE_TIMESTAMP, SQL_C_TYPE_TIMESTAMP},
    {SQL_DATE, SQL_C_DATE},
    {SQL_TIME, SQL_C_TIME},
    {SQL_TIMESTAMP, SQL_C_TIMESTAMP}
};

class test_default_ctype_conversion : public::testing::TestWithParam<short> {

};

// This unit test is for testing the default C type for given SQL type
TEST_P(test_default_ctype_conversion, TestWithParam) {

    int conversionError = FALSE;;
    short sqlType = GetParam();
    EXPECT_TRUE(getDefaultCTypeFromSQLType(sqlType, &conversionError) == mp[sqlType]) << "error C default mapping for SQL type: " << name(sqlType);
    EXPECT_FALSE(conversionError) << "Expected conversionError to be false for SQL type: " << name(sqlType);
    
}

INSTANTIATE_TEST_SUITE_P(DEFAULT_CTYPE_TEST_SUITE, test_default_ctype_conversion, ::testing::Values(SQL_CHAR, SQL_VARCHAR, SQL_LONGVARCHAR, SQL_WCHAR, SQL_WVARCHAR, SQL_WLONGVARCHAR, SQL_DECIMAL, SQL_NUMERIC, SQL_BIT, SQL_TINYINT, SQL_SMALLINT, SQL_INTEGER, SQL_BIGINT, SQL_REAL, SQL_FLOAT, SQL_DOUBLE, SQL_BINARY, SQL_VARBINARY, SQL_LONGVARBINARY, SQL_TYPE_DATE, SQL_TYPE_TIME, SQL_TYPE_TIMESTAMP, SQL_DATE, SQL_TIME, SQL_TIMESTAMP));