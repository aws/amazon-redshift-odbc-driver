/*-------------------------------------------------------------------------
 *
 * Copyright(c) 2024, Amazon.com, Inc. or Its Affiliates. All rights reserved.
 *
 * Author: stinghsu
 *-------------------------------------------------------------------------
 */

#pragma once

#ifndef __RS_METADATAAPIHELPER_H__

#define __RS_METADATAAPIHELPER_H__

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rsodbc.h"
#include "rstrace.h"
#include "rsunicode.h"
#include "rsutil.h"
#include <regex>

// Define constant for SQLTables
// Spec:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqltables-function?view=sql-server-ver16
#define kTablesColNum 5 // Number of column
/* COLUMNS IN SQLTables() RESULT SET */
#define kSQLTables_TABLE_CATALOG 0
#define kSQLTables_TABLE_SCHEM 1
#define kSQLTables_TABLE_NAME 2
#define kSQLTables_TABLE_TYPE 3
#define kSQLTables_REMARKS 4

// Define constant for SQLColumns
// Spec:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlcolumns-function?view=sql-server-ver16
#define kColumnsColNum 18 // Number of column
/* COLUMNS IN SQLColumns() RESULT SET */                     
#define kSQLColumns_TABLE_CAT 0
#define kSQLColumns_TABLE_SCHEM 1
#define kSQLColumns_TABLE_NAME 2
#define kSQLColumns_COLUMN_NAME 3
#define kSQLColumns_DATA_TYPE 4
#define kSQLColumns_TYPE_NAME 5
#define kSQLColumns_COLUMN_SIZE 6
#define kSQLColumns_BUFFER_LENGTH 7
#define kSQLColumns_DECIMAL_DIGITS 8
#define kSQLColumns_NUM_PREC_RADIX 9
#define kSQLColumns_NULLABLE 10
#define kSQLColumns_REMARKS 11
#define kSQLColumns_COLUMN_DEF 12
#define kSQLColumns_SQL_DATA_TYPE 13
#define kSQLColumns_SQL_DATETIME_SUB 14
#define kSQLColumns_CHAR_OCTET_LENGTH 15
#define kSQLColumns_ORDINAL_POSITION 16
#define kSQLColumns_IS_NULLABLE 17

#define kColumnNullable 1
#define kColumnNoNulls 0
#define kColumnNullableUnknown 2

#define kFloat4DecimalDigit 6
#define kFloat8DecimalDigit 15

#define kDefaultSecondPrecision 6

#define kNotApplicable -1

#define kUnknownColumnSize 2147483647


/*----------------
 * RsMetadataAPIHelper
 *
 * A class which contains constant and helper function for new metadata API
 * implementation
 * ----------------
 */
class RsMetadataAPIHelper {
  public:
    // Define column name constant for SHOW API
    static const std::string
        kSHOW_DATABASES_database_name; /* Column name to retrieve catalog from
                                          SHOW DATABASES*/

    static const std::string
        kSHOW_SCHEMAS_database_name; /* Column name to retrieve catalog from
                                        SHOW SCHEMAS*/
    static const std::string
        kSHOW_SCHEMAS_schema_name; /* Column name to retrieve schema from SHOW
                                      SCHEMAS*/

    static const std::string
        kSHOW_TABLES_database_name; /* Column name to retrieve catalog from SHOW
                                       TABLES*/
    static const std::string
        kSHOW_TABLES_schema_name; /* Column name to retrieve schema from SHOW
                                     TABLES*/
    static const std::string
        kSHOW_TABLES_table_name; /* Column name to retrieve table from SHOW
                                    TABLES*/
    static const std::string
        kSHOW_TABLES_table_type; /* Column name to retrieve table type from SHOW
                                    TABLES*/
    static const std::string kSHOW_TABLES_remarks; /* Column name to retrieve
                                                      remark from SHOW TABLES*/

    static const std::string
        kSHOW_COLUMNS_database_name; /* Column name to retrieve catalog from
                                        SHOW COLUMNS*/
    static const std::string
        kSHOW_COLUMNS_schema_name; /* Column name to retrieve schema from SHOW
                                      COLUMNS*/
    static const std::string
        kSHOW_COLUMNS_table_name; /* Column name to retrieve table from SHOW
                                     COLUMNS*/
    static const std::string
        kSHOW_COLUMNS_column_name; /* Column name to retrieve column from SHOW
                                      COLUMNS*/
    static const std::string
        kSHOW_COLUMNS_ordinal_position; /* Column name to retrieve ordinal
                                           position from SHOW COLUMNS*/
    static const std::string
        kSHOW_COLUMNS_column_default; /* Column name to retrieve column default
                                         from SHOW COLUMNS*/
    static const std::string
        kSHOW_COLUMNS_is_nullable; /* Column name to retrieve nullable from SHOW
                                      COLUMNS*/
    static const std::string
        kSHOW_COLUMNS_data_type; /* Column name to retrieve data type from SHOW
                                    COLUMNS*/
    static const std::string
        kSHOW_COLUMNS_character_maximum_length; /* Column name to retrieve char
                                                   max length from SHOW
                                                   COLUMNS*/
    static const std::string
        kSHOW_COLUMNS_numeric_precision; /* Column name to retrieve numeric
                                            precision from SHOW COLUMNS*/
    static const std::string
        kSHOW_COLUMNS_numeric_scale; /* Column name to retrieve numeric scale
                                        from SHOW COLUMNS*/
    static const std::string
        kSHOW_COLUMNS_remarks; /* Column name to retrieve remarks from SHOW
                                  COLUMNS*/

