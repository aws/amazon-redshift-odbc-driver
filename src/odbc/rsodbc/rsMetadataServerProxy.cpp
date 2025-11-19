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
#include "rserror.h"

//
// Helper function to return intermediate result set for SQLTables special call
// to retrieve a list of catalog
//
SQLRETURN RsMetadataServerProxy::sqlCatalogs(
    SQLHSTMT phstmt, std::vector<std::string> &catalogs,
    bool isSingleDatabaseMetaData) {

    // Validate statement handler
    if (!VALID_HSTMT(phstmt)) {
        RS_LOG_ERROR("sqlCatalogs", "Statement handle allocation failed");
        return SQL_INVALID_HANDLE;
    }

    // Return current connected database name
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    if (isSingleDatabaseMetaData) {
        catalogs.push_back(getDatabase(pStmt));
        RS_LOG_TRACE("sqlCatalogs", "Total number of return rows: %d", catalogs.size());
        return SQL_SUCCESS;
    }

    // Get catalog list
    SQLRETURN rc = RsMetadataServerProxyHelpers::ShowDatabasesHelper(
                       phstmt, RsMetadataAPIHelper::SQL_EMPTY, catalogs,
                       isSingleDatabaseMetaData)
                       .execute();
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlCatalogs", "Error in ShowDatabasesHelper");
        return rc;
    }

    RS_LOG_TRACE("sqlCatalogs", "Total number of return rows: %d", catalogs.size());
    return rc;
}

//
// Helper function to return intermediate result set for SQLTables special call
// to retrieve a list of schema
//
SQLRETURN RsMetadataServerProxy::sqlSchemas(
    SQLHSTMT phstmt, std::vector<SHOWSCHEMASResult> &intermediateRS,
    bool isSingleDatabaseMetaData) {

    // Validate statement handler
    if (!VALID_HSTMT(phstmt)) {
        RS_LOG_ERROR("sqlSchemas", "Statement handle allocation failed");
        return SQL_INVALID_HANDLE;
    }

    // Define variable to hold the result from Show command helper
    std::vector<std::string> catalogs;

    // Get catalog list
    SQLRETURN rc = RsMetadataServerProxyHelpers::ShowDatabasesHelper(
                       phstmt, RsMetadataAPIHelper::SQL_EMPTY, catalogs,
                       isSingleDatabaseMetaData)
                       .execute();
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlSchemas", "Error in ShowDatabasesHelper");
        return rc;
    }

    for (const auto& curCatalog : catalogs) {
        // Get schema list
        rc = RsMetadataServerProxyHelpers::ShowSchemasHelper(
                 phstmt, curCatalog, RsMetadataAPIHelper::SQL_EMPTY,
                 intermediateRS)
                 .execute();
        if (!SQL_SUCCEEDED(rc)) {
            RS_LOG_ERROR("sqlSchemas", "Error in ShowSchemasHelper");
            return rc;
        }
    }
    catalogs.clear();

    RS_LOG_TRACE("sqlSchemas", "Total number of return rows: %d", intermediateRS.size());
    return rc;
}

//
// Helper function to return intermediate result set for SQLTables
//
SQLRETURN RsMetadataServerProxy::sqlTables(
    SQLHSTMT phstmt, const std::string &catalogName,
    const std::string &schemaName, const std::string &tableName,
    std::vector<SHOWTABLESResult> &intermediateRS,
    bool isSingleDatabaseMetaData) {

    RS_LOG_TRACE(
        "sqlTables",
        "catalogName = \"%s\", schemaName = \"%s\", tableName = \"%s\"",
        catalogName.c_str(), schemaName.c_str(), tableName.c_str());

    // Validate statement handler
    if (!VALID_HSTMT(phstmt)) {
        RS_LOG_ERROR("sqlTables", "Statement handle allocation failed");
        return SQL_INVALID_HANDLE;
    }

    // Input length validation
    std::vector<std::pair<std::string, std::string>> validations;
    validations.push_back(std::make_pair("catalog", catalogName));
    validations.push_back(std::make_pair("schema", schemaName));
    validations.push_back(std::make_pair("table", tableName));
    if (!SQL_SUCCEEDED(validateNameLengths(validations))) {
        RS_LOG_ERROR("sqlTables", "Invalid input parameters");
        return SQL_ERROR;
    }

    // Define variable to hold the result from Show command helper
    std::vector<std::string> catalogs;
    std::vector<SHOWSCHEMASResult> schemas;

    // Get catalog list
    SQLRETURN rc = RsMetadataServerProxyHelpers::ShowDatabasesHelper(
                       phstmt, catalogName, catalogs, isSingleDatabaseMetaData)
                       .execute();
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlTables", "Error in ShowDatabasesHelper");
        return rc;
    }

    for (const auto& curCatalog : catalogs) {
        // Get schema list
        rc = RsMetadataServerProxyHelpers::ShowSchemasHelper(
                 phstmt, curCatalog, schemaName, schemas)
                 .execute();
        if (!SQL_SUCCEEDED(rc)) {
            RS_LOG_ERROR("sqlTables", "Error in ShowSchemasHelper");
            return rc;
        }
        for (const auto& curSchema : schemas) {
            // Get table list
            rc = RsMetadataServerProxyHelpers::ShowTablesHelper(
                     phstmt, curCatalog, char2String(curSchema.schema_name),
                     tableName, intermediateRS)
                     .execute();
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR("sqlTables", "Error in ShowTablesHelper");
                return rc;
            }
        }
        schemas.clear();
    }
    catalogs.clear();

    RS_LOG_TRACE("sqlTables", "Total number of return rows: %d", intermediateRS.size());
    return rc;
}

