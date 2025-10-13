/*-------------------------------------------------------------------------
 *
 * Copyright(c) 2024, Amazon.com, Inc. or Its Affiliates. All rights reserved.
 *
 * Author: stinghsu
 *-------------------------------------------------------------------------
 */

#include "rsMetadataServerProxy.h"
#include "rsexecute.h"
#include "rsutil.h"

//
// Helper function to return intermediate result set for SQLTables special call
// to retrieve a list of catalog
//
SQLRETURN RsMetadataServerProxy::sqlCatalogs(
    SQLHSTMT phstmt, std::vector<std::string> &catalogs,
    bool isSingleDatabaseMetaData) {

    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    // Return current connected database name
    if (isSingleDatabaseMetaData) {
        catalogs.push_back(getDatabase(pStmt));
        return rc;
    }

    // Get a list of catalog name from SHOW DATABASES
    rc = callShowDatabases(phstmt, RsMetadataAPIHelper::SQL_ALL, catalogs, isSingleDatabaseMetaData);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        RS_LOG_ERROR("sqlCatalogs", "Error in callShowDatabases");
        addError(&pStmt->pErrorList, "HY000",
                 "sqlCatalogs: Error in callShowDatabases ", 0, NULL);
        return rc;
    }

    RS_LOG_TRACE("sqlCatalogs", "Total number of return rows: %zu", catalogs.size());
    
    return rc;
}

//
// Helper function to return intermediate result set for SQLTables special call
// to retrieve a list of schema
//
SQLRETURN RsMetadataServerProxy::sqlSchemas(
    SQLHSTMT phstmt, std::vector<SHOWSCHEMASResult> &intermediateRS,
    bool isSingleDatabaseMetaData) {

    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    std::vector<std::string> catalogs;

    // Get a list of catalog name from SHOW DATABASES
    rc = callShowDatabases(phstmt, RsMetadataAPIHelper::SQL_ALL, catalogs, isSingleDatabaseMetaData);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        RS_LOG_ERROR("sqlSchemas", "Error in callShowDatabases");
        addError(&pStmt->pErrorList, "HY000",
                 "sqlSchemas: Error in callShowDatabases ", 0, NULL);
        return rc;
    }

    for (int i = 0; i < catalogs.size(); i++) {
        // Get a list of schema name from SHOW SCHEMAS
        rc = callShowSchemas(phstmt, catalogs[i], RsMetadataAPIHelper::SQL_ALL, intermediateRS);
        if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
            RS_LOG_ERROR("sqlSchemas", "Error in callShowSchemas");
            addError(&pStmt->pErrorList, "HY000",
                     "sqlSchemas: Error in callShowSchemas ", 0,
                     NULL);
            return rc;
        }
    }
    catalogs.clear();

    RS_LOG_TRACE("sqlSchemas", "Total number of return rows: %zu", intermediateRS.size());
    
    return rc;
}

//
// Helper function to return intermediate result set for SQLTables
//
SQLRETURN RsMetadataServerProxy::sqlTables(
    SQLHSTMT phstmt, const std::string &catalogName,
    const std::string &schemaName, const std::string &tableName, bool retEmpty,
    std::vector<SHOWTABLESResult> &intermediateRS,
    bool isSingleDatabaseMetaData) {

    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    std::vector<std::string> catalogs;
    std::vector<SHOWSCHEMASResult> schemas;

    if (!retEmpty) {
        // Get a list of catalog name from SHOW DATABASES
        rc = callShowDatabases(phstmt, catalogName, catalogs,
                         isSingleDatabaseMetaData);
        if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
            RS_LOG_ERROR("sqlTables", "Error in callShowDatabases");
            addError(&pStmt->pErrorList, "HY000",
                     "sqlTables: Error in callShowDatabases ", 0, NULL);
            return rc;
        }
        for (int i = 0; i < catalogs.size(); i++) {
            // Get a list of schema name from SHOW SCHEMAS
            rc = callShowSchemas(phstmt, catalogs[i], schemaName, schemas);
            if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
                RS_LOG_ERROR("sqlTables", "Error in callShowSchemas");
                addError(&pStmt->pErrorList, "HY000",
                         "sqlTables: Error in callShowSchemas ", 0, NULL);
                return rc;
            }
            for (int j = 0; j < schemas.size(); j++) {
                // Get a list of table name from SHOW TABLES
                rc = callShowTables(phstmt, catalogs[i],
                                     char2String(schemas[j].schema_name),
                                     tableName, intermediateRS);
                if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
                    RS_LOG_ERROR("sqlTables", "Error in callShowTables");
                    addError(&pStmt->pErrorList, "HY000",
                             "sqlTables: Error in callShowTables ", 0,
                             NULL);
                    return rc;
                }
            }
            schemas.clear();
        }
        catalogs.clear();

        RS_LOG_TRACE("sqlTables", "Total number of return rows: %zu", intermediateRS.size());
    } else {
        RS_LOG_TRACE("sqlTables", "Return empty intermediateRS");
    }

    return rc;
}

