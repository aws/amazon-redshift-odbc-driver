/*-------------------------------------------------------------------------
 *
 * Copyright(c) 2024, Amazon.com, Inc. or Its Affiliates. All rights reserved.
 *
 * Author: stinghsu
 *-------------------------------------------------------------------------
 */

#include "rsMetadataServerAPIHelper.h"
#include "rsexecute.h"
#include "rsutil.h"

//
// Helper function to retrieve a list of catalog for given catalog name
// (pattern)
//
SQLRETURN RsMetadataServerAPIHelper::getCatalogs(
    SQLHSTMT phstmt, const std::string &catalogName,
    std::vector<std::string> &catalogs, bool isSingleDatabaseMetaData) {

    SQLRETURN rc = SQL_SUCCESS;
    SQLLEN catalogLen;
    SQLCHAR buf[MAX_IDEN_LEN];
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    std::string sql;

    std::string database = getDatabase(pStmt);
    if (database.empty()) {
        addError(&pStmt->pErrorList, "HY000",
                 "getCatalogs: Error when retrieving current database name", 0,
                 NULL);
        return SQL_ERROR;
    }

    // Call Server API SHOW DATABASES to get a list of Catalog
    if (checkNameIsNotPattern(catalogName)) {
        sql = "SHOW DATABASES;";
    } else {
        sql = "SHOW DATABASES LIKE '" + catalogName + "';";
    }

    // Execute Server API call
    RS_LOG_DEBUG("getCatalogs", "Execute SHOW DATABASES for catalog = %s",
                 catalogName.c_str());
    setCatalogQueryBuf(pStmt, (char *)sql.c_str());
    // TODO: Support for prepare not ready yet
    rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)sql.c_str(), SQL_NTS,
                                     TRUE, FALSE, FALSE, TRUE);
    resetCatalogQueryFlag(pStmt);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        addError(&pStmt->pErrorList, "HY000",
                 "getCatalogs: Fail to execute SHOW DATABASES ... ", 0, NULL);
        return rc;
    }

    // Clean up the column binding
    rc = RS_STMT_INFO::RS_SQLFreeStmt(phstmt, SQL_UNBIND, FALSE);
    if (rc != SQL_SUCCESS) {
        addError(&pStmt->pErrorList, "HY000",
                 "getCatalogs: Fail to clean up the column binding", 0, NULL);
        return rc;
    }

    // Bind columns for SHOW DATABASES result set
    rc = RS_STMT_INFO::RS_SQLBindCol(
        pStmt,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_DATABASES_database_name),
        SQL_C_CHAR, buf, sizeof(buf), &catalogLen);
    if (rc != SQL_SUCCESS) {
        addError(&pStmt->pErrorList, "HY000",
                 "getCatalogs: Fail to bind column for SHOW DATABASES "
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

    // Unbind columns for SHOW TABLES result set
    releaseDescriptorRecByNum(
        pStmt->pStmtAttr->pARD,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_DATABASES_database_name));

    RS_LOG_TRACE("getCatalogs", "number of catalog: %d", catalogs.size());

    // While loop will end if there's no more result to fetch. Therefore, rc
    // will be changed to SQL_ERROR Simply return SQL_SUCCESS since while loop
    // is finished with no issue
    return SQL_SUCCESS;
}