//
// Helper function to return intermediate result set for SQLColumns
//
SQLRETURN RsMetadataServerProxy::sqlColumns(
    SQLHSTMT phstmt, const std::string &catalogName,
    const std::string &schemaName, const std::string &tableName,
    const std::string &columnName,
    std::vector<SHOWCOLUMNSResult> &intermediateRS,
    bool isSingleDatabaseMetaData) {

    RS_LOG_TRACE("sqlColumns",
                 "catalogName = \"%s\", schemaName = \"%s\", tableName = "
                 "\"%s\", columnName = \"%s\"",
                 catalogName.c_str(), schemaName.c_str(), tableName.c_str(),
                 columnName.c_str());

    // Validate statement handler
    if (!VALID_HSTMT(phstmt)) {
        RS_LOG_ERROR("sqlColumns", "Statement handle allocation failed");
        return SQL_INVALID_HANDLE;
    }

    // Input length validation
    std::vector<std::pair<std::string, std::string>> validations;
    validations.push_back(std::make_pair("catalog", catalogName));
    validations.push_back(std::make_pair("schema", schemaName));
    validations.push_back(std::make_pair("table", tableName));
    validations.push_back(std::make_pair("column", columnName));
    if (!SQL_SUCCEEDED(validateNameLengths(validations))) {
        RS_LOG_ERROR("sqlColumns", "Invalid input parameters");
        return SQL_ERROR;
    }

    // Define variable to hold the result from Show command helper
    std::vector<std::string> catalogs;
    std::vector<SHOWSCHEMASResult> schemas;
    std::vector<SHOWTABLESResult> tables;

    // Get catalog list
    SQLRETURN rc = RsMetadataServerProxyHelpers::ShowDatabasesHelper(
                       phstmt, catalogName, catalogs, isSingleDatabaseMetaData)
                       .execute();
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlColumns", "Error in ShowDatabasesHelper");
        return rc;
    }

    for (const auto& curCatalog : catalogs) {
        // Get schema list
        rc = RsMetadataServerProxyHelpers::ShowSchemasHelper(
                 phstmt, curCatalog, schemaName, schemas)
                 .execute();
        if (!SQL_SUCCEEDED(rc)) {
            RS_LOG_ERROR("sqlColumns", "Error in ShowSchemasHelper");
            return rc;
        }
        for (const auto& curSchema : schemas) {
            // Get table list
            rc = RsMetadataServerProxyHelpers::ShowTablesHelper(
                     phstmt, curCatalog, char2String(curSchema.schema_name),
                     tableName, tables)
                     .execute();
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR("sqlColumns", "Error in ShowTablesHelper");
                return rc;
            }
            for (const auto& curTable : tables) {
                // Get column list
                rc = RsMetadataServerProxyHelpers::ShowColumnsHelper(
                         phstmt, curCatalog, char2String(curSchema.schema_name),
                         char2String(curTable.table_name), columnName,
                         intermediateRS)
                         .execute();
                if (!SQL_SUCCEEDED(rc)) {
                    RS_LOG_ERROR("sqlColumns", "Error in ShowColumnsHelper");
                    return rc;
                }
            }
            tables.clear();
        }
        schemas.clear();
    }
    catalogs.clear();

    RS_LOG_TRACE("sqlColumns", "Total number of return rows: %d", intermediateRS.size());
    return rc;
}

//
// Helper function to return intermediate result set for SQLPrimaryKeys
//
SQLRETURN RsMetadataServerProxy::sqlPrimaryKeys(
    SQLHSTMT phstmt,
    const std::string &catalogName,
    const std::string &schemaName,
    const std::string &tableName,
    std::vector<SHOWCONSTRAINTSPRIMARYKEYSResult> &intermediateRS,
    bool isSingleDatabaseMetaData) {

    RS_LOG_TRACE(
        "sqlPrimaryKeys",
        "catalogName = \"%s\", schemaName = \"%s\", tableName = \"%s\"",
        catalogName.c_str(), schemaName.c_str(), tableName.c_str());

    // Validate statement handler
    if(!VALID_HSTMT(phstmt)) {
        RS_LOG_ERROR("sqlPrimaryKeys", "Statement handle allocation failed");
        return SQL_INVALID_HANDLE;
    }

    std::vector<std::pair<std::string, std::string>> validations;
    validations.push_back(std::make_pair("catalog", catalogName));
    validations.push_back(std::make_pair("schema", schemaName));
    validations.push_back(std::make_pair("table", tableName));
    if (!SQL_SUCCEEDED(validateNameLengths(validations))) {
        RS_LOG_ERROR("sqlPrimaryKeys", "Invalid input parameters");
        return SQL_ERROR;
    }

    // Define variable to hold the result from Show command helper
    std::vector<std::string> catalogs;
    std::vector<SHOWSCHEMASResult> schemas;
    std::vector<SHOWTABLESResult> tables;

    // Get catalog list
    SQLRETURN rc = SQL_SUCCESS;
    if (catalogName.empty()) {
        rc = RsMetadataServerProxyHelpers::ShowDatabasesHelper(
                 phstmt, catalogName, catalogs, isSingleDatabaseMetaData)
                 .execute();
        if (!SQL_SUCCEEDED(rc)) {
            RS_LOG_ERROR("sqlPrimaryKeys", "Error in ShowDatabasesHelper");
            return rc;
        }
    } else {
        catalogs.push_back(catalogName);
    }

    for (const auto& curCatalog : catalogs) {
        // Get schema list
        if (schemaName.empty()) {
            rc = RsMetadataServerProxyHelpers::ShowSchemasHelper(
                     phstmt, curCatalog, schemaName, schemas)
                     .execute();
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR("sqlPrimaryKeys", "Error in ShowSchemasHelper");
                return rc;
            }
        } else {
            SHOWSCHEMASResult schemaResult;
            schemaResult.schema_name_Len = std::snprintf(
                reinterpret_cast<char*>(schemaResult.schema_name),
                NAMEDATALEN,
                "%.*s",
                static_cast<int>(schemaName.size()),
                schemaName.data()
            );
            schemas.push_back(schemaResult);
        }

        for (const auto& curSchema : schemas) {
            // Get table list
            if (tableName.empty()) {
                rc = RsMetadataServerProxyHelpers::ShowTablesHelper(phstmt, curCatalog,
                                    char2String(curSchema.schema_name), tableName,
                                    tables).execute();
                if (!SQL_SUCCEEDED(rc)) {
                    RS_LOG_ERROR("sqlPrimaryKeys", "Error in ShowTablesHelper");
                    return rc;
                }
            } else {
                SHOWTABLESResult tableResult;
                tableResult.table_name_Len = std::snprintf(
                    reinterpret_cast<char*>(tableResult.table_name),
                    NAMEDATALEN,
                    "%.*s",
                    static_cast<int>(tableName.size()),
                    tableName.data()
                );
                tables.push_back(tableResult);
            }
            for (const auto& curTable : tables) {
                // Get primary key list
                rc = RsMetadataServerProxyHelpers::ShowConstraintsPkHelper(
                         phstmt, curCatalog, char2String(curSchema.schema_name),
                         char2String(curTable.table_name), intermediateRS).execute();
                if (!SQL_SUCCEEDED(rc)) {
                    RS_LOG_ERROR("sqlPrimaryKeys", "Error in ShowConstraintsPkHelper");
                    return rc;
                }
            }
            tables.clear();
        }
        schemas.clear();
    }
    catalogs.clear();

    RS_LOG_TRACE("sqlPrimaryKeys", "Total number of return rows: %d", intermediateRS.size());
    return rc;
}

