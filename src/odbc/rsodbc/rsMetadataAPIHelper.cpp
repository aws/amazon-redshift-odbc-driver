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

// Constants for SHOW TABLES
const std::string RsMetadataAPIHelper::kSHOW_TABLES_database_name =
    "database_name";
const std::string RsMetadataAPIHelper::kSHOW_TABLES_schema_name = "schema_name";
const std::string RsMetadataAPIHelper::kSHOW_TABLES_table_name = "table_name";
const std::string RsMetadataAPIHelper::kSHOW_TABLES_table_type = "table_type";
const std::string RsMetadataAPIHelper::kSHOW_TABLES_remarks = "remarks";
const std::string RsMetadataAPIHelper::kSHOW_TABLES_owner = "owner";
const std::string RsMetadataAPIHelper::kSHOW_TABLES_last_altered_time = "last_altered_time";
const std::string RsMetadataAPIHelper::kSHOW_TABLES_last_modified_time = "last_modified_time";
const std::string RsMetadataAPIHelper::kSHOW_TABLES_dist_style = "dist_style";
const std::string RsMetadataAPIHelper::kSHOW_TABLES_table_subtype = "table_subtype";

// Constants for SHOW COLUMNS
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
const std::string RsMetadataAPIHelper::kSHOW_COLUMNS_remarks = 
    "remarks";
const std::string RsMetadataAPIHelper::kSHOW_COLUMNS_sort_key_type =
    "sort_key_type";
const std::string RsMetadataAPIHelper::kSHOW_COLUMNS_sort_key = "sort_key";
const std::string RsMetadataAPIHelper::kSHOW_COLUMNS_dist_key = "dist_key";
const std::string RsMetadataAPIHelper::kSHOW_COLUMNS_encoding = "encoding";
const std::string RsMetadataAPIHelper::kSHOW_COLUMNS_collation = "collation";

// Constants for SHOW CONSTRAINTS PK
const std::string RsMetadataAPIHelper::kSHOW_CONSTRAINTS_PK_database_name ="database_name";
const std::string RsMetadataAPIHelper::kSHOW_CONSTRAINTS_PK_schema_name ="schema_name";
const std::string RsMetadataAPIHelper::kSHOW_CONSTRAINTS_PK_table_name ="table_name";
const std::string RsMetadataAPIHelper::kSHOW_CONSTRAINTS_PK_column_name ="column_name";
const std::string RsMetadataAPIHelper::kSHOW_CONSTRAINTS_PK_key_seq ="key_seq";
const std::string RsMetadataAPIHelper::kSHOW_CONSTRAINTS_PK_pk_name ="pk_name";

// Constants for SHOW CONSTRAINTS FK
const std::string RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_pk_database_name = "pk_database_name";
const std::string RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_pk_schema_name = "pk_schema_name";
const std::string RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_pk_table_name = "pk_table_name";
const std::string RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_pk_column_name = "pk_column_name";
const std::string RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_fk_database_name = "fk_database_name";
const std::string RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_fk_schema_name = "fk_schema_name";
const std::string RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_fk_table_name = "fk_table_name";
const std::string RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_fk_column_name = "fk_column_name";
const std::string RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_key_seq = "key_seq";
const std::string RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_update_rule = "update_rule";
const std::string RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_delete_rule = "delete_rule";
const std::string RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_fk_name = "fk_name";
const std::string RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_pk_name = "pk_name";
const std::string RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_deferrability = "deferrability";

// Constants for SHOW GRANTS
const std::string RsMetadataAPIHelper::kSHOW_GRANTS_database_name = "database_name";
const std::string RsMetadataAPIHelper::kSHOW_GRANTS_schema_name = "schema_name";
const std::string RsMetadataAPIHelper::kSHOW_GRANTS_object_name = "object_name";
const std::string RsMetadataAPIHelper::kSHOW_GRANTS_table_name = "table_name";
const std::string RsMetadataAPIHelper::kSHOW_GRANTS_column_name = "column_name";
const std::string RsMetadataAPIHelper::kSHOW_GRANTS_grantor = "grantor_name";
const std::string RsMetadataAPIHelper::kSHOW_GRANTS_identity_name = "identity_name";
const std::string RsMetadataAPIHelper::kSHOW_GRANTS_privilege_type = "privilege_type";
const std::string RsMetadataAPIHelper::kSHOW_GRANTS_admin_option = "admin_option";

// Constants for SHOW PROCEDURES
const std::string RsMetadataAPIHelper::kSHOW_PROCEDURES_database_name = "database_name";
const std::string RsMetadataAPIHelper::kSHOW_PROCEDURES_schema_name = "schema_name";
const std::string RsMetadataAPIHelper::kSHOW_PROCEDURES_procedure_name = "procedure_name";
const std::string RsMetadataAPIHelper::kSHOW_PROCEDURES_return_type = "return_type";
const std::string RsMetadataAPIHelper::kSHOW_PROCEDURES_argument_list = "argument_list";