//
// Helper function to retrieve a list of schema for given catalog name and
// schema name (pattern)
//
SQLRETURN RsMetadataServerAPIHelper::getSchemas(
    SQLHSTMT phstmt, const std::string &catalogName,
    const std::string &schemaName, std::vector<std::string> &schemas) {

    SQLRETURN rc = SQL_SUCCESS;
    SQLLEN schemaLen;
    SQLCHAR buf[MAX_IDEN_LEN];
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    std::string sql;

    // Input parameter check
    if (!checkNameIsExactName(catalogName)) {
        addError(&pStmt->pErrorList, "HY000",
                 "getSchemas: catalog name should be exact name", 0,
                 NULL);
        return SQL_ERROR;
    }

    // Call Server API SHOW SCHEMAS to get a list of Schema
    if (checkNameIsNotPattern(schemaName)) {
        sql = "SHOW SCHEMAS FROM DATABASE " + catalogName + ";";
    } else {
        sql = "SHOW SCHEMAS FROM DATABASE " + catalogName + " LIKE '" +
              schemaName + "';";
    }

    // Execute Server API call
    RS_LOG_DEBUG("getSchemas",
                 "Execute SHOW SCHEMAS for catalog = %s, schema = %s",
                 catalogName.c_str(), schemaName.c_str());
    setCatalogQueryBuf(pStmt, (char *)sql.c_str());
    // TODO: Support for prepare not ready yet
    rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)sql.c_str(), SQL_NTS,
                                     TRUE, FALSE, FALSE, TRUE);
    resetCatalogQueryFlag(pStmt);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        addError(&pStmt->pErrorList, "HY000",
                 "getSchemas: Fail to execute SHOW SCHEMAS ... ", 0, NULL);
        return rc;
    }

    // Clean up the column binding
    rc = RS_STMT_INFO::RS_SQLFreeStmt(phstmt, SQL_UNBIND, FALSE);
    if (rc != SQL_SUCCESS) {
        addError(&pStmt->pErrorList, "HY000",
                 "getSchemas: Fail to clean up the column binding", 0, NULL);
        return rc;
    }

    // Bind columns for SHOW SCHEMAS result set
    rc = RS_STMT_INFO::RS_SQLBindCol(
        pStmt, getIndex(pStmt, RsMetadataAPIHelper::kSHOW_SCHEMAS_schema_name),
        SQL_C_CHAR, buf, sizeof(buf), &schemaLen);
    if (rc != SQL_SUCCESS) {
        addError(&pStmt->pErrorList, "HY000",
                 "getSchemas: Fail to bind column for SHOW SCHEMAS result ... ",
                 0, NULL);
        return SQL_ERROR;
    }

    // Retrieve result from SHOW SCHEMAS
    while (SQL_SUCCEEDED(
        rc = RS_STMT_INFO::RS_SQLFetchScroll(phstmt, SQL_FETCH_NEXT, 0))) {
        schemas.push_back(char2String(buf));
    }

    // Unbind columns for SHOW SCHEMAS result set
    releaseDescriptorRecByNum(
        pStmt->pStmtAttr->pARD,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_SCHEMAS_schema_name));

    RS_LOG_TRACE("getSchemas", "number of schema: %d", schemas.size());

    // While loop will end if there's no more result to fetch. Therefore, rc
    // will be changed to SQL_ERROR Simply return SQL_SUCCESS since while loop
    // is finished with no issue
    return SQL_SUCCESS;
}