//
// Helper function to return intermediate result set for SQLForeignKeys
//
SQLRETURN RsMetadataServerProxy::sqlForeignKeys(
    SQLHSTMT phstmt,
    const std::string &pkCatalogName,
    const std::string &pkSchemaName,
    const std::string &pkTableName,
    const std::string &fkCatalogName,
    const std::string &fkSchemaName,
    const std::string &fkTableName,
    std::vector<SHOWCONSTRAINTSFOREIGNKEYSResult> &intermediateRS,
    bool isSingleDatabaseMetaData) {

    RS_LOG_TRACE(
        "sqlForeignKeys",
        "pkCatalogName = \"%s\", pkSchemaName = \"%s\", pkTableName = \"%s\", "
        "fkCatalogName = \"%s\", fkSchemaName = \"%s\", fkTableName = \"%s\"",
        pkCatalogName.c_str(), pkSchemaName.c_str(), pkTableName.c_str(),
        fkCatalogName.c_str(), fkSchemaName.c_str(), fkTableName.c_str());

    // Define variable to hold the result from Show command helper
    if(!VALID_HSTMT(phstmt)) {
        RS_LOG_ERROR("sqlForeignKeys", "Statement handle allocation failed");
        return SQL_INVALID_HANDLE;
    }

    // Input length validation
    std::vector<std::pair<std::string, std::string>> validations;
    validations.push_back(std::make_pair("pkCatalogName", pkCatalogName));
    validations.push_back(std::make_pair("pkSchemaName", pkSchemaName));
    validations.push_back(std::make_pair("pkTableName", pkTableName));
    validations.push_back(std::make_pair("fkCatalogName", fkCatalogName));
    validations.push_back(std::make_pair("fkSchemaName", fkSchemaName));
    validations.push_back(std::make_pair("fkTableName", fkTableName));
    if (!SQL_SUCCEEDED(validateNameLengths(validations))) {
        RS_LOG_ERROR("sqlForeignKeys", "Invalid input parameters");
        return SQL_ERROR;
    }

    SQLRETURN rc = SQL_SUCCESS;
    
    if (!fkTableName.empty()) {
        // Case 1: FK table specified (handles both imported keys and specific relationships)
        std::vector<SHOWCONSTRAINTSFOREIGNKEYSResult> fkResults;
        
        // Get all foreign keys for the FK table (imported keys)
        rc = processKeysCase(phstmt, false, fkCatalogName, fkSchemaName,
                                    fkTableName, fkResults, isSingleDatabaseMetaData);
        if (!SQL_SUCCEEDED(rc)) {
            RS_LOG_ERROR("sqlForeignKeys", "Error in processKeysCase");
            return rc;
        }

        // Only filter if PK table is specified
        if (!pkTableName.empty()) {
            for (const auto& fkResult : fkResults) {
                if (matchesConstraints(fkResult,
                                    pkCatalogName, pkSchemaName, pkTableName)) {
                    intermediateRS.push_back(fkResult);
                }
            }
        } else {
            // No filtering needed if PK table is not specified
            intermediateRS = std::move(fkResults);
        }
    } else if (!pkTableName.empty() && fkTableName.empty()) {
        // Case 2: Only PK table specified (exported keys)
        rc = processKeysCase(phstmt, true, pkCatalogName, pkSchemaName,
                             pkTableName, intermediateRS, isSingleDatabaseMetaData);
        if (!SQL_SUCCEEDED(rc)) {
            RS_LOG_ERROR("sqlForeignKeys", "Error in processKeysCase");
            return rc;
        }
    } else {
        RS_LOG_ERROR("sqlForeignKeys", "Both PK and FK table names are empty");
        return SQL_ERROR;
    }

    RS_LOG_TRACE("sqlForeignKeys", "Total number of return rows: %d", intermediateRS.size());
    return rc;
}

