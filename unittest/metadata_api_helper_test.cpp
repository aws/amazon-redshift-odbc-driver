#include "common.h"
#include <rsdesc.h>
#include <rsodbc.h>
#include <sql.h>
#include <map>
#include <vector>
#include <rsMetadataAPIHelper.h>
//#include <rsutil.h>

#define expectedSQLTablesColNum 5
char* expectedSQLTablesCol[expectedSQLTablesColNum] = {"TABLE_CAT", "TABLE_SCHEM", "TABLE_NAME", "TABLE_TYPE", "REMARKS"};
int expectedSQLTablesColDataType[expectedSQLTablesColNum] = {VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID};

#define expectedSQLColumnsColNum 18
char* expectedSQLColumnsCol[expectedSQLColumnsColNum] = {"TABLE_CAT", "TABLE_SCHEM", "TABLE_NAME", "COLUMN_NAME", "DATA_TYPE",
                            "TYPE_NAME", "COLUMN_SIZE", "BUFFER_LENGTH", "DECIMAL_DIGITS", "NUM_PREC_RADIX",
                            "NULLABLE", "REMARKS", "COLUMN_DEF", "SQL_DATA_TYPE", "SQL_DATETIME_SUB",
                            "CHAR_OCTET_LENGTH", "ORDINAL_POSITION", "IS_NULLABLE"};
int expectedSQLColumnsDatatype[expectedSQLColumnsColNum] = {VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID, INT2OID,
                                                    VARCHAROID,INT4OID,INT4OID,INT2OID,INT2OID,
                                                    INT2OID,VARCHAROID,VARCHAROID,INT2OID,INT2OID,
                                                    INT4OID,INT4OID,VARCHAROID};

#define character_maximum_length 512
#define numeric_precision 5
#define numeric_scale 5
std::map<std::string, std::string> dataType2RsTypeMap = {
    {"smallint", "int2"},
    {"integer", "int4"},
    {"bigint", "int8"},
    {"numeric", "numeric"},
    {"real", "float4"},
    {"double precision", "float8"},
    {"boolean", "bool"},
    {"character", "char"},
    {"character varying", "varchar"},
    {"date", "date"},
    {"time without time zone", "time"},
    {"time with time zone", "timetz"},
    {"timestamp without time zone", "timestamp"},
    {"timestamp with time zone", "timestamptz"},
    {"interval year to month", "intervaly2m"},
    {"interval day to second", "intervald2s"},
    {"super", "super"},
    {"geometry", "geometry"},
    {"geography", "geography"}
};

#define SQLTYPE 0
#define SQLDATATYPE 1
#define SQLDATESUB 2
#define COLUMNSIZE 3
#define DECIMALDIGIT 4
struct DATA_TYPE_RES{
  short sqlType;
  short sqlDataType;
  short sqlDateSub;
  short colSize;
  short decimalDigit;
  int bufferLen;
};

