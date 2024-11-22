/*-------------------------------------------------------------------------
 *
 * Copyright(c) 2024, Amazon.com, Inc. or Its Affiliates. All rights reserved.
 *
 * Author: stinghsu
 *-------------------------------------------------------------------------
 */

#include "rsMetadataAPIHelper.h"
#include "rsexecute.h"
#include "rsutil.h"

const std::string RsMetadataAPIHelper::kSHOW_DATABASES_database_name =
    "database_name";
const std::string RsMetadataAPIHelper::kSHOW_SCHEMAS_database_name =
    "database_name";
const std::string RsMetadataAPIHelper::kSHOW_SCHEMAS_schema_name =
    "schema_name";
const std::string RsMetadataAPIHelper::kSHOW_TABLES_database_name =
    "database_name";
const std::string RsMetadataAPIHelper::kSHOW_TABLES_schema_name = "schema_name";
const std::string RsMetadataAPIHelper::kSHOW_TABLES_table_name = "table_name";
const std::string RsMetadataAPIHelper::kSHOW_TABLES_table_type = "table_type";
const std::string RsMetadataAPIHelper::kSHOW_TABLES_remarks = "remarks";
const std::string RsMetadataAPIHelper::kSHOW_COLUMNS_database_name =
    "database_name";
const std::string RsMetadataAPIHelper::kSHOW_COLUMNS_schema_name =
    "schema_name";
const std::string RsMetadataAPIHelper::kSHOW_COLUMNS_table_name = "table_name";
const std::string RsMetadataAPIHelper::kSHOW_COLUMNS_column_name =
    "column_name";
const std::string RsMetadataAPIHelper::kSHOW_COLUMNS_ordinal_position =
    "ordinal_position";
const std::string RsMetadataAPIHelper::kSHOW_COLUMNS_column_default =
    "column_default";
const std::string RsMetadataAPIHelper::kSHOW_COLUMNS_is_nullable =
    "is_nullable";
const std::string RsMetadataAPIHelper::kSHOW_COLUMNS_data_type = "data_type";
const std::string RsMetadataAPIHelper::kSHOW_COLUMNS_character_maximum_length =
    "character_maximum_length";
const std::string RsMetadataAPIHelper::kSHOW_COLUMNS_numeric_precision =
    "numeric_precision";
const std::string RsMetadataAPIHelper::kSHOW_COLUMNS_numeric_scale =
    "numeric_scale";
const std::string RsMetadataAPIHelper::kSHOW_COLUMNS_remarks = "remarks";

const std::vector<std::string> RsMetadataAPIHelper::tableTypeList = {
    "SYSTEM TABLE",    "SYSTEM VIEW",    "TABLE",
    "TEMPORARY TABLE", "TEMPORARY VIEW", "VIEW"};

//
// Helper function to initialize column field from given column name and data
// type
//
SQLRETURN RsMetadataAPIHelper::initializeColumnField(RS_STMT_INFO *pStmt,
                                                     char **col, int colNum,
                                                     int *colDatatype) {
    RS_LOG_TRACE("initializeColumnField", "initializeColumnField");
    return libpqInitializeResultSetField(pStmt, col, colNum, colDatatype);
}