bool RsMetadataServerProxy::matchesConstraints(
    const SHOWCONSTRAINTSFOREIGNKEYSResult& result,
    const std::string &pkCatalog,
    const std::string &pkSchema,
    const std::string &pkTable) {
    
    auto matches = [](const SQLCHAR* value, const std::string& expected) -> bool {
        if (expected.empty()) return true;
        if (!value) return false;
        return strncmp(reinterpret_cast<const char*>(value), expected.c_str(), NAMEDATALEN) == 0;
    };

    return matches(result.pk_table_cat, pkCatalog) &&
           matches(result.pk_table_schem, pkSchema) &&
           matches(result.pk_table_name, pkTable);
}

SQLRETURN RsMetadataServerProxy::processKeysCase(
    SQLHSTMT phstmt,
    bool isExportedKeys,
    const std::string& catalogName,
    const std::string& schemaName,
    const std::string& tableName,
    std::vector<SHOWCONSTRAINTSFOREIGNKEYSResult>& intermediateRS,
    bool isSingleDatabaseMetaData) {

    SQLRETURN rc = SQL_SUCCESS;

    // Define variable to hold the result from Show command helper
    std::vector<std::string> catalogs;
    std::vector<SHOWSCHEMASResult> schemas;
    std::vector<SHOWTABLESResult> tables;

    // Get catalog list
    if (catalogName.empty()) {
        rc = RsMetadataServerProxyHelpers::ShowDatabasesHelper(
                 phstmt, catalogName, catalogs, isSingleDatabaseMetaData)
                 .execute();
        if (!SQL_SUCCEEDED(rc)) {
            RS_LOG_ERROR("processKeysCase", "Error in ShowDatabasesHelper");
            return rc;
        }
    } else {
        catalogs.push_back(catalogName);
    }

    for (const auto& curCatalog : catalogs) {
        // Get schema list
        if (schemaName.empty()) {
            rc = RsMetadataServerProxyHelpers::ShowSchemasHelper(
                     phstmt, curCatalog, schemaName, schemas)
                     .execute();
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR("processKeysCase", "Error in ShowSchemasHelper");
                return rc;
            }
        } else {
            SHOWSCHEMASResult schemaResult;
            schemaResult.schema_name_Len = std::snprintf(
                reinterpret_cast<char*>(schemaResult.schema_name),
                NAMEDATALEN,
                "%.*s",
                static_cast<int>(schemaName.size()),
                schemaName.data()
            );
            schemas.push_back(schemaResult);
        }

        for (const auto& curSchema : schemas) {
            // Get table list
            if (tableName.empty()) {
                rc = RsMetadataServerProxyHelpers::ShowTablesHelper(phstmt, curCatalog,
                                    char2String(curSchema.schema_name), tableName,
                                    tables).execute();
                if (!SQL_SUCCEEDED(rc)) {
                    RS_LOG_ERROR("processKeysCase", "Error in ShowTablesHelper");
                    return rc;
                }
            } else {
                SHOWTABLESResult tableResult;
                tableResult.table_name_Len = std::snprintf(
                    reinterpret_cast<char*>(tableResult.table_name),
                    NAMEDATALEN,
                    "%.*s",
                    static_cast<int>(tableName.size()),
                    tableName.data()
                );
                tables.push_back(tableResult);
            }
            for (const auto& curTable : tables) {
                // Get foreign key list
                rc = RsMetadataServerProxyHelpers::ShowConstraintsFkHelper(phstmt, curCatalog,
                                         char2String(curSchema.schema_name),
                                         char2String(curTable.table_name),
                                         isExportedKeys, intermediateRS).execute();
                if (!SQL_SUCCEEDED(rc)) {
                    const char* keyType = isExportedKeys ? "exported" : "imported";
                    RS_LOG_ERROR("processKeysCase",
                                "Error in ShowConstraintsFkHelper for %s keys", keyType);
                    return rc;
                }
            }
            tables.clear();
        }
        schemas.clear();
    }
    catalogs.clear();
    return rc;
}