//
// Helper function to retrieve a list of table for given catalog name, schema
// name and table name (pattern)
//
SQLRETURN RsMetadataServerAPIHelper::getTables(
    SQLHSTMT phstmt, const std::string &catalogName,
    const std::string &schemaName, const std::string &tableName,
    std::vector<std::string> &tables) {

    SQLRETURN rc = SQL_SUCCESS;
    SQLLEN tableLen;
    SQLCHAR buf[MAX_IDEN_LEN];
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    std::string sql;

    // Input parameter check
    if (!checkNameIsExactName(catalogName)) {
        addError(&pStmt->pErrorList, "HY000",
                 "getTables: catalog name should be exact name", 0,
                 NULL);
        return SQL_ERROR;
    }

    if (!checkNameIsExactName(schemaName)) {
        addError(&pStmt->pErrorList, "HY000",
                 "getTables: schema name should be exact name", 0,
                 NULL);
        return SQL_ERROR;
    }

    // Call Server API SHOW TABLES to get a list of Table
    if (checkNameIsNotPattern(tableName)) {
        sql = "SHOW TABLES FROM SCHEMA " + catalogName + "." + schemaName + ";";
    } else {
        sql = "SHOW TABLES FROM SCHEMA " + catalogName + "." + schemaName +
              " LIKE '" + tableName + "';";
    }

    // Execute Server API call
    RS_LOG_DEBUG(
        "getTables",
        "Execute SHOW TABLES for catalog = %s, schema = %s, table = %s",
        catalogName.c_str(), schemaName.c_str(), tableName.c_str());
    setCatalogQueryBuf(pStmt, (char *)sql.c_str());
    // TODO: Support for prepare not ready yet
    rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)sql.c_str(), SQL_NTS,
                                     TRUE, FALSE, FALSE, TRUE);
    resetCatalogQueryFlag(pStmt);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        addError(&pStmt->pErrorList, "HY000",
                 "getTables: Fail to execute SHOW TABLES ... ", 0, NULL);
        return rc;
    }

    // Clean up the column binding
    rc = RS_STMT_INFO::RS_SQLFreeStmt(phstmt, SQL_UNBIND, FALSE);
    if (rc != SQL_SUCCESS) {
        addError(&pStmt->pErrorList, "HY000",
                 "getTables: Fail to clean up the column binding", 0, NULL);
        return rc;
    }

    // Bind columns for SHOW TABLES result set
    rc = RS_STMT_INFO::RS_SQLBindCol(
        pStmt, getIndex(pStmt, RsMetadataAPIHelper::kSHOW_TABLES_table_name),
        SQL_C_CHAR, buf, sizeof(buf), &tableLen);
    if (rc != SQL_SUCCESS) {
        addError(&pStmt->pErrorList, "HY000",
                 "getTables: Fail to bind column for SHOW TABLES result ... ",
                 0, NULL);
        return SQL_ERROR;
    }

    // Retrieve result from SHOW TABLES
    while (SQL_SUCCEEDED(
        rc = RS_STMT_INFO::RS_SQLFetchScroll(phstmt, SQL_FETCH_NEXT, 0))) {
        tables.push_back(char2String(buf));
    }

    // Unbind columns for SHOW TABLES result set
    releaseDescriptorRecByNum(
        pStmt->pStmtAttr->pARD,
        getIndex(pStmt, RsMetadataAPIHelper::kSHOW_TABLES_table_name));

    RS_LOG_TRACE("getTables", "number of table: %d", tables.size());

    // While loop will end if there's no more result to fetch. Therefore, rc
    // will be changed to SQL_ERROR Simply return SQL_SUCCESS since while loop
    // is finished with no issue
    return SQL_SUCCESS;
}

//
// Helper function to return intermediate result set for SQLTables special call
// to retrieve a list of catalog
//
SQLRETURN RsMetadataServerAPIHelper::sqlCatalogsServerAPI(
    SQLHSTMT phstmt, std::vector<std::string> &catalogs,
    bool isSingleDatabaseMetaData) {

    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    std::string catalog = "";

    rc = getCatalogs(phstmt, catalog, catalogs, isSingleDatabaseMetaData);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        addError(&pStmt->pErrorList, "HY000",
                 "sqlCatalogsServerAPI: Error in getCatalogs ", 0, NULL);
        return rc;
    }
    return rc;
}

//
// Helper function to return intermediate result set for SQLTables special call
// to retrieve a list of schema
//
SQLRETURN RsMetadataServerAPIHelper::sqlSchemasServerAPI(
    SQLHSTMT phstmt, std::vector<SHOWSCHEMASResult> &intermediateRS,
    bool isSingleDatabaseMetaData) {

    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    std::vector<std::string> catalogs;
    std::string catalog = "";

    // Get a list of catalog
    rc = getCatalogs(phstmt, catalog, catalogs, isSingleDatabaseMetaData);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        addError(&pStmt->pErrorList, "HY000",
                 "sqlSchemasServerAPI: Error in getCatalogs ", 0, NULL);
        return rc;
    }

    for (int i = 0; i < catalogs.size(); i++) {
        rc = call_show_schema(phstmt, catalogs[i], intermediateRS);
        if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
            addError(&pStmt->pErrorList, "HY000",
                     "sqlSchemasServerAPI: Error in call_show_schema ", 0,
                     NULL);
            return rc;
        }
    }
    catalogs.clear();
    RS_LOG_TRACE("sqlSchemasServerAPI",
                 "Successfully executed SHOW SCHEMAS. Number of rows: %d",
                 intermediateRS.size());
    return rc;
}