//
// Helper function to return intermediate result set for SQLColumns
//
SQLRETURN RsMetadataServerProxy::sqlColumns(
    SQLHSTMT phstmt, const std::string &catalogName,
    const std::string &schemaName, const std::string &tableName,
    const std::string &columnName, bool retEmpty,
    std::vector<SHOWCOLUMNSResult> &intermediateRS,
    bool isSingleDatabaseMetaData) {

    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    std::vector<std::string> catalogs;
    std::vector<SHOWSCHEMASResult> schemas;
    std::vector<SHOWTABLESResult> tables;
    std::string sql;

    if (!retEmpty) {
        // Get a list of catalog name from SHOW DATABASES
        rc = callShowDatabases(phstmt, catalogName, catalogs,
                         isSingleDatabaseMetaData);
        if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
            RS_LOG_ERROR("sqlColumns", "Error in callShowDatabases");
            addError(&pStmt->pErrorList, "HY000",
                     "sqlColumns: Error in callShowDatabases ", 0, NULL);
            return rc;
        }

        for (int i = 0; i < catalogs.size(); i++) {
            // Get a list of schema name from SHOW SCHEMAS
            rc = callShowSchemas(phstmt, catalogs[i], schemaName, schemas);
            if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
                RS_LOG_ERROR("sqlColumns", "Error in callShowSchemas");
                addError(&pStmt->pErrorList, "HY000",
                         "sqlColumns: Error in callShowSchemas ", 0, NULL);
                return rc;
            }
            for (int j = 0; j < schemas.size(); j++) {
                // Get a list of table name from SHOW TABLES
                rc = callShowTables(phstmt, catalogs[i],
                                     char2String(schemas[j].schema_name),
                                     tableName, tables);
                if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
                    RS_LOG_ERROR("sqlColumns", "Error in callShowTables");
                    addError(&pStmt->pErrorList, "HY000",
                             "sqlColumns: Error in callShowTables ", 0,
                             NULL);
                    return rc;
                }
                for (int k = 0; k < tables.size(); k++) {
                    // Get a list of column name from SHOW COLUMNS
                    rc = callShowColumns(phstmt, catalogs[i],
                                          char2String(schemas[j].schema_name),
                                          char2String(tables[k].table_name),
                                          columnName, intermediateRS);
                    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
                        RS_LOG_ERROR("sqlColumns", "Error in callShowColumns");
                        addError(&pStmt->pErrorList, "HY000",
                                 "sqlColumns: Error in "
                                 "callShowColumns ",
                                 0, NULL);
                        return rc;
                    }
                }
                tables.clear();
            }
            schemas.clear();
        }
        catalogs.clear();

        RS_LOG_TRACE("sqlColumns", "Total number of return rows: %zu", intermediateRS.size());
    } else {
        RS_LOG_TRACE("sqlColumns", "Return empty intermediateRS");
    }

    return rc;
}