//
// Helper function to return intermediate result set for SQLSpecialColumns
//
SQLRETURN RsMetadataServerProxy::sqlSpecialColumns(
    SQLHSTMT phstmt,
    const std::string &catalogName,
    const std::string &schemaName,
    const std::string &tableName,
    std::vector<SHOWCOLUMNSResult> &intermediateRS,
    bool isSingleDatabaseMetaData) {

    RS_LOG_TRACE(
        "sqlSpecialColumns",
        "catalogName = \"%s\", schemaName = \"%s\", tableName = \"%s\"",
        catalogName.c_str(), schemaName.c_str(), tableName.c_str());

    // Validate statement handler
    if (!VALID_HSTMT(phstmt)) {
        RS_LOG_ERROR("sqlSpecialColumns", "Statement handle allocation failed");
        return SQL_INVALID_HANDLE;
    }

    // Input length validation
    std::vector<std::pair<std::string, std::string>> validations;
    validations.push_back(std::make_pair("catalog", catalogName));
    validations.push_back(std::make_pair("schema", schemaName));
    validations.push_back(std::make_pair("table", tableName));
    if (!SQL_SUCCEEDED(validateNameLengths(validations))) {
        RS_LOG_ERROR("sqlSpecialColumns", "Invalid input parameters");
        return SQL_ERROR;
    }

    // Define variable to hold the result from Show command helper
    std::vector<std::string> catalogs;
    std::vector<SHOWSCHEMASResult> schemas;
    std::vector<SHOWTABLESResult> tables;

    // Get catalog list
    SQLRETURN rc = RsMetadataServerProxyHelpers::ShowDatabasesHelper(
                       phstmt, catalogName, catalogs, isSingleDatabaseMetaData)
                       .execute();
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlSpecialColumns", "Error in ShowDatabasesHelper");
        return rc;
    }

    for (const auto& curCatalog : catalogs) {
        // Get schema list
        if (schemaName.empty()) {
            rc = RsMetadataServerProxyHelpers::ShowSchemasHelper(
                     phstmt, curCatalog, schemaName, schemas)
                     .execute();
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR("sqlSpecialColumns", "Error in ShowSchemasHelper");
                return rc;
            }
        } else {
            SHOWSCHEMASResult schemaResult;
            schemaResult.schema_name_Len = std::snprintf(
                reinterpret_cast<char*>(schemaResult.schema_name),
                NAMEDATALEN,
                "%.*s",
                static_cast<int>(schemaName.size()),
                schemaName.data()
            );
            schemas.push_back(schemaResult);
        }

        for (const auto& curSchema : schemas) {
            // Get table list
            if (tableName.empty()) {
                rc = RsMetadataServerProxyHelpers::ShowTablesHelper(
                         phstmt, curCatalog, char2String(curSchema.schema_name),
                         tableName, tables)
                         .execute();
                if (!SQL_SUCCEEDED(rc)) {
                    RS_LOG_ERROR("sqlSpecialColumns", "Error in ShowTablesHelper");
                    return rc;
                }
            } else {
                SHOWTABLESResult tableResult;
                tableResult.table_name_Len = std::snprintf(
                    reinterpret_cast<char*>(tableResult.table_name),
                    NAMEDATALEN,
                    "%.*s",
                    static_cast<int>(tableName.size()),
                    tableName.data()
                );
                tables.push_back(tableResult);
            }

            for (const auto& curTable : tables) {
                // Step 1: Get primary key columns for the current table
                std::vector<SHOWCONSTRAINTSPRIMARYKEYSResult> pkResults;
                rc = RsMetadataServerProxyHelpers::ShowConstraintsPkHelper(phstmt, curCatalog,
                                    char2String(curSchema.schema_name),
                                    char2String(curTable.table_name), pkResults).execute();
                if (!SQL_SUCCEEDED(rc)) {
                    RS_LOG_ERROR("sqlSpecialColumns", 
                               "Error in ShowConstraintsPkHelper");
                    return rc;
                }

                // Step 2: Check if primary key columns result is not empty
                if (!pkResults.empty()) {
                    // Create a set of primary key column names for efficient lookup
                    std::set<std::string> pkColumnNames;
                    for (const auto& pk : pkResults) {
                        pkColumnNames.insert(char2String(pk.column_name));
                    }

                    // Step 3: Call SHOW COLUMNS once for the entire table
                    std::vector<SHOWCOLUMNSResult> allColumns;
                    rc = RsMetadataServerProxyHelpers::ShowColumnsHelper(phstmt, curCatalog,
                                           char2String(curSchema.schema_name),
                                           char2String(curTable.table_name),
                                           RsMetadataAPIHelper::SQL_EMPTY,
                                           allColumns).execute();

                    if (!SQL_SUCCEEDED(rc)) {
                        RS_LOG_ERROR("sqlSpecialColumns", 
                                   "Error in ShowColumnsHelper");
                        return rc;
                    }

                    // Step 4: Filter the result from SHOW COLUMNS based on primary key columns
                    for (const auto& column : allColumns) {
                        std::string columnName = char2String(column.column_name);
                        if (pkColumnNames.find(columnName) != pkColumnNames.end()) {
                            intermediateRS.push_back(column);
                        }
                    }
                }
            }
            tables.clear();
        }
        schemas.clear();
    }
    catalogs.clear();

    RS_LOG_TRACE("sqlSpecialColumns", "Total number of return rows: %d", intermediateRS.size());
    return rc;
}