//
// Helper function to return intermediate result set for SQLTables
//
SQLRETURN RsMetadataServerAPIHelper::sqlTablesServerAPI(
    SQLHSTMT phstmt, const std::string &catalogName,
    const std::string &schemaName, const std::string &tableName, bool retEmpty,
    std::vector<SHOWTABLESResult> &intermediateRS,
    bool isSingleDatabaseMetaData) {

    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    std::vector<std::string> catalogs;
    std::vector<std::string> schemas;

    RS_LOG_DEBUG("sqlTablesServerAPI",
                 "Calling Server API SHOW TABLES for catalog = %s, schema = "
                 "%s, tableName = %s",
                 catalogName.c_str(), schemaName.c_str(), tableName.c_str());

    if (!retEmpty) {
        // Skip SHOW DATABASES API call if catalog name is specified
        if (checkNameIsExactName(catalogName)) {
            // Skip SHOW SCHEMA API call if schema name is specified
            if (checkNameIsExactName(schemaName)) {
                // Skip SHOW DATABASES & SHOW SCHEMAS
                // Call SHOW TABLES directly
                rc = call_show_table(phstmt, catalogName, schemaName, tableName,
                                     intermediateRS);
                if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
                    addError(&pStmt->pErrorList, "HY000",
                             "sqlTablesServerAPI: Error in call_show_table ", 0,
                             NULL);
                    return rc;
                }
            } else {
                // Skip SHOW DATABASES
                // Get a list of schema
                rc = getSchemas(phstmt, catalogName, schemaName, schemas);
                if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
                    addError(&pStmt->pErrorList, "HY000",
                             "sqlTablesServerAPI: Error in getSchemas ", 0,
                             NULL);
                    return rc;
                }
                for (int j = 0; j < schemas.size(); j++) {
                    // Skip SHOW DATABASES
                    // Call SHOW TABLES per schema
                    rc = call_show_table(phstmt, catalogName, schemas[j],
                                         tableName, intermediateRS);
                    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
                        addError(
                            &pStmt->pErrorList, "HY000",
                            "sqlTablesServerAPI: Error in call_show_table ", 0,
                            NULL);
                        return rc;
                    }
                }
                schemas.clear();
            }
        } else {
            // Get a list of Catalog
            rc = getCatalogs(phstmt, catalogName, catalogs,
                             isSingleDatabaseMetaData);
            if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
                addError(&pStmt->pErrorList, "HY000",
                         "sqlTablesServerAPI: Error in getCatalogs ", 0, NULL);
                return rc;
            }
            for (int i = 0; i < catalogs.size(); i++) {
                // Can't skip SHOW SCHEMAS if catalog is not specified to
                // prevent calling SHOW TABLES with catalog which required
                // additional permission
                // Get a list of schema per catalog
                rc = getSchemas(phstmt, catalogs[i], schemaName, schemas);
                if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
                    addError(&pStmt->pErrorList, "HY000",
                             "sqlTablesServerAPI: Error in getSchemas ", 0,
                             NULL);
                    return rc;
                }
                for (int j = 0; j < schemas.size(); j++) {
                    // Call SHOW TABLES per catalog/schema
                    rc = call_show_table(phstmt, catalogs[i], schemas[j],
                                         tableName, intermediateRS);
                    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
                        addError(
                            &pStmt->pErrorList, "HY000",
                            "sqlTablesServerAPI: Error in call_show_table ", 0,
                            NULL);
                        return rc;
                    }
                }
                schemas.clear();
            }
            catalogs.clear();
        }
        RS_LOG_TRACE("sqlTablesServerAPI",
                     "Successfully executed SHOW TABLES. Number of rows: %d",
                     intermediateRS.size());
    } else {
        RS_LOG_TRACE("sqlTablesServerAPI", "Return empty intermediateRS");
    }

    return rc;
}