//
// Helper function to retrieve intermediate result set for SHOW DATABASES
//
SQLRETURN RsMetadataServerProxy::callShowDatabases(
    SQLHSTMT phstmt, const std::string &catalog,
    std::vector<std::string> &catalogs, bool isSingleDatabaseMetaData) {
    SQLRETURN rc = SQL_SUCCESS;
    SQLLEN catalogLen;
    SQLCHAR buf[MAX_IDEN_LEN];
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    std::string sql;

    std::string database = getDatabase(pStmt);
    if (database.empty()) {
        addError(
            &pStmt->pErrorList, "HY000",
            "callShowDatabases: Error when retrieving current database name",
            0, NULL);
        return SQL_ERROR;
    }

    // Call Server API SHOW DATABASES to get a list of Catalog
    if (catalog.empty()) {
        sql = "SHOW DATABASES;";
    } else {
        std::string quotedCatalog;
        rc = callQuoteFunc(phstmt, catalog, quotedCatalog, RsMetadataAPIHelper::quotedLiteralQuery);
        if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
            addError(&pStmt->pErrorList, "HY000",
                    "callShowTables: Fail to call QUOTE_LITERAL on catalog name", 0,
                    NULL);
            return rc;
        }
        sql = "SHOW DATABASES LIKE " + quotedCatalog + ";";
    }

    // Execute Server API call
    RS_LOG_DEBUG("callShowDatabases", "Execute SHOW query: %s", sql.c_str());
    setCatalogQueryBuf(pStmt, (char *)sql.c_str());
    // TODO: Support for prepare not ready yet
    rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)sql.c_str(), SQL_NTS,
                                     TRUE, FALSE, FALSE, TRUE);
    resetCatalogQueryFlag(pStmt);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        addError(&pStmt->pErrorList, "HY000",
                 "callShowDatabases: Fail to execute SHOW DATABASES ... ", 0,
                 NULL);
        return rc;
    }

    // Clean up the column binding
    rc = RS_STMT_INFO::RS_SQLFreeStmt(phstmt, SQL_UNBIND, FALSE);
    if (rc != SQL_SUCCESS) {
        addError(&pStmt->pErrorList, "HY000",
                 "callShowDatabases: Fail to clean up the column binding", 0,
                 NULL);
        return rc;
    }

    // Bind columns for SHOW DATABASES result set
    rc = RS_STMT_INFO::RS_SQLBindCol(
        pStmt,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_DATABASES_database_name),
        SQL_C_CHAR, buf, sizeof(buf), &catalogLen);
    if (rc != SQL_SUCCESS) {
        addError(&pStmt->pErrorList, "HY000",
                 "callShowDatabases: Fail to bind column for SHOW DATABASES "
                 "result ... ",
                 0, NULL);
        return SQL_ERROR;
    }

    // Retrieve result from SHOW DATABASES
    while (SQL_SUCCEEDED(
        rc = RS_STMT_INFO::RS_SQLFetchScroll(phstmt, SQL_FETCH_NEXT, 0))) {
        std::string cur = char2String(buf);
        if (!isSingleDatabaseMetaData || cur == database) {
            catalogs.push_back(cur);
        }
    }

    // Unbind columns for SHOW DATABASES result set
    releaseDescriptorRecByNum(
        pStmt->pStmtAttr->pARD,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_DATABASES_database_name));

    RS_LOG_TRACE("callShowDatabases", "number of catalog: %zu",
                 catalogs.size());

    // While loop will end if there's no more result to fetch. Therefore, rc
    // will be changed to SQL_ERROR 
    // Simply return SQL_SUCCESS since while loop was finished with no issue
    return SQL_SUCCESS;
}