//
// Helper function to return intermediate result set for SQLColumnPrivileges
//
SQLRETURN RsMetadataServerProxy::sqlColumnPrivileges(
    SQLHSTMT phstmt,
    const std::string &catalogName,
    const std::string &schemaName,
    const std::string &tableName,
    const std::string &columnName,
    std::vector<SHOWGRANTSCOLUMNResult> &intermediateRS,
    bool isSingleDatabaseMetaData) {

    RS_LOG_TRACE(
        "sqlColumnPrivileges",
        "catalogName = \"%s\", schemaName = \"%s\", tableName = \"%s\", columnName = \"%s\"",
        catalogName.c_str(), schemaName.c_str(), tableName.c_str(), columnName.c_str());

    // Validate statement handler
    if (!VALID_HSTMT(phstmt)) {
        RS_LOG_ERROR("sqlColumnPrivileges", "Invalid statement handle");
        return SQL_INVALID_HANDLE;
    }

    // Input length validation
    std::vector<std::pair<std::string, std::string>> validations;
    validations.push_back(std::make_pair("catalog", catalogName));
    validations.push_back(std::make_pair("schema", schemaName));
    validations.push_back(std::make_pair("table", tableName));
    validations.push_back(std::make_pair("column", columnName));
    if (!SQL_SUCCEEDED(validateNameLengths(validations))) {
        RS_LOG_ERROR("sqlColumnPrivileges", "Invalid input parameters");
        return SQL_ERROR;
    }

    // Define variable to hold the result from Show command helper
    std::vector<std::string> catalogs;
    std::vector<SHOWSCHEMASResult> schemas;
    std::vector<SHOWTABLESResult> tables;

    // Get catalog list
    SQLRETURN rc = RsMetadataServerProxyHelpers::ShowDatabasesHelper(
                       phstmt, catalogName, catalogs, isSingleDatabaseMetaData)
                       .execute();
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlColumnPrivileges", "Error in ShowDatabasesHelper");
        return rc;
    }

    for (const auto& curCatalog : catalogs) {
        // Get schema list
        if (schemaName.empty()) {
            rc = RsMetadataServerProxyHelpers::ShowSchemasHelper(
                     phstmt, curCatalog, schemaName, schemas)
                     .execute();
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR("sqlColumnPrivileges", "Error in ShowSchemasHelper");
                return rc;
            }
        } else {
            SHOWSCHEMASResult schemaResult;
            schemaResult.schema_name_Len = std::snprintf(
                reinterpret_cast<char*>(schemaResult.schema_name),
                NAMEDATALEN,
                "%.*s",
                static_cast<int>(schemaName.size()),
                schemaName.data()
            );
            schemas.push_back(schemaResult);
        }

        for (const auto& curSchema : schemas) {
            // Get table list
            if (tableName.empty()) {
                rc = RsMetadataServerProxyHelpers::ShowTablesHelper(phstmt, curCatalog,
                                      char2String(curSchema.schema_name),
                                      tableName, tables).execute();
                if (!SQL_SUCCEEDED(rc)) {
                    RS_LOG_ERROR("sqlColumnPrivileges", "Error in ShowTablesHelper");
                    return rc;
                }
            } else {
                SHOWTABLESResult tableResult;
                tableResult.table_name_Len = std::snprintf(
                    reinterpret_cast<char*>(tableResult.table_name),
                    NAMEDATALEN,
                    "%.*s",
                    static_cast<int>(tableName.size()),
                    tableName.data()
                );
                tables.push_back(tableResult);
            }

            for (const auto& curTable : tables) {
                // Get column privileges
                rc = RsMetadataServerProxyHelpers::ShowGrantsColumnHelper(phstmt, curCatalog,
                                    char2String(curSchema.schema_name),
                                    char2String(curTable.table_name),
                                    columnName, intermediateRS).execute();
                if (!SQL_SUCCEEDED(rc)) {
                    RS_LOG_ERROR("sqlColumnPrivileges", "Error in ShowGrantsColumnHelper");
                    return rc;
                }
            }
            tables.clear();
        }
        schemas.clear();
    }
    catalogs.clear();

    RS_LOG_TRACE("sqlColumnPrivileges", "Total number of return rows: %d", intermediateRS.size());
    return rc;
}

//
// Helper function to return intermediate result set for SQLTablePrivileges
//
SQLRETURN RsMetadataServerProxy::sqlTablePrivileges(
    SQLHSTMT phstmt,
    const std::string &catalogName,
    const std::string &schemaName,
    const std::string &tableName,
    std::vector<SHOWGRANTSTABLEResult> &intermediateRS,
    bool isSingleDatabaseMetaData) {

    RS_LOG_TRACE(
        "sqlTablePrivileges",
        "catalogName = \"%s\", schemaName = \"%s\", tableName = \"%s\"",
        catalogName.c_str(), schemaName.c_str(), tableName.c_str());

    // Validate statement handler
    if (!VALID_HSTMT(phstmt)) {
        RS_LOG_ERROR("sqlTablePrivileges", "Invalid statement handle");
        return SQL_INVALID_HANDLE;
    }

    // Input length validation
    std::vector<std::pair<std::string, std::string>> validations;
    validations.push_back(std::make_pair("catalog", catalogName));
    validations.push_back(std::make_pair("schema", schemaName));
    validations.push_back(std::make_pair("table", tableName));
    if (!SQL_SUCCEEDED(validateNameLengths(validations))) {
        RS_LOG_ERROR("sqlTablePrivileges", "Invalid input parameters");
        return SQL_ERROR;
    }

    // Define variable to hold the result from Show command helper
    std::vector<std::string> catalogs;
    std::vector<SHOWSCHEMASResult> schemas;
    std::vector<SHOWTABLESResult> tables;

    // Get catalog list
    SQLRETURN rc = RsMetadataServerProxyHelpers::ShowDatabasesHelper(
                       phstmt, catalogName, catalogs, isSingleDatabaseMetaData)
                       .execute();
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlTablePrivileges", "Error in ShowDatabasesHelper");
        return rc;
    }

    for (const auto& curCatalog : catalogs) {
        // Get schema list
        rc = RsMetadataServerProxyHelpers::ShowSchemasHelper(
                 phstmt, curCatalog, schemaName, schemas)
                 .execute();
        if (!SQL_SUCCEEDED(rc)) {
            RS_LOG_ERROR("sqlTablePrivileges", "Error in ShowSchemasHelper");
            return rc;
        }
        for (const auto& curSchema : schemas) {
            // Get table list
            rc = RsMetadataServerProxyHelpers::ShowTablesHelper(
                     phstmt, curCatalog, char2String(curSchema.schema_name),
                     tableName, tables)
                     .execute();
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR("sqlTablePrivileges", "Error in ShowTablesHelper");
                return rc;
            }
            for (const auto& curTable : tables) {
                rc = RsMetadataServerProxyHelpers::ShowGrantsTableHelper(phstmt, curCatalog,
                                  char2String(curSchema.schema_name),
                                    char2String(curTable.table_name), intermediateRS).execute();
                if (!SQL_SUCCEEDED(rc)) {
                    RS_LOG_ERROR("sqlTablePrivileges", "Error in ShowGrantsTableHelper");
                    return rc;
                }
            }
            tables.clear();
        }
        schemas.clear();
    }
    catalogs.clear();

    RS_LOG_TRACE("sqlTablePrivileges", "Total number of return rows: %d", intermediateRS.size());
    return rc;
}