    // Define column attributes for SQLTables
    static constexpr const char *kTablesCol[kTablesColNum] = {
        "TABLE_CAT", "TABLE_SCHEM", "TABLE_NAME", "TABLE_TYPE", "REMARKS"};
    static constexpr const int kTablesColDatatype[kTablesColNum] = {
        VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID};

    // Define column attributes for SQLColumns
    static constexpr const char *kColumnsCol[kColumnsColNum] = {
        "TABLE_CAT",         "TABLE_SCHEM",      "TABLE_NAME",
        "COLUMN_NAME",       "DATA_TYPE",        "TYPE_NAME",
        "COLUMN_SIZE",       "BUFFER_LENGTH",    "DECIMAL_DIGITS",
        "NUM_PREC_RADIX",    "NULLABLE",         "REMARKS",
        "COLUMN_DEF",        "SQL_DATA_TYPE",    "SQL_DATETIME_SUB",
        "CHAR_OCTET_LENGTH", "ORDINAL_POSITION", "IS_NULLABLE"};
    static constexpr const int kColumnsColDatatype[kColumnsColNum] = {
        VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID, INT2OID, VARCHAROID,
        INT4OID,    INT4OID,    INT2OID,    INT2OID,    INT2OID, VARCHAROID,
        VARCHAROID, INT2OID,    INT2OID,    INT4OID,    INT4OID, VARCHAROID};

    // Define the list of table type Redshift support
    static const std::vector<std::string> tableTypeList;

    struct DataTypeName {
        const std::string ksmallint = "smallint";
        const std::string kinteger = "integer";
        const std::string kbigint = "bigint";
        const std::string knumeric = "numeric";
        const std::string kreal = "real";
        const std::string kdouble_precision = "double precision";
        const std::string kboolean = "boolean";
        const std::string kcharacter = "character";
        const std::string kcharacter_varying = "character varying";
        const std::string kstring = "string";
        const std::string kdate = "date";
        const std::string ktime = "time";
        const std::string ktime_without_time_zone = "time without time zone";
        const std::string ktimetz = "timetz";
        const std::string ktime_with_time_zone = "time with time zone";
        const std::string ktimestamp = "timestamp";
        const std::string ktimestamp_without_time_zone = "timestamp without time zone";
        const std::string ktimestamptz = "timestamptz";
        const std::string ktimestamp_with_time_zone = "timestamp with time zone";
        const std::string kinterval_year_to_month = "interval year to month";
        const std::string kinterval_day_to_second = "interval day to second";
        const std::string kbinary = "binary";
        const std::string ksuper = "super";
        const std::string kgeometry = "geometry";
        const std::string kgeography = "geography";
        const std::string karray = "array";
        const std::string kmap = "map";
        const std::string kstruct = "struct";
    };
    static const DataTypeName getDataTypeNameStruct();

    struct RedshiftTypeName {
        const std::string kint2 = "int2";
        const std::string kint4 = "int4";
        const std::string kint8 = "int8";
        const std::string knumeric = "numeric";
        const std::string kfloat4 = "float4";
        const std::string kfloat8 = "float8";
        const std::string kbool = "bool";
        const std::string kchar = "char";
        const std::string kvarchar = "varchar";
        const std::string kdate = "date";
        const std::string ktime = "time";
        const std::string ktimetz = "timetz";
        const std::string ktimestamp = "timestamp";
        const std::string ktimestamptz = "timestamptz";
        const std::string kintervaly2m = "intervaly2m";
        const std::string kintervald2s = "intervald2s";
        const std::string kvarbyte = "varbyte";
        const std::string ksuper = "super";
        const std::string kgeometry = "geometry";
        const std::string kgeography = "geography";
    };
    static const RedshiftTypeName getRedshiftTypeNameStruct();

    static const std::regex glueSuperTypeRegex;

    /**
     * @brief Checks if ODBC2 datetime format should be used
     * 
     * @param odbcVersion The ODBC version being used
     * @return true if ODBC2 datetime format should be used, false otherwise
     */
    static bool returnODBC2DateTime(SQLINTEGER odbcVersion);
    
    /**
     * @brief Gets appropriate datetime data type information based on data type and ODBC version
     * 
     * @param type The data type name
     * @param isODBC_SpecV2 Flag indicating if ODBC2 format should be used
     * @return TypeInfoResult containing the mapped type information
     */
    static TypeInfoResult getAppropriateDateTime(const std::string& type, const bool isODBC_SpecV2);