// Constants for SHOW FUNCTIONS
const std::string RsMetadataAPIHelper::kSHOW_FUNCTIONS_database_name = "database_name";
const std::string RsMetadataAPIHelper::kSHOW_FUNCTIONS_schema_name = "schema_name";
const std::string RsMetadataAPIHelper::kSHOW_FUNCTIONS_function_name = "function_name";
const std::string RsMetadataAPIHelper::kSHOW_FUNCTIONS_return_type = "return_type";
const std::string RsMetadataAPIHelper::kSHOW_FUNCTIONS_argument_list = "argument_list";

// Constants for SHOW PARAMETERS
const std::string RsMetadataAPIHelper::kSHOW_PARAMETERS_database_name = "database_name";
const std::string RsMetadataAPIHelper::kSHOW_PARAMETERS_schema_name = "schema_name";
const std::string RsMetadataAPIHelper::kSHOW_PARAMETERS_procedure_name = "procedure_name";
const std::string RsMetadataAPIHelper::kSHOW_PARAMETERS_function_name = "function_name";
const std::string RsMetadataAPIHelper::kSHOW_PARAMETERS_parameter_name = "parameter_name";
const std::string RsMetadataAPIHelper::kSHOW_PARAMETERS_ordinal_position = "ordinal_position";
const std::string RsMetadataAPIHelper::kSHOW_PARAMETERS_parameter_type = "parameter_type";
const std::string RsMetadataAPIHelper::kSHOW_PARAMETERS_data_type = "data_type";
const std::string RsMetadataAPIHelper::kSHOW_PARAMETERS_character_maximum_length = "character_maximum_length";
const std::string RsMetadataAPIHelper::kSHOW_PARAMETERS_numeric_precision = "numeric_precision";
const std::string RsMetadataAPIHelper::kSHOW_PARAMETERS_numeric_scale = "numeric_scale";


const std::vector<std::string> RsMetadataAPIHelper::tableTypeList = {
    "EXTERNAL TABLE", "SYSTEM TABLE",    "SYSTEM VIEW",    "TABLE",
    "TEMPORARY TABLE", "TEMPORARY VIEW", "VIEW"};

const std::string RsMetadataAPIHelper::SQL_EMPTY = "";

const std::string RsMetadataAPIHelper::kshowDatabasesQuery = "SHOW DATABASES;";
const std::string RsMetadataAPIHelper::kshowDatabasesLikeQuery = "SHOW DATABASES LIKE ?;";
const std::string RsMetadataAPIHelper::kshowSchemasQuery = "SHOW SCHEMAS from database ?;";
const std::string RsMetadataAPIHelper::kshowSchemasLikeQuery = "SHOW SCHEMAS from database ? LIKE ?;";
const std::string RsMetadataAPIHelper::kshowTablesQuery = "SHOW TABLES from schema ?.?;";
const std::string RsMetadataAPIHelper::kshowTablesLikeQuery = "SHOW TABLES from schema ?.? LIKE ?;";
const std::string RsMetadataAPIHelper::kshowColumnsQuery = "SHOW COLUMNS from table ?.?.?;";
const std::string RsMetadataAPIHelper::kshowColumnsLikeQuery = "SHOW COLUMNS from table ?.?.? LIKE ?;";
const std::string RsMetadataAPIHelper::kshowConstraintsPkQuery = "SHOW CONSTRAINTS PRIMARY KEYS FROM TABLE ?.?.?;";
const std::string RsMetadataAPIHelper::kshowConstraintsFkQuery = "SHOW CONSTRAINTS FOREIGN KEYS FROM TABLE ?.?.?;";
const std::string RsMetadataAPIHelper::kshowConstraintsFkExportQuery = "SHOW CONSTRAINTS FOREIGN KEYS EXPORTED FROM TABLE ?.?.?;";
const std::string RsMetadataAPIHelper::kshowGrantsTableQuery = "SHOW GRANTS ON TABLE ?.?.?;";
const std::string RsMetadataAPIHelper::kshowGrantsColumnQuery = "SHOW COLUMN GRANTS ON TABLE ?.?.?;";
const std::string RsMetadataAPIHelper::kshowGrantsColumnLikeQuery = "SHOW COLUMN GRANTS ON TABLE ?.?.? LIKE ?;";
const std::string RsMetadataAPIHelper::kshowProceduresQuery = "SHOW PROCEDURES from schema ?.?;";
const std::string RsMetadataAPIHelper::kshowProceduresLikeQuery = "SHOW PROCEDURES from schema ?.? LIKE ?;";
const std::string RsMetadataAPIHelper::kshowFunctionsQuery = "SHOW FUNCTIONS from schema ?.?;";
const std::string RsMetadataAPIHelper::kshowFunctionsLikeQuery = "SHOW FUNCTIONS from schema ?.? LIKE ?;";
const std::string RsMetadataAPIHelper::kshowParamsProcQuery = "SHOW PARAMETERS OF PROCEDURE ?.?.?";
const std::string RsMetadataAPIHelper::kshowParamsFuncQuery = "SHOW PARAMETERS OF FUNCTION ?.?.?";
const std::string RsMetadataAPIHelper::ksqlSemicolon = ";";
const std::string RsMetadataAPIHelper::ksqlLike = " LIKE ?;";