//
// Helper function to return intermediate result set for SQLColumns
//
SQLRETURN RsMetadataServerAPIHelper::sqlColumnsServerAPI(
    SQLHSTMT phstmt, const std::string &catalogName,
    const std::string &schemaName, const std::string &tableName,
    const std::string &columnName, bool retEmpty,
    std::vector<SHOWCOLUMNSResult> &intermediateRS,
    bool isSingleDatabaseMetaData) {

    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    std::vector<std::string> catalogs;
    std::vector<std::string> schemas;
    std::vector<std::string> tables;
    std::string sql;

    RS_LOG_DEBUG("sqlColumnsServerAPI",
                 "Calling Server API SHOW COLUMNS for catalog = %s, schema = "
                 "%s, tableName = %s, columnName = %s",
                 catalogName.c_str(), schemaName.c_str(), tableName.c_str(),
                 columnName.c_str());

    if (!retEmpty) {
        // Skip SHOW DATABASES API call if catalog name is specified
        if (checkNameIsExactName(catalogName)) {
            // Skip SHOW SCHEMAS API call if schema name is specified
            if (checkNameIsExactName(schemaName)) {
                // Skip SHOW TABLES API call if table name is specified
                if (checkNameIsExactName(tableName)) {
                    // Skip SHOW DATABASES & SHOW SCHEMAS & SHOW TABLES
                    // Call SHOW COLUMNS directly
                    rc =
                        call_show_column(phstmt, catalogName, schemaName,
                                         tableName, columnName, intermediateRS);
                    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
                        addError(
                            &pStmt->pErrorList, "HY000",
                            "sqlColumnsServerAPI: Error in call_show_column ",
                            0, NULL);
                        return rc;
                    }
                } else {
                    // SKip SHOW DATABASES & SHOW SCHEMAS
                    // Get a list of table
                    rc = getTables(phstmt, catalogName, schemaName, tableName,
                                   tables);
                    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
                        addError(&pStmt->pErrorList, "HY000",
                                 "sqlColumnsServerAPI: Error in getTables ", 0,
                                 NULL);
                        return rc;
                    }
                    for (int k = 0; k < tables.size(); k++) {
                        // SKip SHOW DATABASES & SHOW SCHEMAS
                        // Call SHOW COLUMNS per table
                        rc = call_show_column(phstmt, catalogName, schemaName,
                                              tables[k], columnName,
                                              intermediateRS);
                        if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
                            addError(&pStmt->pErrorList, "HY000",
                                     "sqlColumnsServerAPI: Error in "
                                     "call_show_column ",
                                     0, NULL);
                            return rc;
                        }
                    }
                    tables.clear();
                }
            } else {
                // Skip SHOW DATABASES
                // Get a list of schema
                rc = getSchemas(phstmt, catalogName, schemaName, schemas);
                if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
                    addError(&pStmt->pErrorList, "HY000",
                             "sqlColumnsServerAPI: Error in getSchemas ", 0,
                             NULL);
                    return rc;
                }
                for (int j = 0; j < schemas.size(); j++) {
                    if (checkNameIsExactName(tableName)) {
                        // Skip SHOW DATABASES & SHOW TABLES
                        // Call SHOW COLUMNS per schema
                        rc = call_show_column(phstmt, catalogName, schemas[j],
                                              tableName, columnName,
                                              intermediateRS);
                        if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
                            addError(&pStmt->pErrorList, "HY000",
                                     "sqlColumnsServerAPI: Error in "
                                     "call_show_column ",
                                     0, NULL);
                            return rc;
                        }
                    } else {
                        // SKip SHOW DATABASES
                        // Get a list of table per schema
                        rc = getTables(phstmt, catalogName, schemas[j],
                                       tableName, tables);
                        if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
                            addError(&pStmt->pErrorList, "HY000",
                                     "sqlColumnsServerAPI: Error in getTables ",
                                     0, NULL);
                            return rc;
                        }
                        for (int k = 0; k < tables.size(); k++) {
                            // SKip SHOW DATABASES
                            // Call SHOW COLUMNS per schema/table
                            rc = call_show_column(phstmt, catalogName,
                                                  schemas[j], tables[k],
                                                  columnName, intermediateRS);
                            if (rc != SQL_SUCCESS &&
                                rc != SQL_SUCCESS_WITH_INFO) {
                                addError(&pStmt->pErrorList, "HY000",
                                         "sqlColumnsServerAPI: Error in "
                                         "call_show_column ",
                                         0, NULL);
                                return rc;
                            }
                        }
                        tables.clear();
                    }
                }
                schemas.clear();
            }
        } else {
            // Get a list of Catalog
            rc = getCatalogs(phstmt, catalogName, catalogs,
                             isSingleDatabaseMetaData);
            if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
                addError(&pStmt->pErrorList, "HY000",
                         "sqlColumnsServerAPI: Error in getCatalogs ", 0, NULL);
                return rc;
            }

            for (int i = 0; i < catalogs.size(); i++) {
                // Can't skip SHOW SCHEMAS if catalog is not specified to
                // prevent calling SHOW TABLES with catalog which required
                // additional permission Get list of schema per catalog
                rc = getSchemas(phstmt, catalogs[i], schemaName, schemas);
                if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
                    addError(&pStmt->pErrorList, "HY000",
                             "sqlColumnsServerAPI: Error in getSchemas ", 0,
                             NULL);
                    return rc;
                }
                for (int j = 0; j < schemas.size(); j++) {
                    // Skip SHOW TABLES API call if table name is specified
                    if (checkNameIsExactName(tableName)) {
                        // Skip SHOW TABLES
                        // Call SHOW COLUMNS per catalog/schema
                        rc = call_show_column(phstmt, catalogs[i], schemas[j],
                                              tableName, columnName,
                                              intermediateRS);
                        if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
                            addError(&pStmt->pErrorList, "HY000",
                                     "sqlColumnsServerAPI: Error in "
                                     "call_show_column ",
                                     0, NULL);
                            return rc;
                        }
                    } else {
                        // Get a list of table per catalog/schema
                        rc = getTables(phstmt, catalogs[i], schemas[j],
                                       tableName, tables);
                        if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
                            addError(&pStmt->pErrorList, "HY000",
                                     "sqlColumnsServerAPI: Error in getTables ",
                                     0, NULL);
                            return rc;
                        }
                        for (int k = 0; k < tables.size(); k++) {
                            // Call SHOW COLUMNS per catalog/schema/table
                            rc = call_show_column(phstmt, catalogs[i],
                                                  schemas[j], tables[k],
                                                  columnName, intermediateRS);
                            if (rc != SQL_SUCCESS &&
                                rc != SQL_SUCCESS_WITH_INFO) {
                                addError(&pStmt->pErrorList, "HY000",
                                         "sqlColumnsServerAPI: Error in "
                                         "call_show_column ",
                                         0, NULL);
                                return rc;
                            }
                        }
                        tables.clear();
                    }
                }
                schemas.clear();
            }
            catalogs.clear();
        }
        RS_LOG_TRACE("sqlColumnsServerAPI",
                     "Successfully executed SHOW COLUMNS. Number of rows: %d",
                     intermediateRS.size());
    } else {
        RS_LOG_TRACE("sqlColumnsServerAPI", "Return empty intermediateRS");
    }

    return rc;
}