//
//  Mapping between data type and its type info
//  DATA_TYPE_INFO:
//      SQL type (concise data type)
//      SQL Data type (non-concise data type)
//      SQL Datetime subtype
//      Redshift type name
//
const std::unordered_map<std::string, DATA_TYPE_INFO>
    RsMetadataAPIHelper::typeInfoMap = {
        {"character varying",
         {SQL_VARCHAR, SQL_VARCHAR, kNotApplicable, "varchar"}},
        {"character", {SQL_CHAR, SQL_CHAR, kNotApplicable, "char"}},
        {"\"char\"", {SQL_CHAR, SQL_CHAR, kNotApplicable, "char"}},
        {"smallint", {SQL_SMALLINT, SQL_SMALLINT, kNotApplicable, "int2"}},
        {"integer", {SQL_INTEGER, SQL_INTEGER, kNotApplicable, "int4"}},
        {"bigint", {SQL_BIGINT, SQL_BIGINT, kNotApplicable, "int8"}},
        {"real", {SQL_REAL, SQL_REAL, kNotApplicable, "float4"}},
        {"double precision",
         {SQL_DOUBLE, SQL_DOUBLE, kNotApplicable, "float8"}},
        {"numeric", {SQL_NUMERIC, SQL_NUMERIC, kNotApplicable, "numeric"}},
        {"boolean", {SQL_BIT, SQL_BIT, kNotApplicable, "bool"}},
        {"date", {SQL_TYPE_DATE, SQL_DATETIME, SQL_CODE_DATE, "date"}},
        {"time", {SQL_TYPE_TIME, SQL_DATETIME, SQL_CODE_TIME, "time"}},
        {"time without time zone",
         {SQL_TYPE_TIME, SQL_DATETIME, SQL_CODE_TIME, "time"}},
        {"timetz", {SQL_TYPE_TIME, SQL_DATETIME, SQL_CODE_TIME, "timetz"}},
        {"time with time zone",
         {SQL_TYPE_TIME, SQL_DATETIME, SQL_CODE_TIME, "timetz"}},
        {"timestamp",
         {SQL_TYPE_TIMESTAMP, SQL_DATETIME, SQL_CODE_TIMESTAMP, "timestamp"}},
        {"timestamp without time zone",
         {SQL_TYPE_TIMESTAMP, SQL_DATETIME, SQL_CODE_TIMESTAMP, "timestamp"}},
        {"timestamptz",
         {SQL_TYPE_TIMESTAMP, SQL_DATETIME, SQL_CODE_TIMESTAMP, "timestamptz"}},
        {"timestamp with time zone",
         {SQL_TYPE_TIMESTAMP, SQL_DATETIME, SQL_CODE_TIMESTAMP, "timestamptz"}},
        {"interval year to month",
         {SQL_INTERVAL_YEAR_TO_MONTH, SQL_INTERVAL, SQL_CODE_YEAR_TO_MONTH,
          "intervaly2m"}},
        {"interval year",
         {SQL_INTERVAL_YEAR_TO_MONTH, SQL_INTERVAL, SQL_CODE_YEAR_TO_MONTH,
          "intervaly2m"}},
        {"interval month",
         {SQL_INTERVAL_YEAR_TO_MONTH, SQL_INTERVAL, SQL_CODE_YEAR_TO_MONTH,
          "intervaly2m"}},
        {"interval day to second",
         {SQL_INTERVAL_DAY_TO_SECOND, SQL_INTERVAL, SQL_CODE_DAY_TO_SECOND,
          "intervald2s"}},
        {"interval day",
         {SQL_INTERVAL_DAY_TO_SECOND, SQL_INTERVAL, SQL_CODE_DAY_TO_SECOND,
          "intervald2s"}},
        {"interval second",
         {SQL_INTERVAL_DAY_TO_SECOND, SQL_INTERVAL, SQL_CODE_DAY_TO_SECOND,
          "intervald2s"}},
        {"super", {SQL_LONGVARCHAR, SQL_LONGVARCHAR, kNotApplicable, "super"}},
        {"binary varying",
         {SQL_LONGVARBINARY, SQL_LONGVARBINARY, kNotApplicable, "varbyte"}},
        {"geometry",
         {SQL_LONGVARBINARY, SQL_LONGVARBINARY, kNotApplicable, "geometry"}},
        {"geography",
         {SQL_LONGVARBINARY, SQL_LONGVARBINARY, kNotApplicable, "geography"}},
        {"hllsketch",
         {SQL_UNKNOWN_TYPE, SQL_UNKNOWN_TYPE, kNotApplicable, "hllsketch"}}};

const std::unordered_map<std::string, DATA_TYPE_INFO> RsMetadataAPIHelper::getTypeInfoMap() { return RsMetadataAPIHelper::typeInfoMap;}

//
// Define the column size for different data type defined in ODBC spec:
// https://learn.microsoft.com/en-us/sql/odbc/reference/appendixes/column-size?view=sql-server-ver16
//
const std::unordered_map<std::string, int> RsMetadataAPIHelper::columnSizeMap =
    {{"int2", 5},
     {"int4", 10},
     {"int8", 19},
     {"float4", 7},
     {"float8", 15},
     {"bool", 1},
     {"date", MAX_DATEOID_SIZE},
     {"time", MAX_TIMEOID_SIZE},
     {"timetz", MAX_TIMETZOID_SIZE},
     {"timestamp", MAX_TIMESTAMPOID_SIZE},
     {"timestamptz", MAX_TIMESTAMPTZOID_SIZE},
     {"intervaly2m", MAX_INTERVALY2MOID_SIZE},
     {"intervald2s", MAX_INTERVALD2SOID_SIZE}};

const std::unordered_map<std::string, int> RsMetadataAPIHelper::bufferLenMap = {
    {"int2", sizeof(short)},
    {"int4", sizeof(int)},
    {"int8", sizeof(long long)},
    {"float4", sizeof(float)},
    {"float8", sizeof(double)},
    {"bool", sizeof(bool)},
    {"date", sizeof(SQL_DATE_STRUCT)},
    {"time", sizeof(SQL_TIME_STRUCT)},
    {"timetz", sizeof(SQL_TIME_STRUCT)},
    {"timestamp", sizeof(SQL_TIMESTAMP_STRUCT)},
    {"timestamptz", sizeof(SQL_TIMESTAMP_STRUCT)},
    {"intervaly2m", sizeof(INTERVALY2M_STRUCT)},
    {"intervald2s", sizeof(INTERVALD2S_STRUCT)}};