// ODBC 2.x column names
// Reference: https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlcolumns-function?view=sql-server-ver17
const char* RsMetadataAPIHelper::kOdbc2ColumnNames[] = {
    "TABLE_QUALIFIER",  // Instead of TABLE_CAT
    "TABLE_OWNER",      // Instead of TABLE_SCHEM
    "PRECISION",        // Instead of COLUMN_SIZE
    "LENGTH",           // Instead of BUFFER_LENGTH
    "SCALE",            // Instead of DECIMAL_DIGITS
    "RADIX",            // Instead of NUM_PREC_RADIX
    "PKTABLE_QUALIFIER",// Instead of PKTABLE_CAT
    "PKTABLE_OWNER",    // Instead of PKTABLE_SCHEM
    "FKTABLE_QUALIFIER",// Instead of FKTABLE_CAT
    "FKTABLE_OWNER",    // Instead of FKTABLE_SCHEM
    "PROCEDURE_QUALIFIER",// Instead of PROCEDURE_CAT
    "PROCEDURE_OWNER"    // Instead of PROCEDURE_SCHEM
};

// ODBC 3.x column names
const char* RsMetadataAPIHelper::kOdbc3ColumnNames[] = {
    "TABLE_CAT",
    "TABLE_SCHEM",
    "COLUMN_SIZE",
    "BUFFER_LENGTH",
    "DECIMAL_DIGITS",
    "NUM_PREC_RADIX",
    "PKTABLE_CAT",
    "PKTABLE_SCHEM",
    "FKTABLE_CAT",
    "FKTABLE_SCHEM",
    "PROCEDURE_CAT",
    "PROCEDURE_SCHEM"
};

const char* RsMetadataAPIHelper::getOdbc2ColumnName(int columnNum) {
    if(columnNum >= 0 && columnNum < kODBC2DiffColumnNum) {
        return (const char*)RsMetadataAPIHelper::kOdbc2ColumnNames[columnNum];
    }
    return nullptr;
}

// Define column attributes for SQLTables
const char* RsMetadataAPIHelper::kTablesCol[kTablesColNum] = {
    "TABLE_CAT",
    "TABLE_SCHEM",
    "TABLE_NAME",
    "TABLE_TYPE",
    "REMARKS",
    "OWNER",
    "LAST_ALTERED_TIME",
    "LAST_MODIFIED_TIME",
    "DIST_STYLE",
    "TABLE_SUBTYPE"
};

const char* RsMetadataAPIHelper::getSqlTablesColName(int columnNum) {
    if(columnNum >= 0 && columnNum < kTablesColNum) {
        return  (const char*)RsMetadataAPIHelper::kTablesCol[columnNum];
    }
    return nullptr;
}

// Define column attributes for SQLColumns
const char* RsMetadataAPIHelper::kColumnsCol[kColumnsColNum] = {
    "TABLE_CAT",         "TABLE_SCHEM",      "TABLE_NAME",
    "COLUMN_NAME",       "DATA_TYPE",        "TYPE_NAME",
    "COLUMN_SIZE",       "BUFFER_LENGTH",    "DECIMAL_DIGITS",
    "NUM_PREC_RADIX",    "NULLABLE",         "REMARKS",
    "COLUMN_DEF",        "SQL_DATA_TYPE",    "SQL_DATETIME_SUB",
    "CHAR_OCTET_LENGTH", "ORDINAL_POSITION", "IS_NULLABLE",
    "SORT_KEY_TYPE",     "SORT_KEY",         "DIST_KEY",
    "ENCODING",          "COLLATION"
};

const char* RsMetadataAPIHelper::getSqlColumnsColName(int columnNum) {
    if(columnNum >= 0 && columnNum < kColumnsColNum) {
        return  (const char*)RsMetadataAPIHelper::kColumnsCol[columnNum];
    }
    return nullptr;
}


// Define column attributes for SQLPrimaryKeys
const char* RsMetadataAPIHelper::kPrimaryKeysCol[kPrimaryKeysColNum] = {
    "TABLE_CAT",
    "TABLE_SCHEM",
    "TABLE_NAME",
    "COLUMN_NAME",
    "KEY_SEQ",
    "PK_NAME"
};

const char* RsMetadataAPIHelper::getSqlPrimaryKeysColName(int columnNum) {
    if(columnNum >= 0 && columnNum < kPrimaryKeysColNum) {
        return  (const char*)RsMetadataAPIHelper::kPrimaryKeysCol[columnNum];
    }
    return nullptr;
}

// Define column attributes for SQLForeignKeys
const char* RsMetadataAPIHelper::kForeignKeysCol[kForeignKeysColNum] = {
    "PKTABLE_CAT",    "PKTABLE_SCHEM",  "PKTABLE_NAME",   "PKCOLUMN_NAME",
    "FKTABLE_CAT",    "FKTABLE_SCHEM",  "FKTABLE_NAME",   "FKCOLUMN_NAME",
    "KEY_SEQ",        "UPDATE_RULE",     "DELETE_RULE",    "FK_NAME",
    "PK_NAME",        "DEFERRABILITY"
};