//
// Helper function to retrieve intermediate result set for SHOW SCHEMAS
//
SQLRETURN RsMetadataServerProxy::callShowSchemas(
    SQLHSTMT phstmt, const std::string &catalog, const std::string &schema,
    std::vector<SHOWSCHEMASResult> &intermediateRS) {
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    SQLRETURN rc = SQL_SUCCESS;
    std::string sql;
    std::string quotedCatalog;

    // Input parameter check
    if (catalog.empty()) {
        addError(&pStmt->pErrorList, "HY000",
                 "callShowSchemas: catalog name should not be null or empty", 0,
                 NULL);
        return SQL_ERROR;
    }

    // Apply proper quoting and escaping for identifier
    rc = callQuoteFunc(phstmt, catalog, quotedCatalog, RsMetadataAPIHelper::quotedIdentQuery);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        addError(&pStmt->pErrorList, "HY000",
                 "callShowColumns: Fail to call QUOTE_IDENT on catalog name", 0,
                 NULL);
        return rc;
    }

    // Build query for SHOW SCHEMAS
    if (schema.empty()) {
        sql = "SHOW SCHEMAS FROM DATABASE " + quotedCatalog + ";";
    } else {
        // Apply proper quoting and escaping for literal
        std::string quotedSchema;
        rc = callQuoteFunc(phstmt, schema, quotedSchema, RsMetadataAPIHelper::quotedLiteralQuery);
        if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
            addError(&pStmt->pErrorList, "HY000",
                    "callShowTables: Fail to call QUOTE_LITERAL on schema name", 0,
                    NULL);
            return rc;
        }
        sql = "SHOW SCHEMAS FROM DATABASE " + quotedCatalog + " LIKE " + quotedSchema + ";";
    }

    // Execute Server API call
    RS_LOG_DEBUG("callShowSchemas", "Execute SHOW query: %s", sql.c_str());
    setCatalogQueryBuf(pStmt, (char *)sql.c_str());
    // TODO: Support for prepare not ready yet
    rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)sql.c_str(), SQL_NTS,
                                     TRUE, FALSE, FALSE, TRUE);
    resetCatalogQueryFlag(pStmt);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        addError(&pStmt->pErrorList, "HY000",
                 "callShowSchemas: Fail to execute SHOW SCHEMAS ... ", 0,
                 NULL);
        return rc;
    }

    // Clean up the column binding
    rc = RS_STMT_INFO::RS_SQLFreeStmt(phstmt, SQL_UNBIND, FALSE);
    if (rc != SQL_SUCCESS) {
        addError(&pStmt->pErrorList, "HY000",
                 "callShowSchemas: Fail to clean up the column binding", 0,
                 NULL);
        return rc;
    }

    // Bind columns for SHOW SCHEMAS result set
    SHOWSCHEMASResult cur;
    rc += RS_STMT_INFO::RS_SQLBindCol(
        pStmt,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_SCHEMAS_database_name),
        SQL_C_CHAR, cur.database_name, sizeof(cur.database_name),
        &cur.database_name_Len);
    rc += RS_STMT_INFO::RS_SQLBindCol(
        pStmt, getIndex(pStmt, RsMetadataAPIHelper::kSHOW_SCHEMAS_schema_name),
        SQL_C_CHAR, cur.schema_name, sizeof(cur.schema_name),
        &cur.schema_name_Len);

    if (rc != SQL_SUCCESS) {
        addError(&pStmt->pErrorList, "HY000",
                 "callShowSchemas: Fail to bind column for SHOW SCHEMAS "
                 "result ... ",
                 0, NULL);
        return SQL_ERROR;
    }

    // Retrieve result from SHOW SCHEMAS
    while (SQL_SUCCEEDED(
        rc = RS_STMT_INFO::RS_SQLFetchScroll(phstmt, SQL_FETCH_NEXT, 0))) {
        intermediateRS.push_back(cur);
    }

    // Unbind columns for SHOW SCHEMAS result set
    releaseDescriptorRecByNum(
        pStmt->pStmtAttr->pARD,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_SCHEMAS_database_name));
    releaseDescriptorRecByNum(
        pStmt->pStmtAttr->pARD,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_SCHEMAS_schema_name));

    
    // While loop will end if there's no more result to fetch. Therefore, rc
    // will be changed to SQL_ERROR 
    // Simply return SQL_SUCCESS since while loop was finished with no issue
    return SQL_SUCCESS;
}