std::map<std::string, DATA_TYPE_RES> dataTypeMap = {
    {"smallint", {SQL_SMALLINT, SQL_SMALLINT, kNotApplicable, 5, kNotApplicable, sizeof(short)}},
    {"integer", {SQL_INTEGER, SQL_INTEGER, kNotApplicable, 10, kNotApplicable, sizeof(int)}},
    {"bigint", {SQL_BIGINT, SQL_BIGINT, kNotApplicable, 19, kNotApplicable, sizeof(long long)}},
    {"numeric", {SQL_NUMERIC, SQL_NUMERIC, kNotApplicable, numeric_precision, numeric_scale, 8}},
    {"real", {SQL_REAL, SQL_REAL, kNotApplicable, 7, 6, sizeof(float)}},
    {"double precision", {SQL_DOUBLE, SQL_DOUBLE, kNotApplicable, 15, 15, sizeof(double)}},
    {"boolean", {SQL_BIT, SQL_BIT, kNotApplicable, 1, kNotApplicable, sizeof(bool)}},
    {"character", {SQL_CHAR, SQL_CHAR, kNotApplicable, character_maximum_length, kNotApplicable, character_maximum_length}},
    {"character varying", {SQL_VARCHAR, SQL_VARCHAR, kNotApplicable, character_maximum_length, kNotApplicable, character_maximum_length}},
    {"date", {SQL_TYPE_DATE, SQL_DATETIME, SQL_CODE_DATE, 10, kNotApplicable, sizeof(SQL_DATE_STRUCT)}},
    {"time without time zone", {SQL_TYPE_TIME, SQL_DATETIME, SQL_CODE_TIME, 15, 6, sizeof(SQL_TIME_STRUCT)}},
    {"time with time zone", {SQL_TYPE_TIME, SQL_DATETIME, SQL_CODE_TIME, 21, 6, sizeof(SQL_TIME_STRUCT)}},
    {"timestamp without time zone", {SQL_TYPE_TIMESTAMP, SQL_DATETIME, SQL_CODE_TIMESTAMP, 29, 6, sizeof(SQL_TIMESTAMP_STRUCT)}},
    {"timestamp with time zone", {SQL_TYPE_TIMESTAMP, SQL_DATETIME, SQL_CODE_TIMESTAMP, 35, 6, sizeof(SQL_TIMESTAMP_STRUCT)}},
    {"interval year to month", {SQL_INTERVAL_YEAR_TO_MONTH, SQL_INTERVAL, SQL_CODE_YEAR_TO_MONTH, 32, kNotApplicable, sizeof(INTERVALY2M_STRUCT)}},
    {"interval day to second", {SQL_INTERVAL_DAY_TO_SECOND, SQL_INTERVAL, SQL_CODE_DAY_TO_SECOND, 64, 6, sizeof(INTERVALD2S_STRUCT)}},
    {"super", {SQL_LONGVARCHAR, SQL_LONGVARCHAR, kNotApplicable, 0, kNotApplicable, kUnknownColumnSize}},
    {"geometry", {SQL_LONGVARCHAR, SQL_LONGVARCHAR, kNotApplicable, 0, kNotApplicable, kUnknownColumnSize}},
    {"geography", {SQL_LONGVARCHAR, SQL_LONGVARCHAR, kNotApplicable, 0, kNotApplicable, kUnknownColumnSize}}
};


// Unit test for pre-defined constant for SQLTables in class RsMetadataAPIHelper
TEST(METADATA_API_HELPER_TEST_SUITE, test_sqltables_col_num) {
    ASSERT_EQ(kTablesColNum, expectedSQLTablesColNum);
}

class test_sqltables_col :public ::testing::TestWithParam<short> {

};

TEST_P(test_sqltables_col, test_pre_defined_constant) {
    short column_index = GetParam();
    ASSERT_EQ(RsMetadataAPIHelper::kTablesCol[column_index], expectedSQLTablesCol[column_index]) << "metadata API SQLTables Column index: " << column_index << ", has wrong column name: " << RsMetadataAPIHelper::kTablesCol[column_index] << " (Expect: " << expectedSQLTablesCol[column_index] << ")";
    
    short RsSpecialType;
    short sqlType = mapPgTypeToSqlType(RsMetadataAPIHelper::kTablesColDatatype[column_index], &RsSpecialType);
    short expectedSqlType = mapPgTypeToSqlType(expectedSQLTablesColDataType[column_index], &RsSpecialType);
    ASSERT_EQ(RsMetadataAPIHelper::kTablesColDatatype[column_index], expectedSQLTablesColDataType[column_index]) << "metadata API SQLTables Column index: " << column_index << ", has wrong column data type: " << sqlTypeNameMap(sqlType) << " (Expect: " << sqlTypeNameMap(expectedSqlType) << ")";
}
INSTANTIATE_TEST_SUITE_P(METADATA_API_HELPER_TEST_SUITE, test_sqltables_col, ::testing::Values(kSQLTables_TABLE_CATALOG, kSQLTables_TABLE_SCHEM, kSQLTables_TABLE_NAME, kSQLTables_TABLE_TYPE, kSQLTables_REMARKS));