const char* RsMetadataAPIHelper::getSqlForeignKeysColName(int columnNum) {
    if(columnNum >= 0 && columnNum < kForeignKeysColNum) {
        return  (const char*)RsMetadataAPIHelper::kForeignKeysCol[columnNum];
    }
    return nullptr;
}

// Define column attributes for SQLSpecialColumns
const char* RsMetadataAPIHelper::kSpecialColumnsCol[kSpecialColumnsColNum] = {
    "SCOPE",          "COLUMN_NAME",     "DATA_TYPE",      "TYPE_NAME",
    "COLUMN_SIZE",    "BUFFER_LENGTH",   "DECIMAL_DIGITS", "PSEUDO_COLUMN"
};

const char* RsMetadataAPIHelper::getSqlSpecialColumnsColName(int columnNum) {
    if(columnNum >= 0 && columnNum < kSpecialColumnsColNum) {
        return  (const char*)RsMetadataAPIHelper::kSpecialColumnsCol[columnNum];
    }
    return nullptr;
}

// Define column attributes for SQLColumnPrivileges
const char* RsMetadataAPIHelper::kColumnPrivilegesCol[kColumnPrivilegesColNum] = {
    "TABLE_CAT",      "TABLE_SCHEM",     "TABLE_NAME",     "COLUMN_NAME",
    "GRANTOR",        "GRANTEE",         "PRIVILEGE",      "IS_GRANTABLE"
};

const char* RsMetadataAPIHelper::getSqlColumnPrivilegesColName(int columnNum) {
    if(columnNum >= 0 && columnNum < kColumnPrivilegesColNum) {
        return  (const char*)RsMetadataAPIHelper::kColumnPrivilegesCol[columnNum];
    }
    return nullptr;
}

// Define column attributes for SQLTablePrivileges
const char* RsMetadataAPIHelper::kTablePrivilegesCol[kTablePrivilegesColNum] = {
    "TABLE_CAT",      "TABLE_SCHEM",     "TABLE_NAME",     "GRANTOR",
    "GRANTEE",        "PRIVILEGE",       "IS_GRANTABLE"
};

const char* RsMetadataAPIHelper::getSqlTablePrivilegesColName(int columnNum) {
    if(columnNum >= 0 && columnNum < kTablePrivilegesColNum) {
        return  (const char*)RsMetadataAPIHelper::kTablePrivilegesCol[columnNum];
    }
    return nullptr;
}

// Define column attributes for SQLProcedures
const char* RsMetadataAPIHelper::kProceduresCol[kProceduresColNum] = {
    "PROCEDURE_CAT",     "PROCEDURE_SCHEM", "PROCEDURE_NAME", "NUM_INPUT_PARAMS",
    "NUM_OUTPUT_PARAMS", "NUM_RESULT_SETS", "REMARKS",        "PROCEDURE_TYPE"
};

const char* RsMetadataAPIHelper::getSqlProceduresColName(int columnNum) {
    if(columnNum >= 0 && columnNum < kProceduresColNum) {
        return  (const char*)RsMetadataAPIHelper::kProceduresCol[columnNum];
    }
    return nullptr;
}

// Define column attributes for SQLProcedureColumns
const char* RsMetadataAPIHelper::kProcedureColumnsCol[kProcedureColumnsColNum] = {
    "PROCEDURE_CAT",     "PROCEDURE_SCHEM",    "PROCEDURE_NAME",    "COLUMN_NAME",
    "COLUMN_TYPE",       "DATA_TYPE",          "TYPE_NAME",         "COLUMN_SIZE",
    "BUFFER_LENGTH",     "DECIMAL_DIGITS",     "NUM_PREC_RADIX",    "NULLABLE",
    "REMARKS",           "COLUMN_DEF",         "SQL_DATA_TYPE",     "SQL_DATETIME_SUB",
    "CHAR_OCTET_LENGTH", "ORDINAL_POSITION",   "IS_NULLABLE"
};

const char* RsMetadataAPIHelper::getSqlProcedureColumnsColName(int columnNum) {
    if(columnNum >= 0 && columnNum < kProcedureColumnsColNum) {
        return  (const char*)RsMetadataAPIHelper::kProcedureColumnsCol[columnNum];
    }
    return nullptr;
}