//
// Helper function to return intermediate result set for SQLProcedures
//
SQLRETURN RsMetadataServerProxy::sqlProcedures(
    SQLHSTMT phstmt,
    const std::string &catalogName,
    const std::string &schemaName,
    const std::string &procName,
    std::vector<SHOWPROCEDURESFUNCTIONSResult> &intermediateRS,
    bool isSingleDatabaseMetaData) {

    RS_LOG_TRACE(
        "sqlProcedures",
        "catalogName = \"%s\", schemaName = \"%s\", procName = \"%s\"",
        catalogName.c_str(), schemaName.c_str(), procName.c_str());

    // Validate statement handler
    if (!VALID_HSTMT(phstmt)) {
        RS_LOG_ERROR("sqlProcedures", "Invalid statement handle");
        return SQL_INVALID_HANDLE;
    }

    // Input length validation
    std::vector<std::pair<std::string, std::string>> validations;
    validations.push_back(std::make_pair("catalog", catalogName));
    validations.push_back(std::make_pair("schema", schemaName));
    validations.push_back(std::make_pair("procedure/function", procName));
    if (!SQL_SUCCEEDED(validateNameLengths(validations))) {
        RS_LOG_ERROR("sqlProcedures", "Invalid input parameters");
        return SQL_ERROR;
    }

    // Define variable to hold the result from Show command helper
    std::vector<std::string> catalogs;
    std::vector<SHOWSCHEMASResult> schemas;

    // Get catalog list
    SQLRETURN rc = SQL_SUCCESS;
    if (catalogName.empty()) {
        rc = RsMetadataServerProxyHelpers::ShowDatabasesHelper(
                 phstmt, catalogName, catalogs, isSingleDatabaseMetaData)
                 .execute();
        if (!SQL_SUCCEEDED(rc)) {
            RS_LOG_ERROR("sqlProcedures", "Error in ShowDatabasesHelper");
            return rc;
        }
    } else {
        catalogs.push_back(catalogName);
    }
    for (const auto& curCatalog : catalogs) {
        // Get schema list
        rc = RsMetadataServerProxyHelpers::ShowSchemasHelper(
                 phstmt, curCatalog, schemaName, schemas)
                 .execute();
        if (!SQL_SUCCEEDED(rc)) {
            RS_LOG_ERROR("sqlProcedures", "Error in ShowSchemasHelper");
            return rc;
        }
        for (const auto& curSchema : schemas) {
            // Collect procedures and functions for this catalog/schema
            std::vector<SHOWPROCEDURESFUNCTIONSResult> procedureFunctionResults;

            // Get procedures
            rc = RsMetadataServerProxyHelpers::ShowProceduresFunctionsHelper(
                     phstmt, curCatalog, char2String(curSchema.schema_name),
                     procName, true, procedureFunctionResults).execute();
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR("sqlProcedures", "ShowProceduresFunctionsHelper");
                return rc;
            }

            // Get functions
            rc = RsMetadataServerProxyHelpers::ShowProceduresFunctionsHelper(
                     phstmt, curCatalog, char2String(curSchema.schema_name),
                     procName, false, procedureFunctionResults).execute();
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR("sqlProcedures", "Error executing SHOW FUNCTIONS query");
                return rc;
            }

            // Sort only by procedure/function name within this catalog/schema
            std::sort(procedureFunctionResults.begin(), procedureFunctionResults.end(),
                  [](const SHOWPROCEDURESFUNCTIONSResult& a, const SHOWPROCEDURESFUNCTIONSResult& b) {
                      std::string nameA(reinterpret_cast<const char*>(a.object_name), a.object_name_Len);
                      std::string nameB(reinterpret_cast<const char*>(b.object_name), b.object_name_Len);
                      return _stricmp(nameA.c_str(), nameB.c_str()) < 0;
                  });
            
            // Append to final results
            intermediateRS.insert(intermediateRS.end(), procedureFunctionResults.begin(), procedureFunctionResults.end());
        }
        schemas.clear();
    }
    catalogs.clear();

    RS_LOG_TRACE("sqlProcedures", "Total number of return rows: %d", intermediateRS.size());
    return rc;
}