// Unit test for pre-defined constant for SQLColumns in class RsMetadataAPIHelper
TEST(METADATA_API_HELPER_TEST_SUITE, test_sqlcolumns_col_num) {
    ASSERT_EQ(kColumnsColNum, expectedSQLColumnsColNum);
}

class test_sqlcolumns_col :public ::testing::TestWithParam<short> {

};

TEST_P(test_sqlcolumns_col, test_pre_defined_constant) {
    short column_index = GetParam();
    ASSERT_EQ(RsMetadataAPIHelper::kColumnsCol[column_index], expectedSQLColumnsCol[column_index]) << "metadata API SQLTables Column index: " << column_index << ", has wrong column name: " << RsMetadataAPIHelper::kTablesCol[column_index] << " (Expect: " << expectedSQLColumnsCol[column_index] << ")";
    
    short RsSpecialType;
    short sqlType = mapPgTypeToSqlType(RsMetadataAPIHelper::kColumnsColDatatype[column_index], &RsSpecialType);
    short expectedSqlType = mapPgTypeToSqlType(expectedSQLColumnsDatatype[column_index], &RsSpecialType);
    ASSERT_EQ(RsMetadataAPIHelper::kColumnsColDatatype[column_index], expectedSQLColumnsDatatype[column_index]) << "metadata API SQLTables Column index: " << column_index << ", has wrong column data type: " << sqlTypeNameMap(sqlType) << " (Expect: " << sqlTypeNameMap(expectedSqlType) << ")";
}
INSTANTIATE_TEST_SUITE_P(METADATA_API_HELPER_TEST_SUITE, test_sqlcolumns_col, ::testing::Values(kSQLColumns_TABLE_CAT, kSQLColumns_TABLE_SCHEM, kSQLColumns_TABLE_NAME, kSQLColumns_COLUMN_NAME, kSQLColumns_DATA_TYPE, kSQLColumns_TYPE_NAME, kSQLColumns_COLUMN_SIZE, kSQLColumns_BUFFER_LENGTH, kSQLColumns_DECIMAL_DIGITS, kSQLColumns_NUM_PREC_RADIX, kSQLColumns_NULLABLE, kSQLColumns_REMARKS, kSQLColumns_COLUMN_DEF, kSQLColumns_SQL_DATA_TYPE, kSQLColumns_SQL_DATETIME_SUB, kSQLColumns_CHAR_OCTET_LENGTH, kSQLColumns_ORDINAL_POSITION, kSQLColumns_IS_NULLABLE));


// Unit test for helper function getNullable
TEST(METADATA_API_HELPER_TEST_SUITE, test_getnullable) {
    ASSERT_EQ(RsMetadataAPIHelper::getNullable("YES"), 1);
    ASSERT_EQ(RsMetadataAPIHelper::getNullable("NO"), 0);
    ASSERT_EQ(RsMetadataAPIHelper::getNullable("ELSE"), 2);
}

// Unit test for helper function getRSType
class test_getrstype :public ::testing::TestWithParam<std::string> {

};

TEST_P(test_getrstype, test_rsType) {
    std::string dataType = GetParam();
    short sqlType = 0, sqlDataType = 0, sqlDateSub = 0;
    std::string rsType;
    auto typeInfoMap_ = RsMetadataAPIHelper::getTypeInfoMap();
    auto it = typeInfoMap_.find(dataType);
    if(it != typeInfoMap_.end()){
        const auto& typeInfo = it->second;
        sqlType = typeInfo.sqlType;
        sqlDataType = typeInfo.sqlDataType;
        sqlDateSub = typeInfo.sqlDateSub;
        rsType = typeInfo.typeName;
    }
    else{
        rsType = dataType;
    }
    ASSERT_EQ(rsType, dataType2RsTypeMap[dataType]);
}

TEST_P(test_getrstype, test_rsTypeLen) {
    std::string dataType = GetParam();
    std::string rsType;
    auto typeInfoMap_ = RsMetadataAPIHelper::getTypeInfoMap();
    auto it = typeInfoMap_.find(dataType);
    if(it != typeInfoMap_.end()){
        const auto& typeInfo = it->second;
        rsType = typeInfo.typeName;
    }
    else{
        rsType = dataType;
    }
    ASSERT_EQ(rsType.size(), dataType2RsTypeMap[dataType].size());
}

