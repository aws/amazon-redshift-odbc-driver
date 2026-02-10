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
#include <algorithm>
#include <tuple>
#include <set>


// Define constant for SQLGetTypeInfo
#define kTypeInfoColNum 19 // Number of column
/* COLUMNS IN SQLGetTypeInfo() RESULT SET */
#define kSQLGetTypeInfo_TYPE_NAME_COL_NUM 0
#define kSQLGetTypeInfo_DATA_TYPE_COL_NUM 1
#define kSQLGetTypeInfo_COLUMN_SIZE_COL_NUM 2
#define kSQLGetTypeInfo_LITERAL_PREFIX_COL_NUM 3
#define kSQLGetTypeInfo_LITERAL_SUFFIX_COL_NUM 4
#define kSQLGetTypeInfo_CREATE_PARAMS_COL_NUM 5
#define kSQLGetTypeInfo_NULLABLE_COL_NUM 6
#define kSQLGetTypeInfo_CASE_SENSITIVE_COL_NUM 7
#define kSQLGetTypeInfo_SEARCHABLE_COL_NUM 8
#define kSQLGetTypeInfo_UNSIGNED_ATTRIBUTE_COL_NUM 9
#define kSQLGetTypeInfo_FIXED_PREC_SCALE_COL_NUM 10
#define kSQLGetTypeInfo_AUTO_UNIQUE_VAL_COL_NUM 11
#define kSQLGetTypeInfo_LOCAL_TYPE_NAME_COL_NUM 12
#define kSQLGetTypeInfo_MINIMUM_SCALE_COL_NUM 13
#define kSQLGetTypeInfo_MAXIMUM_SCALE_COL_NUM 14
#define kSQLGetTypeInfo_SQL_DATA_TYPE_COL_NUM 15
#define kSQLGetTypeInfo_SQL_DATETIME_SUB_COL_NUM 16
#define kSQLGetTypeInfo_NUM_PREC_RADIX_COL_NUM 17
#define kSQLGetTypeInfo_INTERVAL_PRECISION_COL_NUM 18

// Define constant for SQLTables
// Spec:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqltables-function?view=sql-server-ver16
#define kTablesColNum 10 // Number of column
/* COLUMNS IN SQLTables() RESULT SET */
#define kSQLTables_TABLE_CATALOG_COL_NUM 0
#define kSQLTables_TABLE_SCHEM_COL_NUM 1
#define kSQLTables_TABLE_NAME_COL_NUM 2
#define kSQLTables_TABLE_TYPE_COL_NUM 3
#define kSQLTables_REMARKS_COL_NUM 4
#define kSQLTables_OWNER_COL_NUM 5
#define kSQLTables_LAST_ALTERED_TIME_COL_NUM 6
#define kSQLTables_LAST_MODIFIED_TIME_COL_NUM 7
#define kSQLTables_DIST_STYLE_COL_NUM 8
#define kSQLTables_TABLE_SUBTYPE_COL_NUM 9

// Define constant for SQLColumns
// Spec:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlcolumns-function?view=sql-server-ver16
#define kColumnsColNum 23 // Number of column
/* COLUMNS IN SQLColumns() RESULT SET */                     
#define kSQLColumns_TABLE_CAT_COL_NUM 0
#define kSQLColumns_TABLE_SCHEM_COL_NUM 1
#define kSQLColumns_TABLE_NAME_COL_NUM 2
#define kSQLColumns_COLUMN_NAME_COL_NUM 3
#define kSQLColumns_DATA_TYPE_COL_NUM 4
#define kSQLColumns_TYPE_NAME_COL_NUM 5
#define kSQLColumns_COLUMN_SIZE_COL_NUM 6
#define kSQLColumns_BUFFER_LENGTH_COL_NUM 7
#define kSQLColumns_DECIMAL_DIGITS_COL_NUM 8
#define kSQLColumns_NUM_PREC_RADIX_COL_NUM 9
#define kSQLColumns_NULLABLE_COL_NUM 10
#define kSQLColumns_REMARKS_COL_NUM 11
#define kSQLColumns_COLUMN_DEF_COL_NUM 12
#define kSQLColumns_SQL_DATA_TYPE_COL_NUM 13
#define kSQLColumns_SQL_DATETIME_SUB_COL_NUM 14
#define kSQLColumns_CHAR_OCTET_LENGTH_COL_NUM 15
#define kSQLColumns_ORDINAL_POSITION_COL_NUM 16
#define kSQLColumns_IS_NULLABLE_COL_NUM 17
#define kSQLColumns_SORT_KEY_TYPE_COL_NUM 18
#define kSQLColumns_SORT_KEY_COL_NUM 19
#define kSQLColumns_DIST_KEY_COL_NUM 20
#define kSQLColumns_ENCODING_COL_NUM 21
#define kSQLColumns_COLLATION_COL_NUM 22