//
// Helper function to retrieve intermediate result set for SHOW TABLES
//
SQLRETURN RsMetadataServerProxy::callShowTables(
    SQLHSTMT phstmt, const std::string &catalog, const std::string &schema,
    const std::string &table, std::vector<SHOWTABLESResult> &intermediateRS) {
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    SQLRETURN rc = SQL_SUCCESS;
    std::string sql;
    std::string quotedCatalog;
    std::string quotedSchema;

    // Input parameter check
    if (catalog.empty()) {
        addError(&pStmt->pErrorList, "HY000",
                 "callShowTables: catalog name should not be null or empty", 0, NULL);
        return SQL_ERROR;
    }

    if (schema.empty()) {
        addError(&pStmt->pErrorList, "HY000",
                 "callShowTables: schema name should not be null or empty", 0, NULL);
        return SQL_ERROR;
    }

    // Apply proper quoting and escaping for identifier
    rc = callQuoteFunc(phstmt, catalog, quotedCatalog, RsMetadataAPIHelper::quotedIdentQuery);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        addError(&pStmt->pErrorList, "HY000",
                 "callShowColumns: Fail to call QUOTE_IDENT on catalog name", 0,
                 NULL);
        return rc;
    }
    rc = callQuoteFunc(phstmt, schema, quotedSchema, RsMetadataAPIHelper::quotedIdentQuery);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        addError(&pStmt->pErrorList, "HY000",
                 "callShowColumns: Fail to call QUOTE_IDENT on schema name", 0,
                 NULL);
        return rc;
    }
    

    // Build query for SHOW TABLES
    if (table.empty()) {
        sql = "SHOW TABLES FROM SCHEMA " + quotedCatalog + "." + quotedSchema + ";";
    } else {
        // Apply proper quoting and escaping for literal
        std::string quotedTable;
        rc = callQuoteFunc(phstmt, table, quotedTable, RsMetadataAPIHelper::quotedLiteralQuery);
        if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
            addError(&pStmt->pErrorList, "HY000",
                    "callShowTables: Fail to call QUOTE_LITERAL on table name", 0,
                    NULL);
            return rc;
        }
        sql = "SHOW TABLES FROM SCHEMA " + quotedCatalog + "." + quotedSchema + " LIKE " + quotedTable + ";";
    }

    // Execute Server API call
    RS_LOG_DEBUG("callShowTables", "Execute SHOW query: %s", sql.c_str());
    setCatalogQueryBuf(pStmt, (char *)sql.c_str());
    // TODO: Support for prepare not ready yet
    rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)sql.c_str(), SQL_NTS,
                                     TRUE, FALSE, FALSE, TRUE);
    resetCatalogQueryFlag(pStmt);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        addError(&pStmt->pErrorList, "HY000",
                 "callShowTables: Fail to execute SHOW TABLES ... ", 0, NULL);
        return rc;
    }

    // Clean up the column binding
    rc = RS_STMT_INFO::RS_SQLFreeStmt(phstmt, SQL_UNBIND, FALSE);
    if (rc != SQL_SUCCESS) {
        addError(&pStmt->pErrorList, "HY000",
                 "callShowTables: Fail to clean up the column binding", 0,
                 NULL);
        return rc;
    }

    // Bind columns for SHOW TABLES result set
    SHOWTABLESResult cur;
    rc += RS_STMT_INFO::RS_SQLBindCol(
        pStmt, getIndex(pStmt, RsMetadataAPIHelper::kSHOW_TABLES_database_name),
        SQL_C_CHAR, cur.database_name, sizeof(cur.database_name),
        &cur.database_name_Len);
    rc += RS_STMT_INFO::RS_SQLBindCol(
        pStmt, getIndex(pStmt, RsMetadataAPIHelper::kSHOW_TABLES_schema_name),
        SQL_C_CHAR, cur.schema_name, sizeof(cur.schema_name),
        &cur.schema_name_Len);
    rc += RS_STMT_INFO::RS_SQLBindCol(
        pStmt, getIndex(pStmt, RsMetadataAPIHelper::kSHOW_TABLES_table_name),
        SQL_C_CHAR, cur.table_name, sizeof(cur.table_name),
        &cur.table_name_Len);
    rc += RS_STMT_INFO::RS_SQLBindCol(
        pStmt, getIndex(pStmt, RsMetadataAPIHelper::kSHOW_TABLES_table_type),
        SQL_C_CHAR, cur.table_type, sizeof(cur.table_type),
        &cur.table_type_Len);
    rc += RS_STMT_INFO::RS_SQLBindCol(
        pStmt, getIndex(pStmt, RsMetadataAPIHelper::kSHOW_TABLES_remarks),
        SQL_C_CHAR, cur.remarks, sizeof(cur.remarks), &cur.remarks_Len);
    if (rc != SQL_SUCCESS) {
        addError(&pStmt->pErrorList, "HY000",
                 "callShowTables: Fail to bind column for SHOW TABLES "
                 "result ... ",
                 0, NULL);
        return SQL_ERROR;
    }

    // Retrieve result from SHOW TABLES
    while (SQL_SUCCEEDED(
        rc = RS_STMT_INFO::RS_SQLFetchScroll(phstmt, SQL_FETCH_NEXT, 0))) {
        intermediateRS.push_back(cur);
    }

    // Unbind columns for SHOW TABLES result set
    releaseDescriptorRecByNum(
        pStmt->pStmtAttr->pARD,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_TABLES_database_name));
    releaseDescriptorRecByNum(
        pStmt->pStmtAttr->pARD,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_TABLES_schema_name));
    releaseDescriptorRecByNum(
        pStmt->pStmtAttr->pARD,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_TABLES_table_name));
    releaseDescriptorRecByNum(
        pStmt->pStmtAttr->pARD,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_TABLES_table_type));
    releaseDescriptorRecByNum(
        pStmt->pStmtAttr->pARD,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_TABLES_remarks));

    
    // While loop will end if there's no more result to fetch. Therefore, rc
    // will be changed to SQL_ERROR 
    // Simply return SQL_SUCCESS since while loop was finished with no issue
    return SQL_SUCCESS;
}