TEST_P(test_getrstype, test_sqlType) {
    std::string dataType = GetParam();
    short sqlType = 0;
    auto typeInfoMap_ = RsMetadataAPIHelper::getTypeInfoMap();
    auto it = typeInfoMap_.find(dataType);
    if(it != typeInfoMap_.end()){
        const auto& typeInfo = it->second;
        sqlType = typeInfo.sqlType;
    }
    ASSERT_EQ(sqlType, dataTypeMap[dataType].sqlType);
}

TEST_P(test_getrstype, test_sqlDataType) {
    std::string dataType = GetParam();
    short sqlDataType = 0;
    auto typeInfoMap_ = RsMetadataAPIHelper::getTypeInfoMap();
    auto it = typeInfoMap_.find(dataType);
    if(it != typeInfoMap_.end()){
        const auto& typeInfo = it->second;
        sqlDataType = typeInfo.sqlDataType;
    }
    ASSERT_EQ(sqlDataType, dataTypeMap[dataType].sqlDataType);
}

TEST_P(test_getrstype, test_sqlDateSub) {
    std::string dataType = GetParam();
    short sqlDateSub = 0;
    auto typeInfoMap_ = RsMetadataAPIHelper::getTypeInfoMap();
    auto it = typeInfoMap_.find(dataType);
    if(it != typeInfoMap_.end()){
        const auto& typeInfo = it->second;
        sqlDateSub = typeInfo.sqlDateSub;
    }
    ASSERT_EQ(sqlDateSub, dataTypeMap[dataType].sqlDateSub);
}

TEST_P(test_getrstype, test_columnSize) {
    std::string dataType = GetParam();
    std::string rsType;
    auto typeInfoMap_ = RsMetadataAPIHelper::getTypeInfoMap();
    auto it = typeInfoMap_.find(dataType);
    if(it != typeInfoMap_.end()){
        const auto& typeInfo = it->second;
        rsType = typeInfo.typeName;
    }
    else{
        rsType = dataType;
    }
    int columnSize = RsMetadataAPIHelper::getColumnSize(rsType, character_maximum_length, numeric_precision);
    ASSERT_EQ(columnSize, dataTypeMap[dataType].colSize);
}

TEST_P(test_getrstype, test_decimalDigit) {
    std::string dataType = GetParam();
    std::string rsType;
    auto typeInfoMap_ = RsMetadataAPIHelper::getTypeInfoMap();
    auto it = typeInfoMap_.find(dataType);
    if(it != typeInfoMap_.end()){
        const auto& typeInfo = it->second;
        rsType = typeInfo.typeName;
    }
    else{
        rsType = dataType;
    }
    ASSERT_EQ(RsMetadataAPIHelper::getDecimalDigit(rsType, numeric_scale, 0, false), dataTypeMap[dataType].decimalDigit);
}

TEST_P(test_getrstype, test_bufferLen) {
    std::string dataType = GetParam();
    std::string rsType;
    auto typeInfoMap_ = RsMetadataAPIHelper::getTypeInfoMap();
    auto it = typeInfoMap_.find(dataType);
    if(it != typeInfoMap_.end()){
        const auto& typeInfo = it->second;
        rsType = typeInfo.typeName;
    }
    else{
        rsType = dataType;
    }
    ASSERT_EQ(RsMetadataAPIHelper::getBufferLen(rsType, character_maximum_length, numeric_precision), dataTypeMap[dataType].bufferLen);
}

INSTANTIATE_TEST_SUITE_P(METADATA_API_HELPER_TEST_SUITE, test_getrstype, ::testing::Values("smallint","integer","bigint","numeric","real","double precision","boolean","character","character varying","date","time without time zone","time with time zone","timestamp without time zone","timestamp with time zone", "interval year to month", "interval day to second"));



