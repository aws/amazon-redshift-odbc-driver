#include "common.h"
#include <rsdesc.h>
#include <rsodbc.h>
#include <sql.h>
#include <rsodbc.h>
#include <rs_pq_type.h>

// This unit test is for testing the oid mapping in ODBC 2.x
TEST(OID_MAPPING_TEST_SUITE, test_bool) {
    
    short RsSpecialType;
    EXPECT_TRUE(mapPgTypeToSqlType(BOOLOID, &RsSpecialType) == SQL_BIT);
    
}
TEST(OID_MAPPING_TEST_SUITE, test_char) {
    
    short RsSpecialType;
    EXPECT_TRUE(mapPgTypeToSqlType(CHAROID, &RsSpecialType) == SQL_CHAR);
    
}
TEST(OID_MAPPING_TEST_SUITE, test_int8) {
    
    short RsSpecialType;
    EXPECT_TRUE(mapPgTypeToSqlType(INT8OID, &RsSpecialType) == SQL_BIGINT);
    
}
TEST(OID_MAPPING_TEST_SUITE, test_int2) {
    
    short RsSpecialType;
    EXPECT_TRUE(mapPgTypeToSqlType(INT2OID, &RsSpecialType) == SQL_SMALLINT);
    
}
TEST(OID_MAPPING_TEST_SUITE, test_int4) {
    
    short RsSpecialType;
    EXPECT_TRUE(mapPgTypeToSqlType(INT4OID, &RsSpecialType) == SQL_INTEGER);
    
}
TEST(OID_MAPPING_TEST_SUITE, test_text) {
    
    short RsSpecialType;
    EXPECT_TRUE(mapPgTypeToSqlType(TEXTOID, &RsSpecialType) == SQL_VARCHAR);
    
}
TEST(OID_MAPPING_TEST_SUITE, test_float4) {
    
    short RsSpecialType;
    EXPECT_TRUE(mapPgTypeToSqlType(FLOAT4OID, &RsSpecialType) == SQL_REAL);
    
}
TEST(OID_MAPPING_TEST_SUITE, test_float8) {
    
    short RsSpecialType;
    EXPECT_TRUE(mapPgTypeToSqlType(FLOAT8OID, &RsSpecialType) == SQL_DOUBLE);
    
}
TEST(OID_MAPPING_TEST_SUITE, test_bpchar) {
    
    short RsSpecialType;
    EXPECT_TRUE(mapPgTypeToSqlType(BPCHAROID, &RsSpecialType) == SQL_CHAR);
    
}
TEST(OID_MAPPING_TEST_SUITE, test_varchar) {
    
    short RsSpecialType;
    EXPECT_TRUE(mapPgTypeToSqlType(VARCHAROID, &RsSpecialType) == SQL_VARCHAR);
    
}
TEST(OID_MAPPING_TEST_SUITE, test_time) {
    
    short RsSpecialType;
    EXPECT_TRUE(mapPgTypeToSqlType(TIMEOID, &RsSpecialType) == SQL_TYPE_TIME);
    
}
TEST(OID_MAPPING_TEST_SUITE, test_timestamp) {
    
    short RsSpecialType;
    EXPECT_TRUE(mapPgTypeToSqlType(TIMESTAMPOID, &RsSpecialType) == SQL_TYPE_TIMESTAMP);
    
}
TEST(OID_MAPPING_TEST_SUITE, test_timestamptz) {
    
    short RsSpecialType;
    EXPECT_TRUE(mapPgTypeToSqlType(TIMESTAMPTZOID, &RsSpecialType) == SQL_TYPE_TIMESTAMP);
    EXPECT_TRUE(RsSpecialType == TIMESTAMPTZOID);
    
}
TEST(OID_MAPPING_TEST_SUITE, test_numeric) {
    
    short RsSpecialType;
    EXPECT_TRUE(mapPgTypeToSqlType(NUMERICOID, &RsSpecialType) == SQL_NUMERIC);
    
}