//
// Helper function to retrieve intermediate result set for SHOW COLUMNS
//
SQLRETURN RsMetadataServerProxy::callShowColumns(
    SQLHSTMT phstmt, const std::string &catalog, const std::string &schema,
    const std::string &table, const std::string &column,
    std::vector<SHOWCOLUMNSResult> &intermediateRS) {
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    SQLRETURN rc = SQL_SUCCESS;
    std::string sql;
    std::string quotedCatalog;
    std::string quotedSchema;
    std::string quotedTable;

    // Input parameter check
    if (catalog.empty()) {
        addError(&pStmt->pErrorList, "HY000",
                 "callShowColumns: catalog name should not be null or empty", 0,
                 NULL);
        return SQL_ERROR;
    }

    if (schema.empty()) {
        addError(&pStmt->pErrorList, "HY000",
                 "callShowColumns: schema name should not be null or empty", 0, NULL);
        return SQL_ERROR;
    }

    if (table.empty()) {
        addError(&pStmt->pErrorList, "HY000",
                 "callShowColumns: table name should not be null or empty", 0, NULL);
        return SQL_ERROR;
    }

    // Apply proper quoting and escaping for identifier
    rc = callQuoteFunc(phstmt, catalog, quotedCatalog, RsMetadataAPIHelper::quotedIdentQuery);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        addError(&pStmt->pErrorList, "HY000",
                 "callShowColumns: Fail to call QUOTE_IDENT on catalog name", 0,
                 NULL);
        return rc;
    }

    rc = callQuoteFunc(phstmt, schema, quotedSchema, RsMetadataAPIHelper::quotedIdentQuery);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        addError(&pStmt->pErrorList, "HY000",
                 "callShowColumns: Fail to call QUOTE_IDENT on schema name", 0,
                 NULL);
        return rc;
    }

    rc = callQuoteFunc(phstmt, table, quotedTable, RsMetadataAPIHelper::quotedIdentQuery);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        addError(&pStmt->pErrorList, "HY000",
                 "callShowColumns: Fail to call QUOTE_IDENT on table name", 0,
                 NULL);
        return rc;
    }

    // Build query for SHOW COLUMNS
    if (column.empty()) {
        sql = "SHOW COLUMNS FROM TABLE " + quotedCatalog + "." + quotedSchema + "." + quotedTable + ";";
    } else {
        // Apply proper quoting and escaping for literal
        std::string quotedColumn;
        rc = callQuoteFunc(phstmt, column, quotedColumn, RsMetadataAPIHelper::quotedLiteralQuery);
        if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
            addError(&pStmt->pErrorList, "HY000",
                    "callShowColumns: Fail to call QUOTE_LITERAL on column name", 0,
                    NULL);
            return rc;
        }
        sql = "SHOW COLUMNS FROM TABLE " + quotedCatalog + "." + quotedSchema + "." + quotedTable + " LIKE " + quotedColumn + ";";
    }

    // Execute Server API call
    RS_LOG_DEBUG("callShowColumns", "Execute SHOW query: %s", sql.c_str());
    setCatalogQueryBuf(pStmt, (char *)sql.c_str());
    // TODO: Support for prepare not ready yet
    rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)sql.c_str(), SQL_NTS,
                                     TRUE, FALSE, FALSE, TRUE);
    resetCatalogQueryFlag(pStmt);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        addError(&pStmt->pErrorList, "HY000",
                 "callShowColumns: Fail to execute SHOW COLUMNS ... ", 0,
                 NULL);
        return rc;
    }

    // Clean up the column binding
    rc = RS_STMT_INFO::RS_SQLFreeStmt(phstmt, SQL_UNBIND, FALSE);
    if (rc != SQL_SUCCESS) {
        addError(&pStmt->pErrorList, "HY000",
                 "callShowColumns: Fail to clean up the column binding", 0,
                 NULL);
        return rc;
    }

    // Bind columns for SHOW TABLES result set
    SHOWCOLUMNSResult cur;
    rc += RS_STMT_INFO::RS_SQLBindCol(
        pStmt,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_COLUMNS_database_name),
        SQL_C_CHAR, cur.database_name, sizeof(cur.database_name),
        &cur.database_name_Len);
    rc += RS_STMT_INFO::RS_SQLBindCol(
        pStmt, getIndex(pStmt, RsMetadataAPIHelper::kSHOW_COLUMNS_schema_name),
        SQL_C_CHAR, cur.schema_name, sizeof(cur.schema_name),
        &cur.schema_name_Len);
    rc += RS_STMT_INFO::RS_SQLBindCol(
        pStmt, getIndex(pStmt, RsMetadataAPIHelper::kSHOW_COLUMNS_table_name),
        SQL_C_CHAR, cur.table_name, sizeof(cur.table_name),
        &cur.table_name_Len);
    rc += RS_STMT_INFO::RS_SQLBindCol(
        pStmt, getIndex(pStmt, RsMetadataAPIHelper::kSHOW_COLUMNS_column_name),
        SQL_C_CHAR, cur.column_name, sizeof(cur.column_name),
        &cur.column_name_Len);
    rc += RS_STMT_INFO::RS_SQLBindCol(
        pStmt,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_COLUMNS_ordinal_position),
        SQL_C_SSHORT, &cur.ordinal_position, sizeof(cur.ordinal_position),
        &cur.ordinal_position_Len);
    rc += RS_STMT_INFO::RS_SQLBindCol(
        pStmt,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_COLUMNS_column_default),
        SQL_C_CHAR, cur.column_default, sizeof(cur.column_default),
        &cur.column_default_Len);
    rc += RS_STMT_INFO::RS_SQLBindCol(
        pStmt, getIndex(pStmt, RsMetadataAPIHelper::kSHOW_COLUMNS_is_nullable),
        SQL_C_CHAR, cur.is_nullable, sizeof(cur.is_nullable),
        &cur.is_nullable_Len);
    rc += RS_STMT_INFO::RS_SQLBindCol(
        pStmt, getIndex(pStmt, RsMetadataAPIHelper::kSHOW_COLUMNS_data_type),
        SQL_C_CHAR, cur.data_type, sizeof(cur.data_type), &cur.data_type_Len);
    rc += RS_STMT_INFO::RS_SQLBindCol(
        pStmt,
        getIndex(pStmt,
                 RsMetadataAPIHelper::kSHOW_COLUMNS_character_maximum_length),
        SQL_C_SSHORT, &cur.character_maximum_length,
        sizeof(cur.character_maximum_length),
        &cur.character_maximum_length_Len);
    rc += RS_STMT_INFO::RS_SQLBindCol(
        pStmt,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_COLUMNS_numeric_precision),
        SQL_C_SSHORT, &cur.numeric_precision, sizeof(cur.numeric_precision),
        &cur.numeric_precision_Len);
    rc += RS_STMT_INFO::RS_SQLBindCol(
        pStmt,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_COLUMNS_numeric_scale),
        SQL_C_SSHORT, &cur.numeric_scale, sizeof(cur.numeric_scale),
        &cur.numeric_scale_Len);
    rc += RS_STMT_INFO::RS_SQLBindCol(
        pStmt, getIndex(pStmt, RsMetadataAPIHelper::kSHOW_COLUMNS_remarks),
        SQL_C_CHAR, cur.remarks, sizeof(cur.remarks), &cur.remarks_Len);

    if (rc != SQL_SUCCESS) {
        addError(&pStmt->pErrorList, "HY000",
                 "callShowColumns: Fail to bind column for SHOW COLUMNS "
                 "result ... ",
                 0, NULL);
        return SQL_ERROR;
    }

    // Retrieve result from SHOW COLUMNS
    while (SQL_SUCCEEDED(
        rc = RS_STMT_INFO::RS_SQLFetchScroll(phstmt, SQL_FETCH_NEXT, 0))) {
        intermediateRS.push_back(cur);
    }

    // Unbind columns for SHOW TABLES result set
    releaseDescriptorRecByNum(
        pStmt->pStmtAttr->pARD,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_COLUMNS_database_name));
    releaseDescriptorRecByNum(
        pStmt->pStmtAttr->pARD,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_COLUMNS_schema_name));
    releaseDescriptorRecByNum(
        pStmt->pStmtAttr->pARD,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_COLUMNS_table_name));
    releaseDescriptorRecByNum(
        pStmt->pStmtAttr->pARD,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_COLUMNS_column_name));
    releaseDescriptorRecByNum(
        pStmt->pStmtAttr->pARD,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_COLUMNS_ordinal_position));
    releaseDescriptorRecByNum(
        pStmt->pStmtAttr->pARD,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_COLUMNS_column_default));
    releaseDescriptorRecByNum(
        pStmt->pStmtAttr->pARD,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_COLUMNS_is_nullable));
    releaseDescriptorRecByNum(
        pStmt->pStmtAttr->pARD,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_COLUMNS_data_type));
    releaseDescriptorRecByNum(
        pStmt->pStmtAttr->pARD,
        getIndex(pStmt,
                 RsMetadataAPIHelper::kSHOW_COLUMNS_character_maximum_length));
    releaseDescriptorRecByNum(
        pStmt->pStmtAttr->pARD,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_COLUMNS_numeric_precision));
    releaseDescriptorRecByNum(
        pStmt->pStmtAttr->pARD,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_COLUMNS_numeric_scale));
    releaseDescriptorRecByNum(
        pStmt->pStmtAttr->pARD,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_COLUMNS_remarks));

    
    // While loop will end if there's no more result to fetch. Therefore, rc
    // will be changed to SQL_ERROR 
    // Simply return SQL_SUCCESS since while loop was finished with no issue
    return SQL_SUCCESS;
}

