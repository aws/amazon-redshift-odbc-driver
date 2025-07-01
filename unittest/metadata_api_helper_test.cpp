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


#define SQLTYPE 0
#define SQLDATATYPE 1
#define SQLDATESUB 2
#define COLUMNSIZE 3
#define DECIMALDIGIT 4
struct DATA_TYPE_RES{
  short sqlType;
  short sqlTypeODBC2;
  short sqlDataType;
  short sqlDateSub;
  short colSize;
  short decimalDigit;
  int bufferLen;
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
class TestGetRsType :public ::testing::TestWithParam<std::tuple<std::string, bool>> {
    protected:
        static std::map<std::string, std::string> dataType2RsTypeMap;
        static std::map<std::string, DATA_TYPE_RES> dataTypeMap;
        static void SetUpTestSuite();

        void SetUp() override {
            std::tie(dataType, isODBC_SpecV2) = GetParam();
        }
        
        std::string dataType;
        bool isODBC_SpecV2;
};

std::map<std::string, std::string> TestGetRsType::dataType2RsTypeMap;
std::map<std::string, DATA_TYPE_RES> TestGetRsType::dataTypeMap;


void TestGetRsType::SetUpTestSuite() {
    auto dataTypeName = RsMetadataAPIHelper::getDataTypeNameStruct();
    auto redshiftTypeName = RsMetadataAPIHelper::getRedshiftTypeNameStruct();

    dataType2RsTypeMap = {
        {dataTypeName.ksmallint, redshiftTypeName.kint2},
        {dataTypeName.kinteger, redshiftTypeName.kint4},
        {dataTypeName.kbigint, redshiftTypeName.kint8},
        {dataTypeName.knumeric, redshiftTypeName.knumeric},
        {dataTypeName.kreal, redshiftTypeName.kfloat4},
        {dataTypeName.kdouble_precision, redshiftTypeName.kfloat8},
        {dataTypeName.kboolean, redshiftTypeName.kbool},
        {dataTypeName.kcharacter, redshiftTypeName.kchar},
        {dataTypeName.kcharacter_varying, redshiftTypeName.kvarchar},
        {dataTypeName.kdate, redshiftTypeName.kdate},
        {dataTypeName.ktime_without_time_zone, redshiftTypeName.ktime},
        {dataTypeName.ktime, redshiftTypeName.ktime},
        {dataTypeName.ktime_with_time_zone, redshiftTypeName.ktimetz},
        {dataTypeName.ktimetz, redshiftTypeName.ktimetz},
        {dataTypeName.ktimestamp_without_time_zone, redshiftTypeName.ktimestamp},
        {dataTypeName.ktimestamp, redshiftTypeName.ktimestamp},
        {dataTypeName.ktimestamp_with_time_zone, redshiftTypeName.ktimestamptz},
        {dataTypeName.ktimestamptz, redshiftTypeName.ktimestamptz},
        {dataTypeName.kinterval_year_to_month, redshiftTypeName.kintervaly2m},
        {dataTypeName.kinterval_day_to_second, redshiftTypeName.kintervald2s},
        {dataTypeName.ksuper, redshiftTypeName.ksuper},
        {dataTypeName.kgeometry, redshiftTypeName.kgeometry},
        {dataTypeName.kgeography, redshiftTypeName.kgeography},
        {dataTypeName.kstring, dataTypeName.kstring},
        {dataTypeName.kbinary, dataTypeName.kbinary},
        {dataTypeName.karray, dataTypeName.karray},
        {dataTypeName.kmap, dataTypeName.kmap},
        {dataTypeName.kstruct, dataTypeName.kstruct}
    };

    dataTypeMap = {
        {dataTypeName.ksmallint, {SQL_SMALLINT, SQL_SMALLINT, SQL_SMALLINT, kNotApplicable, 5, kNotApplicable, sizeof(short)}},
        {dataTypeName.kinteger, {SQL_INTEGER, SQL_INTEGER, SQL_INTEGER, kNotApplicable, 10, kNotApplicable, sizeof(int)}},
        {dataTypeName.kbigint, {SQL_BIGINT, SQL_BIGINT, SQL_BIGINT, kNotApplicable, 19, kNotApplicable, sizeof(long long)}},
        {dataTypeName.knumeric, {SQL_NUMERIC, SQL_NUMERIC, SQL_NUMERIC, kNotApplicable, numeric_precision, numeric_scale, 8}},
        {dataTypeName.kreal, {SQL_REAL, SQL_REAL, SQL_REAL, kNotApplicable, 7, 6, sizeof(float)}},
        {dataTypeName.kdouble_precision, {SQL_DOUBLE, SQL_DOUBLE, SQL_DOUBLE, kNotApplicable, 15, 15, sizeof(double)}},
        {dataTypeName.kboolean, {SQL_BIT, SQL_BIT, SQL_BIT, kNotApplicable, 1, kNotApplicable, sizeof(bool)}},
        {dataTypeName.kcharacter, {SQL_CHAR, SQL_CHAR, SQL_CHAR, kNotApplicable, character_maximum_length, kNotApplicable, character_maximum_length}},
        {dataTypeName.kcharacter_varying, {SQL_VARCHAR, SQL_VARCHAR, SQL_VARCHAR, kNotApplicable, character_maximum_length, kNotApplicable, character_maximum_length}},
        {dataTypeName.kstring, {SQL_VARCHAR, SQL_VARCHAR, SQL_VARCHAR, kNotApplicable, character_maximum_length, kNotApplicable, character_maximum_length}},
        {dataTypeName.kdate, {SQL_TYPE_DATE, SQL_DATE, SQL_DATETIME, SQL_CODE_DATE, 10, kNotApplicable, sizeof(SQL_DATE_STRUCT)}},
        {dataTypeName.ktime_without_time_zone, {SQL_TYPE_TIME, SQL_TIME, SQL_DATETIME, SQL_CODE_TIME, 15, 6, sizeof(SQL_TIME_STRUCT)}},
        {dataTypeName.ktime, {SQL_TYPE_TIME, SQL_TIME, SQL_DATETIME, SQL_CODE_TIME, 15, 6, sizeof(SQL_TIME_STRUCT)}},
        {dataTypeName.ktime_with_time_zone, {SQL_TYPE_TIME, SQL_TIME, SQL_DATETIME, SQL_CODE_TIME, 21, 6, sizeof(SQL_TIME_STRUCT)}},
        {dataTypeName.ktimetz, {SQL_TYPE_TIME, SQL_TIME, SQL_DATETIME, SQL_CODE_TIME, 21, 6, sizeof(SQL_TIME_STRUCT)}},
        {dataTypeName.ktimestamp_without_time_zone, {SQL_TYPE_TIMESTAMP, SQL_TIMESTAMP, SQL_DATETIME, SQL_CODE_TIMESTAMP, 29, 6, sizeof(SQL_TIMESTAMP_STRUCT)}},
        {dataTypeName.ktimestamp, {SQL_TYPE_TIMESTAMP, SQL_TIMESTAMP, SQL_DATETIME, SQL_CODE_TIMESTAMP, 29, 6, sizeof(SQL_TIMESTAMP_STRUCT)}},
        {dataTypeName.ktimestamp_with_time_zone, {SQL_TYPE_TIMESTAMP, SQL_TIMESTAMP, SQL_DATETIME, SQL_CODE_TIMESTAMP, 35, 6, sizeof(SQL_TIMESTAMP_STRUCT)}},
        {dataTypeName.ktimestamptz, {SQL_TYPE_TIMESTAMP, SQL_TIMESTAMP, SQL_DATETIME, SQL_CODE_TIMESTAMP, 35, 6, sizeof(SQL_TIMESTAMP_STRUCT)}},
        {dataTypeName.kinterval_year_to_month, {SQL_INTERVAL_YEAR_TO_MONTH, SQL_INTERVAL_YEAR_TO_MONTH, SQL_INTERVAL, SQL_CODE_YEAR_TO_MONTH, 32, kNotApplicable, sizeof(INTERVALY2M_STRUCT)}},
        {dataTypeName.kinterval_day_to_second, {SQL_INTERVAL_DAY_TO_SECOND, SQL_INTERVAL_DAY_TO_SECOND, SQL_INTERVAL, SQL_CODE_DAY_TO_SECOND, 64, 6, sizeof(INTERVALD2S_STRUCT)}},
        {dataTypeName.kbinary, {SQL_LONGVARBINARY, SQL_LONGVARBINARY, SQL_LONGVARBINARY, kNotApplicable, kNotApplicable, kNotApplicable, kNotApplicable}},
        {dataTypeName.ksuper, {SQL_LONGVARCHAR, SQL_LONGVARCHAR, SQL_LONGVARCHAR, kNotApplicable, kNotApplicable, kNotApplicable, kNotApplicable}},
        {dataTypeName.karray, {SQL_LONGVARCHAR, SQL_LONGVARCHAR, SQL_LONGVARCHAR, kNotApplicable, kNotApplicable, kNotApplicable, kNotApplicable}},
        {dataTypeName.kmap, {SQL_LONGVARCHAR, SQL_LONGVARCHAR, SQL_LONGVARCHAR, kNotApplicable, kNotApplicable, kNotApplicable, kNotApplicable}},
        {dataTypeName.kstruct, {SQL_LONGVARCHAR, SQL_LONGVARCHAR, SQL_LONGVARCHAR, kNotApplicable, kNotApplicable, kNotApplicable, kNotApplicable}},
        {dataTypeName.kgeometry, {SQL_LONGVARBINARY, SQL_LONGVARBINARY, SQL_LONGVARBINARY, kNotApplicable, kNotApplicable, kNotApplicable, kNotApplicable}},
        {dataTypeName.kgeography, {SQL_LONGVARBINARY, SQL_LONGVARBINARY, SQL_LONGVARBINARY, kNotApplicable, kNotApplicable, kNotApplicable, kNotApplicable}}
    };
}

TEST_P(TestGetRsType, test_rsType) {
    short sqlType = 0, sqlDataType = 0, sqlDateSub = 0;
    std::string rsType;
    TypeInfoResult typeInfoResult = RsMetadataAPIHelper::getTypeInfo(dataType, isODBC_SpecV2);
    if(typeInfoResult.found){
        DATA_TYPE_INFO typeInfo = typeInfoResult.typeInfo;
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

TEST_P(TestGetRsType, test_rsTypeLen) {
    std::string rsType;
    TypeInfoResult typeInfoResult = RsMetadataAPIHelper::getTypeInfo(dataType, isODBC_SpecV2);
    if(typeInfoResult.found){
        DATA_TYPE_INFO typeInfo = typeInfoResult.typeInfo;
        rsType = typeInfo.typeName;
    }
    else{
        rsType = dataType;
    }
    ASSERT_EQ(rsType.size(), dataType2RsTypeMap[dataType].size());
}

TEST_P(TestGetRsType, test_sqlType) {
    short sqlType = 0;
    TypeInfoResult typeInfoResult = RsMetadataAPIHelper::getTypeInfo(dataType, isODBC_SpecV2);
    if(typeInfoResult.found){
        DATA_TYPE_INFO typeInfo = typeInfoResult.typeInfo;
        sqlType = typeInfo.sqlType;
    }
    if(isODBC_SpecV2){
        ASSERT_EQ(sqlType, dataTypeMap[dataType].sqlTypeODBC2);
    }
    else{
        ASSERT_EQ(sqlType, dataTypeMap[dataType].sqlType);
    }
    
}

TEST_P(TestGetRsType, test_sqlDataType) {
    short sqlDataType = 0;
    TypeInfoResult typeInfoResult = RsMetadataAPIHelper::getTypeInfo(dataType, isODBC_SpecV2);
    if(typeInfoResult.found){
        DATA_TYPE_INFO typeInfo = typeInfoResult.typeInfo;
        sqlDataType = typeInfo.sqlDataType;
    }
    ASSERT_EQ(sqlDataType, dataTypeMap[dataType].sqlDataType);
}

TEST_P(TestGetRsType, test_sqlDateSub) {
    short sqlDateSub = 0;
    TypeInfoResult typeInfoResult = RsMetadataAPIHelper::getTypeInfo(dataType, isODBC_SpecV2);
    if(typeInfoResult.found){
        DATA_TYPE_INFO typeInfo = typeInfoResult.typeInfo;
        sqlDateSub = typeInfo.sqlDateSub;
    }
    ASSERT_EQ(sqlDateSub, dataTypeMap[dataType].sqlDateSub);
}

TEST_P(TestGetRsType, test_columnSize) {
    std::string rsType;
    TypeInfoResult typeInfoResult = RsMetadataAPIHelper::getTypeInfo(dataType, isODBC_SpecV2);
    if(typeInfoResult.found){
        DATA_TYPE_INFO typeInfo = typeInfoResult.typeInfo;
        rsType = typeInfo.typeName;
    }
    else{
        rsType = dataType;
    }
    int columnSize = RsMetadataAPIHelper::getColumnSize(rsType, character_maximum_length, numeric_precision);
    ASSERT_EQ(columnSize, dataTypeMap[dataType].colSize);
}

TEST_P(TestGetRsType, test_decimalDigit) {
    std::string rsType;
    TypeInfoResult typeInfoResult = RsMetadataAPIHelper::getTypeInfo(dataType, isODBC_SpecV2);
    if(typeInfoResult.found){
        DATA_TYPE_INFO typeInfo = typeInfoResult.typeInfo;
        rsType = typeInfo.typeName;
    }
    else{
        rsType = dataType;
    }
    ASSERT_EQ(RsMetadataAPIHelper::getDecimalDigit(rsType, numeric_scale, 0, false), dataTypeMap[dataType].decimalDigit);
}

TEST_P(TestGetRsType, test_bufferLen) {
    std::string rsType;
    TypeInfoResult typeInfoResult = RsMetadataAPIHelper::getTypeInfo(dataType, isODBC_SpecV2);
    if(typeInfoResult.found){
        DATA_TYPE_INFO typeInfo = typeInfoResult.typeInfo;
        rsType = typeInfo.typeName;
    }
    else{
        rsType = dataType;
    }
    ASSERT_EQ(RsMetadataAPIHelper::getBufferLen(rsType, character_maximum_length, numeric_precision), dataTypeMap[dataType].bufferLen);
}

INSTANTIATE_TEST_SUITE_P(METADATA_API_HELPER_TEST_SUITE, TestGetRsType, 
    ::testing::Combine(
        ::testing::Values(
            RsMetadataAPIHelper::getDataTypeNameStruct().ksmallint,
            RsMetadataAPIHelper::getDataTypeNameStruct().kinteger,
            RsMetadataAPIHelper::getDataTypeNameStruct().kbigint,
            RsMetadataAPIHelper::getDataTypeNameStruct().knumeric,
            RsMetadataAPIHelper::getDataTypeNameStruct().kreal,
            RsMetadataAPIHelper::getDataTypeNameStruct().kdouble_precision,
            RsMetadataAPIHelper::getDataTypeNameStruct().kboolean,
            RsMetadataAPIHelper::getDataTypeNameStruct().kcharacter,
            RsMetadataAPIHelper::getDataTypeNameStruct().kcharacter_varying,
            RsMetadataAPIHelper::getDataTypeNameStruct().kstring,
            RsMetadataAPIHelper::getDataTypeNameStruct().kdate,
            RsMetadataAPIHelper::getDataTypeNameStruct().ktime,
            RsMetadataAPIHelper::getDataTypeNameStruct().ktime_without_time_zone,
            RsMetadataAPIHelper::getDataTypeNameStruct().ktimetz,
            RsMetadataAPIHelper::getDataTypeNameStruct().ktime_with_time_zone,
            RsMetadataAPIHelper::getDataTypeNameStruct().ktimestamp,
            RsMetadataAPIHelper::getDataTypeNameStruct().ktimestamp_without_time_zone,
            RsMetadataAPIHelper::getDataTypeNameStruct().ktimestamptz,
            RsMetadataAPIHelper::getDataTypeNameStruct().ktimestamp_with_time_zone,
            RsMetadataAPIHelper::getDataTypeNameStruct().kinterval_year_to_month,
            RsMetadataAPIHelper::getDataTypeNameStruct().kinterval_day_to_second,
            RsMetadataAPIHelper::getDataTypeNameStruct().kbinary,
            RsMetadataAPIHelper::getDataTypeNameStruct().ksuper,
            RsMetadataAPIHelper::getDataTypeNameStruct().karray,
            RsMetadataAPIHelper::getDataTypeNameStruct().kmap,
            RsMetadataAPIHelper::getDataTypeNameStruct().kstruct,
            RsMetadataAPIHelper::getDataTypeNameStruct().kgeometry,
            RsMetadataAPIHelper::getDataTypeNameStruct().kgeography
        ),
        ::testing::Values(true, false)
    ));