//
// Helper function to return intermediate result set for SQLProcedureColumns
//
SQLRETURN RsMetadataServerProxy::sqlProcedureColumns(
    SQLHSTMT phstmt, const std::string &catalogName,
    const std::string &schemaName, const std::string &procName,
    const std::string &columnName,
    std::vector<SHOWCOLUMNSResult> &intermediateRS,
    bool isSingleDatabaseMetaData) {

    RS_LOG_TRACE(
        "sqlProcedureColumns",
        "catalogName = \"%s\", schemaName = \"%s\", procName = \"%s\", columnName = \"%s\"",
        catalogName.c_str(), schemaName.c_str(), procName.c_str(), columnName.c_str());

    // Validate statement handler
    if (!VALID_HSTMT(phstmt)) {
        RS_LOG_ERROR("sqlProcedureColumns", "Statement handle allocation failed");
        return SQL_INVALID_HANDLE;
    }

    // Input length validation
    std::vector<std::pair<std::string, std::string>> validations;
    validations.push_back(std::make_pair("catalog", catalogName));
    validations.push_back(std::make_pair("schema", schemaName));
    validations.push_back(std::make_pair("procedure/function", procName));
    validations.push_back(std::make_pair("column", columnName));
    if (!SQL_SUCCEEDED(validateNameLengths(validations))) {
        RS_LOG_ERROR("sqlProcedureColumns", "Invalid input parameters");
        return SQL_ERROR;
    }

    // Define variable to hold the result from Show command helper
    std::vector<std::string> catalogs;
    std::vector<SHOWSCHEMASResult> schemas;

    // Get catalog list
    SQLRETURN rc = SQL_SUCCESS;
    if (catalogName.empty()) {
        rc = RsMetadataServerProxyHelpers::ShowDatabasesHelper(
                 phstmt, catalogName, catalogs, isSingleDatabaseMetaData)
                 .execute();
        if (!SQL_SUCCEEDED(rc)) {
            RS_LOG_ERROR("sqlProcedureColumns", "Error in ShowDatabasesHelper");
            return rc;
        }
    } else {
        catalogs.push_back(catalogName);
    }

    for (int i = 0; i < catalogs.size(); i++) {
        // Get schema list
        rc = RsMetadataServerProxyHelpers::ShowSchemasHelper(
                 phstmt, catalogs[i], schemaName, schemas)
                 .execute();
        if (!SQL_SUCCEEDED(rc)) {
            RS_LOG_ERROR("sqlProcedureColumns", "Error in ShowSchemasHelper");
            return rc;
        }
        for (int j = 0; j < schemas.size(); j++) {
            // Collect procedures and functions for this catalog/schema
            std::vector<SHOWPROCEDURESFUNCTIONSResult> procedureFunctionResults;

            // Get procedures
            rc = RsMetadataServerProxyHelpers::ShowProceduresFunctionsHelper(
                     phstmt, catalogs[i], char2String(schemas[j].schema_name),
                     procName, true, procedureFunctionResults).execute();
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR("sqlProcedureColumns", "Error in ShowProceduresFunctionsHelper for procedure");
                return rc;
            }

            // Get functions
            rc = RsMetadataServerProxyHelpers::ShowProceduresFunctionsHelper(
                     phstmt, catalogs[i], char2String(schemas[j].schema_name),
                     procName, false, procedureFunctionResults).execute();
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR("sqlProcedureColumns", "Error in ShowProceduresFunctionsHelper for function");
                return rc;
            }

            // Sort only by procedure/function name within this catalog/schema
            std::sort(procedureFunctionResults.begin(), procedureFunctionResults.end(),
                  [](const SHOWPROCEDURESFUNCTIONSResult& a, const SHOWPROCEDURESFUNCTIONSResult& b) {
                      std::string nameA(reinterpret_cast<const char*>(a.object_name), a.object_name_Len);
                      std::string nameB(reinterpret_cast<const char*>(b.object_name), b.object_name_Len);
                      return _stricmp(nameA.c_str(), nameB.c_str()) < 0;
                  });

            // Get columns for each procedure and function
            for (int k = 0; k < procedureFunctionResults.size(); k++) {
                bool isProcedure = (procedureFunctionResults[k].object_type == SQL_PT_PROCEDURE);
                std::string sqlBase = isProcedure ? RsMetadataAPIHelper::kshowParamsProcQuery : RsMetadataAPIHelper::kshowParamsFuncQuery;
                std::string argumentListStr = char2String(procedureFunctionResults[k].argument_list);
                try {
                    auto [sqlQuery, argumentList] = RsMetadataAPIHelper::createParameterizedQueryString(
                        argumentListStr,
                        sqlBase,
                        columnName
                    );
                    rc = RsMetadataServerProxyHelpers::
                            ShowProcedureFunctionColumnsHelper(
                                phstmt, catalogs[i],
                                char2String(schemas[j].schema_name),
                                char2String(
                                    procedureFunctionResults[k].object_name),
                                columnName,
                                sqlQuery,
                                argumentList,
                                isProcedure,
                                intermediateRS)
                                .execute();
                    if (!SQL_SUCCEEDED(rc)) {
                        RS_LOG_ERROR("sqlProcedureColumns", "Error in ShowProcedureFunctionColumnsHelper");
                        return rc;
                    }
                } catch (const std::invalid_argument& e) {
                    RS_LOG_ERROR("sqlProcedureColumns", "Invalid argument list for procedure/function '%s': %s",
                        char2String(procedureFunctionResults[k].object_name).c_str(), e.what());
                    return SQL_ERROR;
                } catch (...) {
                    RS_LOG_ERROR("sqlProcedureColumns", "Unexpected exception during query preparation or query execution");
                    return SQL_ERROR;
                }
            }
        }
        schemas.clear();
    }
    catalogs.clear();

    RS_LOG_TRACE("sqlProcedureColumns", "Total number of return rows: %d", intermediateRS.size());
    return rc;
}

SQLRETURN RsMetadataServerProxy::validateNameLengths(
    const std::vector<std::pair<std::string, std::string>>& validations) {

    for (const auto& validation : validations) {
        if (!validation.second.empty() && validation.second.size() > NAMEDATALEN) {
            RS_LOG_ERROR("validateNameLengths",
                        "Invalid %s provided: length %zu exceeds maximum allowed length %d",
                        validation.first.c_str(), validation.second.size(), NAMEDATALEN);
            return SQL_ERROR;
        }
    }
    return SQL_SUCCESS;
}