SQLRETURN RsMetadataServerProxy::callQuoteFunc(
    SQLHSTMT phstmt, const std::string &input, std::string& output, const std::string &quotedQuery) { // output should be the last argument
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    SQLLEN lenIndex = input.size();
    SQLCHAR buf[MAX_IDEN_LEN] = {0};

    RS_LOG_TRACE("callQuoteFunc", "input string: %s, len: %zu", input.c_str(), input.size());

    rc = RsPrepare::RS_SQLPrepare(phstmt, (SQLCHAR *)quotedQuery.c_str(),
                                  SQL_NTS, FALSE, FALSE, FALSE, TRUE);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        addError(&pStmt->pErrorList, "HY000",
                 "callQuoteFunc: Fail to prepare QUOTE function ... ", 0,
                 NULL);
        return rc;
    }

     // Clean up the column binding
    rc = RS_STMT_INFO::RS_SQLFreeStmt(phstmt, SQL_UNBIND, FALSE);
    if (rc != SQL_SUCCESS) {
        addError(&pStmt->pErrorList, "HY000",
                 "callQuoteFunc: Fail to clean up the column binding", 0,
                 NULL);
        return rc;
    }

    rc = RsParameter::RS_SQLBindParameter(
        phstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, MAX_IDEN_LEN, 0,
        (SQLCHAR *)input.c_str(), input.size(), &lenIndex);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        addError(
            &pStmt->pErrorList, "HY000",
            "callQuoteFunc: Fail to bind parameter for QUOTE function ... ",
            0, NULL);
        return rc;
    }

    rc = RsExecute::RS_SQLExecDirect(phstmt, NULL, 0, FALSE, TRUE, FALSE, TRUE);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        addError(&pStmt->pErrorList, "HY000",
                 "callQuoteFunc: Fail to execute prepare statement for "
                 "QUOTE function ... ",
                 0, NULL);
        return rc;
    }

    releaseDescriptorRecByNum(pStmt->pStmtAttr->pARD, 1);

    rc = RS_STMT_INFO::RS_SQLFetchScroll(phstmt, SQL_FETCH_NEXT, 0);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        addError(
            &pStmt->pErrorList, "HY000",
            "callQuoteFunc: Fail to fetch result for QUOTE funcion ... ", 0,
            NULL);
        return rc;
    }

    rc = RS_STMT_INFO::RS_SQLGetData(pStmt, 1, SQL_C_CHAR, buf, sizeof(buf),
                                     &lenIndex, TRUE);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        addError(&pStmt->pErrorList, "HY000",
                 "callQuoteFunc: Fail to get data for QUOTE funcion ... ", 0,
                 NULL);
        return rc;
    }

    RS_LOG_TRACE("callQuoteFunc", "Quoted string: %s", buf);

    output = char2String(buf);
    return rc;
}