const std::unordered_map<std::string, int>
    RsMetadataAPIHelper::numPrecRadixMap = {
        {"varbyte", 10}, {"geography", 2}, {"int2", 10},   {"int4", 10},
        {"int8", 10},   {"float4", 10},   {"float8", 10}, {"numeric", 10},
        {"char", 0}, {"varchar", 0},};

const std::unordered_set<std::string> RsMetadataAPIHelper::charOctetLenSet = {
    "char", "varchar", "varbyte", "geography", "hllsketch"};

//
// Helper function to get column size by Redshift type name
//
int RsMetadataAPIHelper::getColumnSize(const std::string &rsType,
                                       short character_maximum_length,
                                       short numeric_precision) {
    auto it = RsMetadataAPIHelper::columnSizeMap.find(rsType);
    if (it != RsMetadataAPIHelper::columnSizeMap.end()) {
        return it->second;
    } else {
        if (rsType == "numeric") {
            return numeric_precision;
        } else if (rsType == "char" || rsType == "varchar") {
            return character_maximum_length;
        } else if (rsType == "super" || rsType == "geometry" ||
                   rsType == "geography") {
            return kNotApplicable;
        } else {
            return kUnknownColumnSize;
        }
    }
}

//
// Helper function to get column size by Redshift type name
//
int RsMetadataAPIHelper::getBufferLen(const std::string &rsType,
                                      short character_maximum_length,
                                      short numeric_precision) {
    auto it = RsMetadataAPIHelper::bufferLenMap.find(rsType);
    if (it != RsMetadataAPIHelper::bufferLenMap.end()) {
        return it->second;
    } else {
        if (rsType == "numeric") {
            // Numeric value with 19 or fewer significant digits of precision
            // are stored internally as 8-byte integer, while with 20 to 38
            // significant digits of precision are stored internally as 16-byte
            // integer
            return numeric_precision <= 19 ? 8 : 16;
        } else if (rsType == "char" || rsType == "varchar") {
            return character_maximum_length;
        } else if (rsType == "super" || rsType == "geometry" ||
                   rsType == "geography" || rsType == "varbyte" ||
                   rsType == "hllsketch") {
            return kNotApplicable;
        } else {
            return kUnknownColumnSize;
        }
    }
}

//
// Helper function to return decimal digit by Redshift type name
//
short RsMetadataAPIHelper::getDecimalDigit(const std::string &rsType,
                                           short numeric_scale,
                                           short precisions,
                                           bool dateTimeCustomizePrecision) {
    if (rsType == "float4") {
        return kFloat4DecimalDigit;
    } else if (rsType == "float8") {
        return kFloat8DecimalDigit;
    } else if (rsType == "time" || rsType == "timetz" ||
               rsType == "timestamp" || rsType == "timestamptz" ||
               rsType == "intervald2s") {
        return dateTimeCustomizePrecision ? precisions
                                          : kDefaultSecondPrecision;
    } else if (rsType == "numeric") {
        return numeric_scale;
    } else {
        return kNotApplicable;
    }
}

//
// Helper function to convert nullable String to corresponding number
//
short RsMetadataAPIHelper::getNullable(const std::string &nullable) {
    if (nullable == "YES") {
        return kColumnNullable;
    } else if (nullable == "NO") {
        return kColumnNoNulls;
    } else {
        return kColumnNullableUnknown;
    }
}

//
// Helper function to get Number of Prefix Radix from given Redshift type
//
short RsMetadataAPIHelper::getNumPrecRadix(const std::string &rsType) {
    auto it = RsMetadataAPIHelper::numPrecRadixMap.find(rsType);
    if (it != RsMetadataAPIHelper::numPrecRadixMap.end()) {
        return it->second;
    } else {
        //return kNotApplicable;
        return 10; // Return 10 to avoid breaking change
    }
}

//
// Helper function to get char octet length
//
int RsMetadataAPIHelper::getCharOctetLen(const std::string &rsType,
                                         short character_maximum_length) {
    auto it = RsMetadataAPIHelper::charOctetLenSet.find(rsType);
    if (it != RsMetadataAPIHelper::charOctetLenSet.end()) {
        if (rsType == "char" || rsType == "varchar") {
            return character_maximum_length;
        } else if (rsType == "varbyte") {
            return kUnknownColumnSize;
        } else if (rsType == "geography") {
            return kNotApplicable;
        } else {
            return kUnknownColumnSize;
        }
    } else {
        return kNotApplicable;
    }
}