// Define constant for SQLPrimaryKeys
#define kPrimaryKeysColNum 6 // Number of column
/* COLUMNS IN SQLPrimaryKeys() RESULT SET */
#define kSQLPrimaryKeys_TABLE_CAT_COL_NUM 0
#define kSQLPrimaryKeys_TABLE_SCHEM_COL_NUM 1
#define kSQLPrimaryKeys_TABLE_NAME_COL_NUM 2
#define kSQLPrimaryKeys_COLUMN_NAME_COL_NUM 3
#define kSQLPrimaryKeys_KEY_SEQ_COL_NUM 4
#define kSQLPrimaryKeys_PK_NAME_COL_NUM 5

// Define constant for SQLForeignKeys
#define kForeignKeysColNum 14 // Number of column
/* COLUMNS IN SQLForeignKeys() RESULT SET */
#define kSQLForeignKeys_PKTABLE_CAT_COL_NUM 0
#define kSQLForeignKeys_PKTABLE_SCHEM_COL_NUM 1
#define kSQLForeignKeys_PKTABLE_NAME_COL_NUM 2
#define kSQLForeignKeys_PKCOLUMN_NAME_COL_NUM 3
#define kSQLForeignKeys_FKTABLE_CAT_COL_NUM 4
#define kSQLForeignKeys_FKTABLE_SCHEM_COL_NUM 5
#define kSQLForeignKeys_FKTABLE_NAME_COL_NUM 6
#define kSQLForeignKeys_FKCOLUMN_NAME_COL_NUM 7
#define kSQLForeignKeys_KEY_SEQ_COL_NUM 8
#define kSQLForeignKeys_UPDATE_RULE_COL_NUM 9
#define kSQLForeignKeys_DELETE_RULE_COL_NUM 10
#define kSQLForeignKeys_FK_NAME_COL_NUM 11
#define kSQLForeignKeys_PK_NAME_COL_NUM 12
#define kSQLForeignKeys_DEFERRABILITY_COL_NUM 13

// Define constant for SQLSpecialColumns
#define kSpecialColumnsColNum 8 // Number of column
/* COLUMNS IN SQLSpecialColumns() RESULT SET */
#define kSQLSpecialColumns_SCOPE 0
#define kSQLSpecialColumns_COLUMN_NAME 1
#define kSQLSpecialColumns_DATA_TYPE 2
#define kSQLSpecialColumns_TYPE_NAME 3
#define kSQLSpecialColumns_COLUMN_SIZE 4
#define kSQLSpecialColumns_BUFFER_LENGTH 5
#define kSQLSpecialColumns_DECIMAL_DIGITS 6
#define kSQLSpecialColumns_PSEUDO_COLUMN 7

// Define constant for SQLColumnPrivileges
#define kColumnPrivilegesColNum 8 // Number of column
/* COLUMNS IN SQLColumnPrivileges() RESULT SET */
#define kSQLColumnPrivileges_TABLE_CAT_COL_NUM 0
#define kSQLColumnPrivileges_TABLE_SCHEM_COL_NUM 1
#define kSQLColumnPrivileges_TABLE_NAME_COL_NUM 2
#define kSQLColumnPrivileges_COLUMN_NAME_COL_NUM 3
#define kSQLColumnPrivileges_GRANTOR_COL_NUM 4
#define kSQLColumnPrivileges_GRANTEE_COL_NUM 5
#define kSQLColumnPrivileges_PRIVILEGE_COL_NUM 6
#define kSQLColumnPrivileges_IS_GRANTABLE_COL_NUM 7