//
// Helper function to retrieve intermediate result set for SHOW SCHEMAS
//
SQLRETURN RsMetadataServerAPIHelper::call_show_schema(
    SQLHSTMT phstmt, const std::string &catalog,
    std::vector<SHOWSCHEMASResult> &intermediateRS) {
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    SQLRETURN rc = SQL_SUCCESS;
    std::string sql = {0};

    // Input parameter check
    if (!checkNameIsExactName(catalog)) {
        addError(&pStmt->pErrorList, "HY000",
                 "call_show_schema: catalog name should be exact name", 0,
                 NULL);
        return SQL_ERROR;
    }

    // Build query for SHOW SCHEMAS
    sql = "SHOW SCHEMAS FROM DATABASE " + catalog + ";";

    // Execute Server API call
    setCatalogQueryBuf(pStmt, (char *)sql.c_str());
    // TODO: Support for prepare not ready yet
    rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)sql.c_str(), SQL_NTS,
                                     TRUE, FALSE, FALSE, TRUE);
    resetCatalogQueryFlag(pStmt);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        addError(&pStmt->pErrorList, "HY000",
                 "call_show_schema: Fail to execute SHOW TABLES ... ", 0, NULL);
        return rc;
    }

    // Clean up the column binding
    rc = RS_STMT_INFO::RS_SQLFreeStmt(phstmt, SQL_UNBIND, FALSE);
    if (rc != SQL_SUCCESS) {
        addError(&pStmt->pErrorList, "HY000",
                 "call_show_schema: Fail to clean up the column binding", 0,
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
                 "call_show_schema: Fail to bind column for SHOW SCHEMAS "
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

    return SQL_SUCCESS;
}

//
// Helper function to retrieve intermediate result set for SHOW TABLES
//
SQLRETURN RsMetadataServerAPIHelper::call_show_table(
    SQLHSTMT phstmt, const std::string &catalog, const std::string &schema,
    const std::string &table, std::vector<SHOWTABLESResult> &intermediateRS) {
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    SQLRETURN rc = SQL_SUCCESS;
    std::string sql = {0};

    // Input parameter check
    if (!checkNameIsExactName(catalog)) {
        addError(&pStmt->pErrorList, "HY000",
                 "call_show_table: catalog name should be exact name", 0, NULL);
        return SQL_ERROR;
    }

    if (!checkNameIsExactName(schema)) {
        addError(&pStmt->pErrorList, "HY000",
                 "call_show_table: schema name should be exact name", 0, NULL);
        return SQL_ERROR;
    }

    // Build query for SHOW TABLES
    if (checkNameIsNotPattern(table)) {
        sql = "SHOW TABLES FROM SCHEMA " + catalog + "." + schema + ";";
    } else {
        sql = "SHOW TABLES FROM SCHEMA " + catalog + "." + schema + " LIKE '" +
              table + "';";
    }

    // Execute Server API call
    setCatalogQueryBuf(pStmt, (char *)sql.c_str());
    // TODO: Support for prepare not ready yet
    rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)sql.c_str(), SQL_NTS,
                                     TRUE, FALSE, FALSE, TRUE);
    resetCatalogQueryFlag(pStmt);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        addError(&pStmt->pErrorList, "HY000",
                 "call_show_table: Fail to execute SHOW TABLES ... ", 0, NULL);
        return rc;
    }

    // Clean up the column binding
    rc = RS_STMT_INFO::RS_SQLFreeStmt(phstmt, SQL_UNBIND, FALSE);
    if (rc != SQL_SUCCESS) {
        addError(&pStmt->pErrorList, "HY000",
                 "call_show_table: Fail to clean up the column binding", 0,
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
                 "call_show_table: Fail to bind column for SHOW TABLES "
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

    return SQL_SUCCESS;
}

//
// Helper function to retrieve intermediate result set for SHOW COLUMNS
//
SQLRETURN RsMetadataServerAPIHelper::call_show_column(
    SQLHSTMT phstmt, const std::string &catalog, const std::string &schema,
    const std::string &table, const std::string &column,
    std::vector<SHOWCOLUMNSResult> &intermediateRS) {
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    SQLRETURN rc = SQL_SUCCESS;
    std::string sql = {0};

    // Input parameter check
    if (!checkNameIsExactName(catalog)) {
        addError(&pStmt->pErrorList, "HY000",
                 "call_show_column: catalog name should be exact name", 0,
                 NULL);
        return SQL_ERROR;
    }

    if (!checkNameIsExactName(schema)) {
        addError(&pStmt->pErrorList, "HY000",
                 "call_show_column: schema name should be exact name", 0, NULL);
        return SQL_ERROR;
    }

    if (!checkNameIsExactName(table)) {
        addError(&pStmt->pErrorList, "HY000",
                 "call_show_column: table name should be exact name", 0, NULL);
        return SQL_ERROR;
    }

    // Build query for SHOW COLUMNS
    if (checkNameIsNotPattern(column)) {
        sql = "SHOW COLUMNS FROM TABLE " + catalog + "." + schema + "." +
              table + ";";
    } else {
        sql = "SHOW COLUMNS FROM TABLE " + catalog + "." + schema + "." +
              table + " LIKE '" + column + "';";
    }

    // Execute Server API call
    setCatalogQueryBuf(pStmt, (char *)sql.c_str());
    // TODO: Support for prepare not ready yet
    rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)sql.c_str(), SQL_NTS,
                                     TRUE, FALSE, FALSE, TRUE);
    resetCatalogQueryFlag(pStmt);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        addError(&pStmt->pErrorList, "HY000",
                 "call_show_column: Fail to execute SHOW COLUMNS ... ", 0,
                 NULL);
        return rc;
    }

    // Clean up the column binding
    rc = RS_STMT_INFO::RS_SQLFreeStmt(phstmt, SQL_UNBIND, FALSE);
    if (rc != SQL_SUCCESS) {
        addError(&pStmt->pErrorList, "HY000",
                 "call_show_column: Fail to clean up the column binding", 0,
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
                 "call_show_column: Fail to bind column for SHOW COLUMNS "
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

    return SQL_SUCCESS;
}