void RsMetadataAPIHelper::initializeColumnNames(RS_STMT_INFO* pStmt) {
    if (!pStmt || !pStmt->phdbc || !pStmt->phdbc->phenv || !pStmt->phdbc->phenv->pEnvAttr) {
        // Skip initialization if any required pointer in the statement hierarchy is null
        return;
    }

    bool isODBC2 = (pStmt->phdbc->phenv->pEnvAttr->iOdbcVersion == SQL_OV_ODBC2);

    // Initialize SQLTables columns
    kTablesCol[kSQLTables_TABLE_CATALOG_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[0] : kOdbc3ColumnNames[0];
    kTablesCol[kSQLTables_TABLE_SCHEM_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[1] : kOdbc3ColumnNames[1];

    // Initialize SQLColumns columns
    kColumnsCol[kSQLColumns_TABLE_CAT_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[0] : kOdbc3ColumnNames[0];
    kColumnsCol[kSQLColumns_TABLE_SCHEM_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[1] : kOdbc3ColumnNames[1];
    kColumnsCol[kSQLColumns_COLUMN_SIZE_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[2] : kOdbc3ColumnNames[2];
    kColumnsCol[kSQLColumns_BUFFER_LENGTH_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[3] : kOdbc3ColumnNames[3];
    kColumnsCol[kSQLColumns_DECIMAL_DIGITS_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[4] : kOdbc3ColumnNames[4];
    kColumnsCol[kSQLColumns_NUM_PREC_RADIX_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[5] : kOdbc3ColumnNames[5];

    // Initialize SQLPrimaryKeys columns
    kPrimaryKeysCol[kSQLPrimaryKeys_TABLE_CAT_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[0] : kOdbc3ColumnNames[0];
    kPrimaryKeysCol[kSQLPrimaryKeys_TABLE_SCHEM_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[1] : kOdbc3ColumnNames[1];

    // Initialize SQLForeignKeys columns
    kForeignKeysCol[kSQLForeignKeys_PKTABLE_CAT_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[6] : kOdbc3ColumnNames[6];
    kForeignKeysCol[kSQLForeignKeys_PKTABLE_SCHEM_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[7] : kOdbc3ColumnNames[7];
    kForeignKeysCol[kSQLForeignKeys_FKTABLE_CAT_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[8] : kOdbc3ColumnNames[8];
    kForeignKeysCol[kSQLForeignKeys_FKTABLE_SCHEM_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[9] : kOdbc3ColumnNames[9];

    // Initialize SQLSpecialColumns columns
    kSpecialColumnsCol[kSQLSpecialColumns_COLUMN_SIZE] = isODBC2 ? kOdbc2ColumnNames[2] : kOdbc3ColumnNames[2];
    kSpecialColumnsCol[kSQLSpecialColumns_BUFFER_LENGTH] = isODBC2 ? kOdbc2ColumnNames[3] : kOdbc3ColumnNames[3];
    kSpecialColumnsCol[kSQLSpecialColumns_DECIMAL_DIGITS] = isODBC2 ? kOdbc2ColumnNames[4] : kOdbc3ColumnNames[4];

    // Initialize SQLColumnPrivileges columns
    kColumnPrivilegesCol[kSQLColumnPrivileges_TABLE_CAT_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[0] : kOdbc3ColumnNames[0];
    kColumnPrivilegesCol[kSQLColumnPrivileges_TABLE_SCHEM_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[1] : kOdbc3ColumnNames[1];

    // Initialize SQLTablePrivileges columns
    kTablePrivilegesCol[kSQLTablePrivileges_TABLE_CAT_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[0] : kOdbc3ColumnNames[0];
    kTablePrivilegesCol[kSQLTablePrivileges_TABLE_SCHEM_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[1] : kOdbc3ColumnNames[1];

    // Initialize SQLProcedures columns
    kProceduresCol[kSQLProcedures_PROCEDURE_CAT_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[10] : kOdbc3ColumnNames[10];
    kProceduresCol[kSQLProcedures_PROCEDURE_SCHEM_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[11] : kOdbc3ColumnNames[11];

    // Initialize SQLProcedureColumns columns
    kProcedureColumnsCol[kSQLProcedureColumns_PROCEDURE_CAT_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[10] : kOdbc3ColumnNames[10];
    kProcedureColumnsCol[kSQLProcedureColumns_PROCEDURE_SCHEM_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[11] : kOdbc3ColumnNames[11];
    kProcedureColumnsCol[kSQLProcedureColumns_COLUMN_SIZE_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[2] : kOdbc3ColumnNames[2];
    kProcedureColumnsCol[kSQLProcedureColumns_BUFFER_LENGTH_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[3] : kOdbc3ColumnNames[3];
    kProcedureColumnsCol[kSQLProcedureColumns_DECIMAL_DIGITS_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[4] : kOdbc3ColumnNames[4];
    kProcedureColumnsCol[kSQLProcedureColumns_NUM_PREC_RADIX_COL_NUM] = isODBC2 ? kOdbc2ColumnNames[5] : kOdbc3ColumnNames[5];
    
}

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
// Checks if ODBC2 datetime format should be used
//
bool RsMetadataAPIHelper::returnODBC2DateTime(SQLINTEGER odbcVersion) {
    return (odbcVersion == SQL_OV_ODBC2);
}

// 
// Gets appropriate datetime data type information based on data type and ODBC version
//
TypeInfoResult RsMetadataAPIHelper::getAppropriateDateTime(const std::string& type, const bool isODBC_SpecV2) {
    auto dataTypeName = RsMetadataAPIHelper::getDataTypeNameStruct();
    auto redshiftTypeName = RsMetadataAPIHelper::getRedshiftTypeNameStruct();
    if (type == dataTypeName.kdate) {
        return TypeInfoResult(
            {
                (isODBC_SpecV2) ? SQL_DATE : SQL_TYPE_DATE,
                SQL_DATETIME,
                SQL_CODE_DATE,
                redshiftTypeName.kdate
            },
            true
        );
    }
    else if (type == dataTypeName.ktime || type == dataTypeName.ktime_without_time_zone) {
        return TypeInfoResult(
            {
                (isODBC_SpecV2) ? SQL_TIME : SQL_TYPE_TIME,
                SQL_DATETIME,
                SQL_CODE_TIME,
                redshiftTypeName.ktime
            },
            true
        );
    }
    else if (type == dataTypeName.ktimetz || type == dataTypeName.ktime_with_time_zone) {
        return TypeInfoResult(
            {
                (isODBC_SpecV2) ? SQL_TIME : SQL_TYPE_TIME,
                SQL_DATETIME,
                SQL_CODE_TIME,
                redshiftTypeName.ktimetz
            },
            true
        );
    }
    else if (type == dataTypeName.ktimestamp || type == dataTypeName.ktimestamp_without_time_zone) {
        return TypeInfoResult(
            {
                (isODBC_SpecV2) ? SQL_TIMESTAMP : SQL_TYPE_TIMESTAMP,
                SQL_DATETIME,
                SQL_CODE_TIMESTAMP,
                redshiftTypeName.ktimestamp
            },
            true
        );
    }
    else if (type == dataTypeName.ktimestamptz || type == dataTypeName.ktimestamp_with_time_zone) {
        return TypeInfoResult(
            {
                (isODBC_SpecV2) ? SQL_TIMESTAMP : SQL_TYPE_TIMESTAMP,
                SQL_DATETIME,
                SQL_CODE_TIMESTAMP,
                redshiftTypeName.ktimestamptz
            },
            true
        );
    }
    return TypeInfoResult::notFound();
}


//
// Checks if a given type is a datetime type
//
bool RsMetadataAPIHelper::isDateTimeType(const std::string& type) {
    return RsMetadataAPIHelper::dateTimeDataTypes.find(type) != RsMetadataAPIHelper::dateTimeDataTypes.end();
}

const std::unordered_set<std::string> RsMetadataAPIHelper::dateTimeDataTypes = {
    getDataTypeNameStruct().kdate,
    getDataTypeNameStruct().ktime,
    getDataTypeNameStruct().ktime_without_time_zone,
    getDataTypeNameStruct().ktimetz,
    getDataTypeNameStruct().ktime_with_time_zone,
    getDataTypeNameStruct().ktimestamp,
    getDataTypeNameStruct().ktimestamp_without_time_zone,
    getDataTypeNameStruct().ktimestamptz,
    getDataTypeNameStruct().ktimestamp_with_time_zone
};

//
// Gets type information for a given data type
//
TypeInfoResult RsMetadataAPIHelper::getTypeInfo(const std::string& type, const bool isODBC_SpecV2) {
    // Check for Glue super data type
    if (std::regex_match(type, RsMetadataAPIHelper::glueSuperTypeRegex)){
        return TypeInfoResult(
            {
                SQL_LONGVARCHAR,
                SQL_LONGVARCHAR,
                kNotApplicable,
                type
            },
            true
        );
    }

    if (isDateTimeType(type)) {
        return getAppropriateDateTime(type, isODBC_SpecV2);
    }

    auto it = RsMetadataAPIHelper::typeInfoMap.find(type);
    if (it != RsMetadataAPIHelper::typeInfoMap.end()){
        return TypeInfoResult(it->second, true);
    }
    return TypeInfoResult::notFound();
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
        {"varchar", {SQL_VARCHAR, SQL_VARCHAR, kNotApplicable, "varchar"}},
        {"smallint", {SQL_SMALLINT, SQL_SMALLINT, kNotApplicable, "int2"}},
        {"integer", {SQL_INTEGER, SQL_INTEGER, kNotApplicable, "int4"}},
        {"bigint", {SQL_BIGINT, SQL_BIGINT, kNotApplicable, "int8"}},
        {"real", {SQL_REAL, SQL_REAL, kNotApplicable, "float4"}},
        {"double precision",
         {SQL_DOUBLE, SQL_DOUBLE, kNotApplicable, "float8"}},
        {"numeric", {SQL_NUMERIC, SQL_NUMERIC, kNotApplicable, "numeric"}},
        {"boolean", {SQL_BIT, SQL_BIT, kNotApplicable, "bool"}},
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
         {SQL_UNKNOWN_TYPE, SQL_UNKNOWN_TYPE, kNotApplicable, "hllsketch"}},
        
         // Additional Glue data type
        {"binary", {SQL_LONGVARBINARY, SQL_LONGVARBINARY, kNotApplicable, "binary"}},
        {"string", {SQL_VARCHAR, SQL_VARCHAR, kNotApplicable, "string"}},
    };

ProcessedTypeInfo RsMetadataAPIHelper::processDataTypeInfo(std::string& dataType, int ODBCVer) {
    ProcessedTypeInfo result;
    result.typeInfoResult = TypeInfoResult::notFound();
    result.cleanedTypeName = "";
    result.dateTimeInfo.precision = 0;
    result.dateTimeInfo.hasCustomPrecision = false;

    if (std::regex_match(dataType, RsMetadataAPIHelper::dateTimeRegex) ||
        std::regex_match(dataType, RsMetadataAPIHelper::intervalRegex)) {
        // Clean the data type by removing precision
        std::string cleanedDataType = std::regex_replace(dataType, std::regex("\\(\\d+\\)"), "");
        trim(cleanedDataType);

        result.cleanedTypeName = cleanedDataType;

        // Get type information
        result.typeInfoResult = RsMetadataAPIHelper::getTypeInfo(
            cleanedDataType,
            RsMetadataAPIHelper::returnODBC2DateTime(ODBCVer));

        // Extract precision if present
        std::smatch match;
        if (std::regex_search(dataType, match, std::regex(".*\\(([0-9]+)\\).*"))) {
            result.dateTimeInfo.precision = std::stoi(match[1]);
            result.dateTimeInfo.hasCustomPrecision = true;
        }
    } else {
        // For non-datetime types, just get the type information directly
        result.cleanedTypeName = dataType;
        result.typeInfoResult = RsMetadataAPIHelper::getTypeInfo(
            dataType,
            RsMetadataAPIHelper::returnODBC2DateTime(ODBCVer));
    }

    return result;
}

const std::unordered_map<std::string, DATA_TYPE_INFO> RsMetadataAPIHelper::getTypeInfoMap() { return RsMetadataAPIHelper::typeInfoMap;}
const RsMetadataAPIHelper::DataTypeName RsMetadataAPIHelper::getDataTypeNameStruct(){ return RsMetadataAPIHelper::DataTypeName();}
const RsMetadataAPIHelper::RedshiftTypeName RsMetadataAPIHelper::getRedshiftTypeNameStruct(){ return RsMetadataAPIHelper::RedshiftTypeName();}

const std::regex RsMetadataAPIHelper::glueSuperTypeRegex("(array|map|struct).*");
const std::regex RsMetadataAPIHelper::dateTimeRegex(
        "(time|time without time zone|timetz|time with time "
        "zone|timestamp|timestamp without time zone|timestamptz|timestamp with "
        "time zone).*.\\(\\d+\\).*");
const std::regex RsMetadataAPIHelper::intervalRegex("interval.*.\\(\\d+\\)");

const std::set<std::string> RsMetadataAPIHelper::VALID_TYPES = {
    // Numeric types
    "smallint", "int2",
    "integer", "int", "int4",
    "bigint", "int8",
    "decimal", "numeric",
    "real", "float4",
    "double precision", "float8", "float",

    // Character types
    "char", "character", "nchar", "bpchar",
    "varchar", "character varying", "nvarchar", "text",

    // Date/Time types
    "date",
    "time", "time without time zone",
    "timetz", "time with time zone",
    "timestamp", "timestamp without time zone",
    "timestamptz", "timestamp with time zone",
    "intervaly2m", "interval year to month",
    "intervald2s", "interval day to second",

    // Other types
    "boolean", "bool",
    "hllsketch",
    "super",
    "varbyte", "varbinary", "binary varying",
    "geometry",
    "geography",

    // Legacy data types
    "oid",
    "smallint[]",
    "pg_attribute",
    "pg_type",
    "refcursor"
};

const std::unordered_map<std::string, int> RsMetadataAPIHelper::procedureFunctionColumnTypeMap = {
    {"IN", SQL_PARAM_INPUT},
    {"INOUT", SQL_PARAM_INPUT_OUTPUT},
    {"OUT", SQL_PARAM_OUTPUT},
    {"TABLE", SQL_RESULT_COL},
    {"RETURN", SQL_RETURN_VALUE}
};

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
        } else if (rsType == "char" || rsType == "varchar" || rsType == "string") {
            return character_maximum_length;
        } else if (rsType == "super" || rsType == "geometry" ||
                   rsType == "geography" || rsType == "varbyte" || rsType == "binary") {
            return kNotApplicable;
        } else if (std::regex_match(rsType, RsMetadataAPIHelper::glueSuperTypeRegex)){
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
        } else if (rsType == "char" || rsType == "varchar" || rsType == "string") {
            return character_maximum_length;
        } else if (rsType == "super" || rsType == "geometry" ||
                   rsType == "geography" || rsType == "varbyte" ||
                   rsType == "hllsketch" || rsType == "binary") {
            return kNotApplicable;
        } else if (std::regex_match(rsType, RsMetadataAPIHelper::glueSuperTypeRegex)){
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

bool RsMetadataAPIHelper::isValidType(const std::string& dataType) {
    return VALID_TYPES.find(dataType) != VALID_TYPES.end();
}

std::tuple<bool, std::vector<std::string>> RsMetadataAPIHelper::validateTypes(
    const std::vector<std::string>& dataTypes) {
    std::vector<std::string> invalidTypes;

    for (const auto& dtype : dataTypes) {
        if (!isValidType(dtype)) {
            invalidTypes.push_back(dtype);
        }
    }

    return std::make_tuple(invalidTypes.empty(), invalidTypes);
}

std::tuple<std::string, std::vector<std::string>> RsMetadataAPIHelper::createParameterizedQueryString(
    const std::string& argumentListStr,
    const std::string& sqlBase,
    const std::string& columnNamePattern) {

    // Handle empty argument list
    if (argumentListStr.empty()) {
        std::string sql = sqlBase + "()" +
            (columnNamePattern.empty() ? ksqlSemicolon : ksqlLike);
        return std::make_tuple(sql, std::vector<std::string>());
    }

    // Split the argument list
    std::vector<std::string> args;
    std::string tempList = argumentListStr;
    std::string temp;
    size_t pos = 0;
    std::string delimiter = ", ";

    while ((pos = tempList.find(delimiter)) != std::string::npos) {
        temp = tempList.substr(0, pos);
        if (temp.empty()) {
            throw std::invalid_argument(
                "Empty argument found in argument list: " + tempList +
                ". All arguments must be valid data types."
            );
        }
        args.push_back(temp);
        tempList = tempList.substr(pos + delimiter.length());
    }
    // Add last element
    if (!tempList.empty()) {
        args.push_back(tempList);
    }

    // Validate arguments
    if (!args.empty()) {
        auto [isValid, invalidTypes] = validateTypes(args);
        if (!isValid) {
            std::string invalidTypesStr;
            for (const auto& type : invalidTypes) {
                if (!invalidTypesStr.empty()) invalidTypesStr += ", ";
                invalidTypesStr += type;
            }
            throw std::invalid_argument(
                "Invalid data type(s) in argument list: " + invalidTypesStr +
                ". Argument list provided: " + argumentListStr
            );
        }
    }

    // Create the parameterized string
    std::string placeholders;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) placeholders += ", ";
        placeholders += "?";
    }

    std::string sql = sqlBase + "(" + placeholders + ")" +
        (columnNamePattern.empty() ? ksqlSemicolon : ksqlLike);

    return std::make_tuple(sql, args);
}

int RsMetadataAPIHelper::getProcedureFunctionColumnType(const std::string& parameterType) {
    if (parameterType.empty()) {
        return SQL_PARAM_TYPE_UNKNOWN;
    }

    // Convert to upper case for comparison
    std::string upperParamType = parameterType;
    std::transform(upperParamType.begin(), upperParamType.end(),
                  upperParamType.begin(), ::toupper);

    auto it = procedureFunctionColumnTypeMap.find(upperParamType);
    if (it != procedureFunctionColumnTypeMap.end()) {
        return it->second;
    }
    return SQL_PARAM_TYPE_UNKNOWN;
}

bool RsMetadataAPIHelper::patternMatch(const std::string& str, const std::string& pattern) {
    if (pattern.empty()) {
        return true; // Empty pattern matches any string
    }

    // Convert SQL LIKE pattern to regex
    std::string regexPattern = RsMetadataAPIHelper::convertSqlLikeToRegex(pattern);

    // Use regex to match
    std::regex compiledPattern(regexPattern, std::regex_constants::ECMAScript);

    return std::regex_match(str, compiledPattern);
}

std::string RsMetadataAPIHelper::convertSqlLikeToRegex(const std::string& pattern) {
    // Check if pattern only contains '%'
    bool onlyPercent = true;
    for (char c : pattern) {
        if (c != '%') {
            onlyPercent = false;
            break;
        }
    }
    if (onlyPercent) {
        return ".*";
    }

    std::string regexPattern;
    size_t i = 0;

    while (i < pattern.length()) {
        char ch = pattern[i];

        if (ch == '\\' && i + 1 < pattern.length()) {
            // Handle escaped characters
            char nextChar = pattern[i + 1];
            if (nextChar == '%' || nextChar == '_' || nextChar == '\\') {
                // Escape the next character for regex
                regexPattern += "\\";
                regexPattern += nextChar;
                i += 2;
            } else {
                // Not a special escape, treat backslash literally
                regexPattern += "\\\\"; // Escape backslash for regex
                i += 1;
            }
        } else if (ch == '%') {
            // % matches zero or more characters
            regexPattern += "[\\s\\S]*";
            i += 1;
        } else if (ch == '_') {
            // _ matches exactly one character
            regexPattern += "[\\s\\S]";
            i += 1;
        } else {
            // Regular character, escape special regex characters
            if (ch == '.' || ch == '^' || ch == '$' || ch == '*' || ch == '+' ||
                ch == '?' || ch == '(' || ch == ')' || ch == '[' || ch == ']' ||
                ch == '{' || ch == '}' || ch == '|') {
                regexPattern += "\\";
            }
            regexPattern += ch;
            i += 1;
        }
    }

    return "^" + regexPattern + "$";
}