// Define constant for SQLTablePrivileges
#define kTablePrivilegesColNum 7 // Number of column
/* COLUMNS IN SQLTablePrivileges() RESULT SET */
#define kSQLTablePrivileges_TABLE_CAT_COL_NUM 0
#define kSQLTablePrivileges_TABLE_SCHEM_COL_NUM 1
#define kSQLTablePrivileges_TABLE_NAME_COL_NUM 2
#define kSQLTablePrivileges_GRANTOR_COL_NUM 3
#define kSQLTablePrivileges_GRANTEE_COL_NUM 4
#define kSQLTablePrivileges_PRIVILEGE_COL_NUM 5
#define kSQLTablePrivileges_IS_GRANTABLE_COL_NUM 6

// Define constant for SQLProcedures
#define kProceduresColNum 8 // Number of column
/* COLUMNS IN SQLProcedures() RESULT SET */
#define kSQLProcedures_PROCEDURE_CAT_COL_NUM 0
#define kSQLProcedures_PROCEDURE_SCHEM_COL_NUM 1
#define kSQLProcedures_PROCEDURE_NAME_COL_NUM 2
#define kSQLProcedures_NUM_INPUT_PARAMS_COL_NUM 3
#define kSQLProcedures_NUM_OUTPUT_PARAMS_COL_NUM 4
#define kSQLProcedures_NUM_RESULT_SETS_COL_NUM 5
#define kSQLProcedures_REMARKS_COL_NUM 6
#define kSQLProcedures_PROCEDURE_TYPE_COL_NUM 7

// Define constant for SQLProcedureColumns
#define kProcedureColumnsColNum 19 // Number of column
/* COLUMNS IN SQLProcedureColumns() RESULT SET */
#define kSQLProcedureColumns_PROCEDURE_CAT_COL_NUM 0
#define kSQLProcedureColumns_PROCEDURE_SCHEM_COL_NUM 1
#define kSQLProcedureColumns_PROCEDURE_NAME_COL_NUM 2
#define kSQLProcedureColumns_COLUMN_NAME_COL_NUM 3
#define kSQLProcedureColumns_COLUMN_TYPE_COL_NUM 4
#define kSQLProcedureColumns_DATA_TYPE_COL_NUM 5
#define kSQLProcedureColumns_TYPE_NAME_COL_NUM 6
#define kSQLProcedureColumns_COLUMN_SIZE_COL_NUM 7
#define kSQLProcedureColumns_BUFFER_LENGTH_COL_NUM 8
#define kSQLProcedureColumns_DECIMAL_DIGITS_COL_NUM 9
#define kSQLProcedureColumns_NUM_PREC_RADIX_COL_NUM 10
#define kSQLProcedureColumns_NULLABLE_COL_NUM 11
#define kSQLProcedureColumns_REMARKS_COL_NUM 12
#define kSQLProcedureColumns_COLUMN_DEF_COL_NUM 13
#define kSQLProcedureColumns_SQL_DATA_TYPE_COL_NUM 14
#define kSQLProcedureColumns_SQL_DATETIME_SUB_COL_NUM 15
#define kSQLProcedureColumns_CHAR_OCTET_LENGTH_COL_NUM 16
#define kSQLProcedureColumns_ORDINAL_POSITION_COL_NUM 17
#define kSQLProcedureColumns_IS_NULLABLE_COL_NUM 18

#define kColumnNullable 1
#define kColumnNoNulls 0
#define kColumnNullableUnknown 2

#define kFloat4DecimalDigit 6
#define kFloat8DecimalDigit 15

#define kDefaultSecondPrecision 6

#define kNotApplicable -1

#define kUnknownColumnSize 2147483647