    /**
     * @brief Checks if a given type is a datetime type
     * 
     * @param type The database type name to check
     * @return true if the type is a datetime type, false otherwise
     */
    static bool isDateTimeType(const std::string& type);

    /**
     * @brief Gets type information for a given data type
     * @param type Database type name
     * @param isODBC_SpecV2 ODBC version to determine datetime type
     * @return TypeInfoResult containing type information and success status
     */
    static TypeInfoResult getTypeInfo(const std::string& type, const bool isODBC_SpecV2);

    /**
     * @brief Set of all supported datetime types
     * Includes both with and without timezone variants
     */
    static const std::unordered_set<std::string> dateTimeDataTypes;

    // Define the data type info mapping between rsType and DATA_TYPE_INFO
    static const std::unordered_map<std::string, DATA_TYPE_INFO> typeInfoMap;

    static const std::unordered_map<std::string, DATA_TYPE_INFO> getTypeInfoMap();

    // Define mapping between rsType and column size
    static const std::unordered_map<std::string, int> columnSizeMap;

    // Define mapping between rsType and buffer len
    static const std::unordered_map<std::string, int> bufferLenMap;

    // Define mapping between rsType and number radix
    static const std::unordered_map<std::string, int> numPrecRadixMap;

    // Define set for character data type and binary data type
    static const std::unordered_set<std::string> charOctetLenSet;

    // Define QUOTE_IDENT query for prepare statement
    static const std::string quotedIdentQuery;

    // Define QUOTE_LITERAL query for prepare statement
    static const std::string quotedLiteralQuery;

    // Define empty string
    static const std::string SQL_ALL;

    /* ----------------
     * initializeColumnField
     *
     * helper function to initialize customize PGResult object and link to
     * RS_STMT_INFO object
     *
     * Parameters:
     *   pStm (RS_STMT_INFO*): statement object
     *   col (char**): an array of column name
     *   colNum (int): column number
     *   colDatatype (int*): an array of column data type
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN initializeColumnField(RS_STMT_INFO *pStmt, char **col,
                                           int colNum, int *colDatatype);

    /* ----------------
     * getColumnSize
     *
     * helper function to return column size
     *
     * Parameters:
     *   rsType (const std::string &): redshift type name
     *   character_maximum_length (short): char max length received from SHOW
     *      COLUMNS
     *   numeric_precision (short): numeric precision received from SHOW
     *      COLUMNS
     *
     * Return:
     *   column size (int)
     * ----------------
     */
    static int getColumnSize(const std::string &rsType,
                             short character_maximum_length,
                             short numeric_precision);

    /* ----------------
     * getBufferLen
     *
     * helper function to return buffer length
     *
     * Parameters:
     *   rsType (const std::string &): redshift type name
     *   character_maximum_length (short): char max length received from SHOW
     *      COLUMNS
     *   numeric_precision (short): numeric precision received from SHOW
     *      COLUMNS
     *
     * Return:
     *   buffer length (int)
     * ----------------
     */
    static int getBufferLen(const std::string &rsType,
                            short character_maximum_length,
                            short numeric_precision);

    /* ----------------
     * getDecimalDigit
     *
     * helper function to return decimal digit
     *
     * Parameters:
     *   rsType (const std::string &): redshift type name
     *   numeric_scale (short): numeric scale received from SHOW COLUMNS
     *   precisions (short): second fraction retrieve from data type name
     *      received from SHOW COLUMNS
     *   dateTimeCustomizePrecision (bool): specify if
     *      the table define customized second fraction
     *
     * Return:
     *   decimal digit (short)
     * ----------------
     */
    static short getDecimalDigit(const std::string &rsType, short numeric_scale,
                                 short precisions,
                                 bool dateTimeCustomizePrecision);

    /* ----------------
     * getNullable
     *
     * helper function to return nullable as number
     *
     *
     * Parameters:
     *   nullable (const std::string &): nullable string received from SHOW
     * COLUMNS
     *
     * Return:
     *   Nullable: 1
     *   Non-Nullable: 0
     *   Unknown: 2
     * ----------------
     */
    static short getNullable(const std::string &nullable);

    /* ----------------
     * getNumPrecRadix
     *
     * helper function to return nullable as number
     *
     *
     * Parameters:
     *   nullable (const std::string &): redshift type name
     *
     * Return:
     *   Either 10 or 2 for numeric data type
     * ----------------
     */
    static short getNumPrecRadix(const std::string &rsType);

    /* ----------------
     * getCharOctetLen
     *
     * helper function to return char octet length
     *
     * Parameters:
     *   rsType (const std::string &): redshift type name
     *   character_maximum_length (short): char max length received from SHOW
     *      COLUMNS
     *
     * Return:
     *   column size (int)
     * ----------------
     */
    static int getCharOctetLen(const std::string &rsType,
                               short character_maximum_length);
};

#endif // __RS_METADATAAPIHELPER_H__