#define kODBC2DiffColumnNum 14

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
    static const std::string kSHOW_TABLES_owner; /* Column name to retrieve owner from SHOW TABLES*/
    static const std::string kSHOW_TABLES_last_altered_time; /* Column name to retrieve last_altered_time from SHOW TABLES*/
    static const std::string kSHOW_TABLES_last_modified_time; /* Column name to retrieve last_modified_time from SHOW TABLES*/
    static const std::string kSHOW_TABLES_dist_style; /* Column name to retrieve dist_style from SHOW TABLES*/
    static const std::string kSHOW_TABLES_table_subtype; /* Column name to retrieve table_subtype from SHOW TABLES*/

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
    static const std::string kSHOW_COLUMNS_sort_key_type; /* Column name to retrieve sort_key_type from SHOW COLUMNS*/
    static const std::string kSHOW_COLUMNS_sort_key; /* Column name to retrieve sort_key from SHOW COLUMNS*/
    static const std::string kSHOW_COLUMNS_dist_key; /* Column name to retrieve dist_key from SHOW COLUMNS*/
    static const std::string kSHOW_COLUMNS_encoding; /* Column name to retrieve encoding from SHOW COLUMNS*/
    static const std::string kSHOW_COLUMNS_collation; /* Column name to retrieve collation from SHOW COLUMNS*/

    // Constants for SHOW CONSTRAINS PK
    static const std::string kSHOW_CONSTRAINTS_PK_database_name;    /* Column name to retrieve database name from SHOW CONSTRAINTS PRIMARY KEY */
    static const std::string kSHOW_CONSTRAINTS_PK_schema_name;      /* Column name to retrieve schema name from SHOW CONSTRAINTS PRIMARY KEY */
    static const std::string kSHOW_CONSTRAINTS_PK_table_name;       /* Column name to retrieve table name from SHOW CONSTRAINTS PRIMARY KEY */
    static const std::string kSHOW_CONSTRAINTS_PK_column_name;      /* Column name to retrieve column name from SHOW CONSTRAINTS PRIMARY KEY */
    static const std::string kSHOW_CONSTRAINTS_PK_key_seq;          /* Column name to retrieve key sequence from SHOW CONSTRAINTS PRIMARY KEY */
    static const std::string kSHOW_CONSTRAINTS_PK_pk_name;          /* Column name to retrieve primary key name from SHOW CONSTRAINTS PRIMARY KEY */

    // Constants for SHOW CONSTRAINTS FK
    static const std::string kSHOW_CONSTRAINTS_FK_pk_database_name;  /* Column name to retrieve primary key database name from SHOW CONSTRAINTS FOREIGN KEY */
    static const std::string kSHOW_CONSTRAINTS_FK_pk_schema_name;    /* Column name to retrieve primary key schema name from SHOW CONSTRAINTS FOREIGN KEY */
    static const std::string kSHOW_CONSTRAINTS_FK_pk_table_name;     /* Column name to retrieve primary key table name from SHOW CONSTRAINTS FOREIGN KEY */
    static const std::string kSHOW_CONSTRAINTS_FK_pk_column_name;    /* Column name to retrieve primary key column name from SHOW CONSTRAINTS FOREIGN KEY */
    static const std::string kSHOW_CONSTRAINTS_FK_fk_database_name;  /* Column name to retrieve foreign key database name from SHOW CONSTRAINTS FOREIGN KEY */
    static const std::string kSHOW_CONSTRAINTS_FK_fk_schema_name;    /* Column name to retrieve foreign key schema name from SHOW CONSTRAINTS FOREIGN KEY */
    static const std::string kSHOW_CONSTRAINTS_FK_fk_table_name;     /* Column name to retrieve foreign key table name from SHOW CONSTRAINTS FOREIGN KEY */
    static const std::string kSHOW_CONSTRAINTS_FK_fk_column_name;    /* Column name to retrieve foreign key column name from SHOW CONSTRAINTS FOREIGN KEY */
    static const std::string kSHOW_CONSTRAINTS_FK_key_seq;           /* Column name to retrieve key sequence from SHOW CONSTRAINTS FOREIGN KEY */
    static const std::string kSHOW_CONSTRAINTS_FK_update_rule;       /* Column name to retrieve update rule from SHOW CONSTRAINTS FOREIGN KEY */
    static const std::string kSHOW_CONSTRAINTS_FK_delete_rule;       /* Column name to retrieve delete rule from SHOW CONSTRAINTS FOREIGN KEY */
    static const std::string kSHOW_CONSTRAINTS_FK_fk_name;           /* Column name to retrieve foreign key name from SHOW CONSTRAINTS FOREIGN KEY */
    static const std::string kSHOW_CONSTRAINTS_FK_pk_name;           /* Column name to retrieve primary key name from SHOW CONSTRAINTS FOREIGN KEY */
    static const std::string kSHOW_CONSTRAINTS_FK_deferrability;     /* Column name to retrieve deferrability from SHOW CONSTRAINTS FOREIGN KEY */

    // Constants for SHOW GRANTS
    static const std::string kSHOW_GRANTS_database_name;    /* Column name to retrieve database name from SHOW GRANTS */
    static const std::string kSHOW_GRANTS_schema_name;      /* Column name to retrieve schema name from SHOW GRANTS */
    static const std::string kSHOW_GRANTS_object_name;      /* Column name to retrieve object name from SHOW GRANTS */
    static const std::string kSHOW_GRANTS_table_name;       /* Column name to retrieve table name from SHOW GRANTS */
    static const std::string kSHOW_GRANTS_column_name;      /* Column name to retrieve column name from SHOW GRANTS */
    static const std::string kSHOW_GRANTS_grantor;          /* Column name to retrieve grantor from SHOW GRANTS */
    static const std::string kSHOW_GRANTS_identity_name;    /* Column name to retrieve identity name from SHOW GRANTS */
    static const std::string kSHOW_GRANTS_privilege_type;   /* Column name to retrieve privilege type from SHOW GRANTS */
    static const std::string kSHOW_GRANTS_admin_option;     /* Column name to retrieve admin option from SHOW GRANTS */

    // Constants for SHOW PROCEDURES
    static const std::string kSHOW_PROCEDURES_database_name;  /* Column name to retrieve database name from SHOW PROCEDURES */
    static const std::string kSHOW_PROCEDURES_schema_name;    /* Column name to retrieve schema name from SHOW PROCEDURES */
    static const std::string kSHOW_PROCEDURES_procedure_name; /* Column name to retrieve procedure name from SHOW PROCEDURES */
    static const std::string kSHOW_PROCEDURES_return_type;    /* Column name to retrieve return type from SHOW PROCEDURES */
    static const std::string kSHOW_PROCEDURES_argument_list;  /* Column name to retrieve argument list from SHOW PROCEDURES */

    // Constants for SHOW FUNCTIONS
    static const std::string kSHOW_FUNCTIONS_database_name;   /* Column name to retrieve database name from SHOW FUNCTIONS */
    static const std::string kSHOW_FUNCTIONS_schema_name;     /* Column name to retrieve schema name from SHOW FUNCTIONS */
    static const std::string kSHOW_FUNCTIONS_function_name;   /* Column name to retrieve function name from SHOW FUNCTIONS */
    static const std::string kSHOW_FUNCTIONS_return_type;     /* Column name to retrieve return type from SHOW FUNCTIONS */
    static const std::string kSHOW_FUNCTIONS_argument_list;   /* Column name to retrieve argument list from SHOW FUNCTIONS */

    // Constants for SHOW PARAMETERS
    static const std::string kSHOW_PARAMETERS_database_name;
    static const std::string kSHOW_PARAMETERS_schema_name;
    static const std::string kSHOW_PARAMETERS_procedure_name;
    static const std::string kSHOW_PARAMETERS_function_name;
    static const std::string kSHOW_PARAMETERS_parameter_name;
    static const std::string kSHOW_PARAMETERS_ordinal_position;
    static const std::string kSHOW_PARAMETERS_parameter_type;
    static const std::string kSHOW_PARAMETERS_data_type;
    static const std::string kSHOW_PARAMETERS_character_maximum_length;
    static const std::string kSHOW_PARAMETERS_numeric_precision;
    static const std::string kSHOW_PARAMETERS_numeric_scale;


    static const char* kTypeInfoCol[kTypeInfoColNum];
    static const char* getSqlGetTypeInfoColName(int columnNum);
    static const char* kTablesCol[kTablesColNum];
    static const char* getSqlTablesColName(int columnNum);
    static const char* kColumnsCol[kColumnsColNum];
    static const char* getSqlColumnsColName(int columnNum);
    static const char* kPrimaryKeysCol[kPrimaryKeysColNum];
    static const char* getSqlPrimaryKeysColName(int columnNum);
    static const char* kForeignKeysCol[kForeignKeysColNum];
    static const char* getSqlForeignKeysColName(int columnNum);
    static const char* kSpecialColumnsCol[kSpecialColumnsColNum];
    static const char* getSqlSpecialColumnsColName(int columnNum);
    static const char* kColumnPrivilegesCol[kColumnPrivilegesColNum];
    static const char* getSqlColumnPrivilegesColName(int columnNum);
    static const char* kTablePrivilegesCol[kTablePrivilegesColNum];
    static const char* getSqlTablePrivilegesColName(int columnNum);
    static const char* kProceduresCol[kProceduresColNum];
    static const char* getSqlProceduresColName(int columnNum);
    static const char* kProcedureColumnsCol[kProcedureColumnsColNum];
    static const char* getSqlProcedureColumnsColName(int columnNum);

    static const char* kOdbc2ColumnNames[];
    static const char* getOdbc2ColumnName(int columnNum);
    static const char* kOdbc3ColumnNames[];
    static void initializeColumnNames(RS_STMT_INFO* pStmt);

    static constexpr const int kTypeInfoColDatatype[kTypeInfoColNum] = {
        VARCHAROID, INT2OID, INT4OID, VARCHAROID, VARCHAROID,
        VARCHAROID, INT2OID, INT2OID, INT2OID, INT2OID,
        INT2OID, INT2OID, VARCHAROID, INT2OID, INT2OID,
        INT2OID, INT2OID, INT4OID, INT2OID};

    static constexpr const int kTablesColDatatype[kTablesColNum] = {
        VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID,
        VARCHAROID, TIMESTAMPOID, TIMESTAMPOID, VARCHAROID, VARCHAROID};

    static constexpr const int kColumnsColDatatype[kColumnsColNum] = {
        VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID, INT2OID, VARCHAROID,
        INT4OID,    INT4OID,    INT2OID,    INT2OID,    INT2OID, VARCHAROID,
        VARCHAROID, INT2OID,    INT2OID,    INT4OID,    INT4OID, VARCHAROID,
        VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID};

    static constexpr const int kPrimaryKeysColDatatype[kPrimaryKeysColNum] = {
        VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID, INT2OID, VARCHAROID};

    static constexpr const int kForeignKeysColDatatype[kForeignKeysColNum] = {
        VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID,
        VARCHAROID, VARCHAROID, INT2OID,    INT2OID,    INT2OID,    VARCHAROID,
        VARCHAROID, INT2OID};

    static constexpr const int kSpecialColumnsColDatatype[kSpecialColumnsColNum] = {
        INT2OID, VARCHAROID, INT2OID, VARCHAROID, INT4OID, INT4OID, INT2OID, INT2OID};

    static constexpr const int kColumnPrivilegesColDatatype[kColumnPrivilegesColNum] = {
        VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID,
        VARCHAROID, VARCHAROID};

    static constexpr const int kTablePrivilegesColDatatype[kTablePrivilegesColNum] = {
        VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID,
        VARCHAROID};

    static constexpr const int kProceduresColDatatype[kProceduresColNum] = {
        VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID,
        VARCHAROID, INT2OID};

    static constexpr const int kProcedureColumnsColDatatype[kProcedureColumnsColNum] = {
        VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID, INT2OID,    INT4OID,
        VARCHAROID, INT4OID,    INT4OID,    INT2OID,    INT2OID,    INT2OID,
        VARCHAROID, VARCHAROID, INT4OID,    INT4OID,    INT4OID,    INT4OID,
        VARCHAROID};

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
    static const std::regex dateTimeRegex;
    static const std::regex intervalRegex;

    static const std::set<std::string> VALID_TYPES;
    static const std::unordered_map<std::string, int> procedureFunctionColumnTypeMap;

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

    // Define empty string
    static const std::string SQL_EMPTY;

    // Define query text for SHOW command
    static const std::string kshowDatabasesQuery;
    static const std::string kshowDatabasesLikeQuery;
    static const std::string kshowSchemasQuery;
    static const std::string kshowSchemasLikeQuery;
    static const std::string kshowTablesQuery;
    static const std::string kshowTablesLikeQuery;
    static const std::string kshowColumnsQuery;
    static const std::string kshowColumnsLikeQuery;
    static const std::string kshowConstraintsPkQuery;
    static const std::string kshowConstraintsFkQuery;
    static const std::string kshowConstraintsFkExportQuery;
    static const std::string kshowGrantsTableQuery;
    static const std::string kshowGrantsColumnQuery;
    static const std::string kshowGrantsColumnLikeQuery;
    static const std::string kshowProceduresQuery;
    static const std::string kshowProceduresLikeQuery;
    static const std::string kshowFunctionsQuery;
    static const std::string kshowFunctionsLikeQuery;
    static const std::string kshowParamsProcQuery;
    static const std::string kshowParamsFuncQuery;
    static const std::string ksqlSemicolon;
    static const std::string ksqlLike;

    static ProcessedTypeInfo processDataTypeInfo(std::string& dataType, int ODBCVer);

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

    /**
     * Checks if a given data type is a valid Redshift data type.
     * 
     * @param dataType The data type string to validate
     * @return true if the data type is valid (exists in VALID_TYPES set),
     *         false otherwise
     */
    static bool isValidType(const std::string& dataType);

    /**
     * Validates a list of data types against the predefined valid Redshift data types.
     * 
     * @param dataTypes Vector of data type strings to validate
     * @return A tuple containing:
     *         - bool: true if all data types are valid, false if any are invalid
     *         - vector<string>: list of invalid data types found (empty if all are valid)
     */
    static std::tuple<bool, std::vector<std::string>> validateTypes(
        const std::vector<std::string>& dataTypes);

    /**
     * Creates a parameterized SQL query string based on the argument list and base SQL statement.
     * Appends either a semicolon or LIKE clause depending on whether column_name_pattern is provided.
     *
     * @param argumentList Comma-separated string of argument types
     *                    (e.g. "integer, short, character varying")
     * @param sqlBase Base SQL statement to which parameters will be added
     *                (e.g. "SHOW PARAMETERS OF PROCEDURE")
     * @param columnNamePattern Optional pattern for filtering column names.
     *                         If provided, adds LIKE clause instead of semicolon
     * @return tuple containing:
     *         - Complete SQL query with appropriate placeholders and termination
     *         - Vector of argument types with whitespace stripped
     * @throws std::invalid_argument if invalid arguments are found
     */
    static std::tuple<std::string, std::vector<std::string>> createParameterizedQueryString(
        const std::string& argumentListStr,
        const std::string& sqlBase,
        const std::string& columnNamePattern);

    /**
     * Maps string parameter type to ProcedureColumnType enum value
     * @param parameterType String representation of parameter type
     * @return Corresponding ProcedureColumnType enum value
     */
    static int getProcedureFunctionColumnType(const std::string& parameterType);

    /**
     * Pattern matching function similar to SQL LIKE operator. Driver currently
     * treat empty string as null which match anything
     * @param str: input string to match against the pattern
     * @param pattern: pattern string containing wildcards
     * @return: true if string matches pattern, false otherwise
     *
     * Wildcards:
     * % - matches zero or more characters
     * _ - matches exactly one character
     */
    static bool patternMatch(const std::string& str, const std::string& pattern);

    /**
     * Convert SQL LIKE pattern to regex pattern
     * @param pattern SQL LIKE pattern with % and _ wildcards
     * @return Equivalent regex pattern
     */
    static std::string convertSqlLikeToRegex(const std::string& pattern);

};

#endif // __RS_METADATAAPIHELPER_H__
