/*-------------------------------------------------------------------------
 *
 * Copyright(c) 2025, Amazon.com, Inc. or Its Affiliates. All rights reserved.
 *
 * Author: stinghsu
 *-------------------------------------------------------------------------
 */

#include "rsMetadataServerProxyHelper.h"

namespace RsMetadataServerProxyHelpers {
    //-------------------------------------------------------------------------
    // ShowDiscoveryBase Implementation
    //-------------------------------------------------------------------------
    ShowDiscoveryBase::ShowDiscoveryBase(SQLHSTMT stmt, const char *operationName)
        : m_stmt(stmt), m_pStmt((RS_STMT_INFO *)stmt),
            m_operationName(operationName) {}

    ShowDiscoveryBase::ColumnBindingCleanup::ColumnBindingCleanup(SQLHSTMT& stmt) : m_stmt(stmt) {
        cleanup();
    }

    ShowDiscoveryBase::ColumnBindingCleanup::~ColumnBindingCleanup() {
        cleanup();
    }

    void ShowDiscoveryBase::ColumnBindingCleanup::cleanup() {
        auto rc = RS_STMT_INFO::RS_SQLFreeStmt(m_stmt, SQL_UNBIND, FALSE);
        if (!SQL_SUCCEEDED(rc)) {
            RS_LOG_ERROR("ColumnBindingCleanup", 
                "Failed to clean up column binding"); 
        }
    }

    SQLRETURN ShowDiscoveryBase::execute() {
        return SQL_ERROR;
    }

    /**
     * @brief Template method for complete SHOW command execution workflow
     * 
     * Implements the standard ODBC execution pattern:
     * 1. Prepare SQL statement
     * 2. Clean existing parameter bindings
     * 3. Bind input parameters sequentially
     * 4. Execute prepared statement
     * 5. Clean up parameter bindings
     * 
     * @param query SQL query string to execute
     * @param inputParameters Vector of string parameters to bind (in order)
     * @return SQLRETURN Success/failure code from ODBC operations
     * 
     */
    SQLRETURN ShowDiscoveryBase::prepareBindAndExecuteQuery(
        const std::string& query, const std::vector<std::string> &inputParameters) {
        
        // Step 1: Prepare the SQL statement
        if (query.empty()) {
            RS_LOG_ERROR("prepareBindAndExecuteQuery", "Query can't be empty");
            return SQL_ERROR;
        }
        SQLRETURN rc = RsPrepare::RS_SQLPrepare(m_stmt, (SQLCHAR *)query.c_str(), SQL_NTS,
                                    FALSE, FALSE, FALSE, TRUE);
        if (!SQL_SUCCEEDED(rc)) {
            std::string errorDetails = getErrorMessage(m_stmt);
            RS_LOG_ERROR("prepareBindAndExecuteQuery",
                        "Fail to prepare query \"%s\". Details: %s",
                        query.c_str(),
                        errorDetails.c_str()); 
            return rc;
        }

        // Step 2: Clean up any existing parameter bindings before new operation
        // This prevents any conflicts with previous bindings
        rc = RS_STMT_INFO::RS_SQLFreeStmt(m_stmt, SQL_RESET_PARAMS, FALSE);
        if (!SQL_SUCCEEDED(rc)) {
            std::string errorDetails = getErrorMessage(m_stmt);
            RS_LOG_ERROR("prepareBindAndExecuteQuery", 
                "Fail to clean up the parameter binding. Details: %s",
                errorDetails.c_str());
            return rc;
        }

        // Step 3: Bind each input parameter to the statement
        // Parameters are bound in order, with 1-based indexing
        std::vector<SQLLEN> lenIndexList(inputParameters.size(), 0);
        for (int i = 0; i < inputParameters.size(); i++) {
            lenIndexList[i] = inputParameters[i].size();
            rc = RsParameter::RS_SQLBindParameter(
                m_stmt, i + 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                MAX_IDEN_LEN, 0, (SQLCHAR *)inputParameters[i].c_str(),
                inputParameters[i].size(), &lenIndexList[i]);
            if (!SQL_SUCCEEDED(rc)) {
                std::string errorDetails = getErrorMessage(m_stmt);
                RS_LOG_ERROR("prepareBindAndExecuteQuery",
                            "Fail to bind parameter ... Details: %s",
                            errorDetails.c_str());
                return rc;
            }
        }

        // Step 4: Execute the prepared statement with bound parameters
        rc = RsExecute::RS_SQLExecDirect(m_stmt, NULL, 0, FALSE, TRUE, FALSE, TRUE);
        if (!SQL_SUCCEEDED(rc)) {
            std::string errorDetails = getErrorMessage(m_stmt);
            RS_LOG_ERROR("prepareBindAndExecuteQuery",
                        "Fail to execute the prepared statement. Details: %s",
                        errorDetails.c_str());
            return rc;
        }

        // Step 5: Final cleanup - free the parameter bindings
        // This ensures resources are properly released after execution
        rc = RS_STMT_INFO::RS_SQLFreeStmt(m_stmt, SQL_RESET_PARAMS, FALSE);
        if (!SQL_SUCCEEDED(rc)) {
            std::string errorDetails = getErrorMessage(m_stmt);
            RS_LOG_ERROR("prepareBindAndExecuteQuery", 
                "Fail to clean up the parameter binding. Details: %s",
                errorDetails.c_str());
            return rc;
        }
        return rc;
    }

    /**
     * @brief Extracts comprehensive error information from ODBC diagnostic records
     * 
     * Iterates through all available diagnostic records to build a complete
     * error message including SQL state, native error codes, and descriptive text.
     * 
     * @param phstmt ODBC statement handle to extract diagnostics from
     * @return std::string Formatted error message with all diagnostic information
     * 
     */
    std::string ShowDiscoveryBase::getErrorMessage(SQLHSTMT phstmt) {
        SQLCHAR sqlState[6];
        SQLCHAR message[SQL_MAX_MESSAGE_LENGTH];
        SQLINTEGER nativeError;
        SQLSMALLINT textLength;
        SQLSMALLINT i = 1;

        // Build error message with all available diagnostic records
        std::string errorDetails;
        while (RsError::RS_SQLGetDiagRec(SQL_HANDLE_STMT, 
                            phstmt,
                            i,
                            sqlState,
                            &nativeError,
                            message,
                            sizeof(message),
                            &textLength) == SQL_SUCCESS) {
            if (i > 1) {
                errorDetails += "\n";
            }
            errorDetails += "[" + std::string((char*)sqlState) + "] ";
            errorDetails += "Native Error: " + std::to_string(nativeError) + " - ";
            errorDetails += (char*)message;
            i++;
        }
        return errorDetails;
    }

    /**
     * Helper function to bind columns for SQL result set.
     *
     * This function iterates through a vector of ColumnBinding structures,
     * binding each column to the appropriate buffer in the result set.
     *
     * @param bindings A vector of ColumnBinding structures, each containing
     *                 information for binding a single column.
     *
     * @return SQL_SUCCESS if all columns are successfully bound,
     *         SQL_ERROR if any binding fails or if a column is not found.
     */
    SQLRETURN ShowDiscoveryBase::columnBindingHelper(std::vector<ColumnBinding>& bindings) {
        SQLRETURN rc = SQL_SUCCESS;
        for (const auto& binding : bindings) {
            int columnIndex = getIndex(m_pStmt, binding.columnName);
            if (columnIndex <= 0) {
                RS_LOG_ERROR("columnBindingHelper",
                             "Column name not found in the mapping: %s",
                             binding.columnName.c_str());
                return SQL_ERROR;
            }
            rc = RS_STMT_INFO::RS_SQLBindCol(
                m_pStmt,
                columnIndex,
                binding.targetType,
                binding.targetValue,
                binding.bufferLength,
                binding.strLengthPtr);
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR("columnBindingHelper", binding.errorMsg);
                return rc;
            }
        }
        return rc;
    }

    //-------------------------------------------------------------------------
    // ShowDatabasesHelper Implementation
    //-------------------------------------------------------------------------
    ShowDatabasesHelper::ShowDatabasesHelper(SQLHSTMT stmt, const std::string& catalog,
                        std::vector<std::string>& catalogs,
                        bool isSingleDatabaseMetaData)
        : ShowDiscoveryBase(stmt, "ShowDatabasesHelper"),
            m_catalog(catalog), m_catalogs(catalogs),
            m_isSingleDatabaseMetaData(isSingleDatabaseMetaData) {}

    SQLRETURN ShowDatabasesHelper::execute() {
        RS_LOG_TRACE(m_operationName,
                        "Starting operation with parameter catalog = \"%s\", "
                        "isSingleDatabaseMetaData = %d",
                        m_catalog.c_str(), m_isSingleDatabaseMetaData);

        // Handle current database case
        std::string curCatalog = getDatabase(m_pStmt);
        if (curCatalog.empty()) {
            RS_LOG_ERROR(m_operationName, 
                "Error when retrieving current database name");
            return SQL_ERROR;
        }
        RS_LOG_TRACE(m_operationName, "Current database name: \"%s\"", curCatalog.c_str());

        // Handle the case when isSingleDatabaseMetaData is true
        if (m_isSingleDatabaseMetaData && m_catalog.empty()) {
            m_catalogs.push_back(curCatalog);
            RS_LOG_TRACE(m_operationName, "number of catalogs: %d", m_catalogs.size());
            return SQL_SUCCESS;
        }

        // Prepare and execute query
        std::string query = m_catalog.empty() 
            ? RsMetadataAPIHelper::kshowDatabasesQuery 
            : RsMetadataAPIHelper::kshowDatabasesLikeQuery;

        std::vector<std::string> params = m_catalog.empty() 
            ? std::vector<std::string>{} 
            : std::vector<std::string>{m_catalog};

        SQLRETURN rc = prepareBindAndExecuteQuery(query, params);
        if (!SQL_SUCCEEDED(rc)) {
            RS_LOG_ERROR(m_operationName, 
                    "Failed to execute query: \"%s\".",
                    query.c_str());
            return rc;
        }

        // Process results
        {
            ColumnBindingCleanup guard(m_stmt);

            SQLCHAR buf[MAX_IDEN_LEN] = {0};
            SQLLEN catalogLen;

            rc = RS_STMT_INFO::RS_SQLBindCol(
                m_pStmt,
                getIndex(m_pStmt, RsMetadataAPIHelper::kSHOW_DATABASES_database_name),
                SQL_C_CHAR,
                buf,
                sizeof(buf),
                &catalogLen);

            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR(m_operationName, "Failed to bind column for result");
                return rc;
            }

            while (SQL_SUCCEEDED(rc = RS_STMT_INFO::RS_SQLFetchScroll(
                m_stmt, SQL_FETCH_NEXT, 0))) {
                std::string cur = char2String(buf);
                if (m_isSingleDatabaseMetaData) {
                    if (cur == curCatalog) {
                        m_catalogs.emplace_back(cur);
                        break;
                    }
                    continue;
                }
                m_catalogs.emplace_back(cur);
            }
        }
        RS_LOG_TRACE(m_operationName, "number of catalogs: %d", m_catalogs.size());
        return (rc == SQL_NO_DATA) ? SQL_SUCCESS : rc;
    }

    //-------------------------------------------------------------------------
    // ShowSchemasHelper Implementation
    //-------------------------------------------------------------------------
    ShowSchemasHelper::ShowSchemasHelper(SQLHSTMT phstmt, const std::string &catalog,
                    const std::string &schema,
                    std::vector<SHOWSCHEMASResult> &intermediateRS)
        : ShowDiscoveryBase(phstmt, "ShowSchemasHelper"), m_catalog(catalog),
            m_schema(schema), m_intermediateRS(intermediateRS) {}

    SQLRETURN ShowSchemasHelper::execute() {
        RS_LOG_TRACE(m_operationName,
                        "Starting operation with parameter catalog = \"%s\", "
                        "schema = \"%s\"",
                        m_catalog.c_str(), m_schema.c_str());

        // Input parameter validation
        if (m_catalog.empty()) {
            RS_LOG_ERROR(m_operationName, "catalog name should not be empty");
            return SQL_ERROR;
        }

        // Prepare and execute query
        std::vector<std::string> params = {m_catalog};
        std::string query;
        if (m_schema.empty()) {
            query = RsMetadataAPIHelper::kshowSchemasQuery;
        } else {
            params.push_back(m_schema);
            query = RsMetadataAPIHelper::kshowSchemasLikeQuery;
        }

        SQLRETURN rc = prepareBindAndExecuteQuery(query, params);
        if (!SQL_SUCCEEDED(rc)) {
            RS_LOG_ERROR(m_operationName, 
                    "Failed to execute query: \"%s\".",
                    query.c_str());
            return rc;
        }

        // Process results
        {
            ColumnBindingCleanup guard(m_stmt);

            SHOWSCHEMASResult cur;
            std::vector<ColumnBinding> bindings = {
                {RsMetadataAPIHelper::kSHOW_SCHEMAS_database_name, SQL_C_CHAR,
                 cur.database_name, sizeof(cur.database_name), &cur.database_name_Len,
                 "Failed to bind column for database_name"},
                {RsMetadataAPIHelper::kSHOW_SCHEMAS_schema_name, SQL_C_CHAR,
                 cur.schema_name, sizeof(cur.schema_name), &cur.schema_name_Len,
                 "Failed to bind column for schema_name"}
            };

            rc = columnBindingHelper(bindings);
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR(m_operationName, "Failed to bind result column");
                return rc;
            }

            while (SQL_SUCCEEDED(
                rc = RS_STMT_INFO::RS_SQLFetchScroll(m_pStmt, SQL_FETCH_NEXT, 0))) {
                m_intermediateRS.emplace_back(cur);
            }
        }
        RS_LOG_TRACE(m_operationName, "number of schemas: %d", m_intermediateRS.size());
        return (rc == SQL_NO_DATA) ? SQL_SUCCESS : rc;
    }

    //-------------------------------------------------------------------------
    // ShowTablesHelper Implementation
    //-------------------------------------------------------------------------
    ShowTablesHelper::ShowTablesHelper(SQLHSTMT phstmt, const std::string &catalog,
                    const std::string &schema, const std::string &table,
                    std::vector<SHOWTABLESResult> &intermediateRS)
        : ShowDiscoveryBase(phstmt, "ShowTablesHelper"), m_catalog(catalog),
            m_schema(schema), m_table(table), m_intermediateRS(intermediateRS) {}

    SQLRETURN ShowTablesHelper::execute() {
        RS_LOG_TRACE(m_operationName,
                        "Starting operation with parameter catalog = \"%s\", "
                        "schema = \"%s\", table = \"%s\"",
                        m_catalog.c_str(), m_schema.c_str(), m_table.c_str());

        // Input parameter validation
        if (m_catalog.empty() || m_schema.empty()) {
            RS_LOG_ERROR(m_operationName,
                            "Required parameters catalog/schema should not be empty");
            return SQL_ERROR;
        }

        // Prepare and execute query
        std::vector<std::string> params = {m_catalog, m_schema};
        std::string query;
        if (m_table.empty()) {
            query = RsMetadataAPIHelper::kshowTablesQuery;
        } else {
            params.push_back(m_table);
            query = RsMetadataAPIHelper::kshowTablesLikeQuery;
        }

        SQLRETURN rc = prepareBindAndExecuteQuery(query, params);
        if (!SQL_SUCCEEDED(rc)) {
            RS_LOG_ERROR(m_operationName, "Failed to execute query: \"%s\".",
                        query.c_str());
            return rc;
        }

        // Process results
        {
            ColumnBindingCleanup guard(m_stmt);

            SHOWTABLESResult cur;
            std::vector<ColumnBinding> bindings = {
                {RsMetadataAPIHelper::kSHOW_TABLES_database_name, SQL_C_CHAR,
                 cur.database_name, sizeof(cur.database_name), &cur.database_name_Len,
                 "Failed to bind column for database_name"},
                {RsMetadataAPIHelper::kSHOW_TABLES_schema_name, SQL_C_CHAR,
                 cur.schema_name, sizeof(cur.schema_name), &cur.schema_name_Len,
                 "Failed to bind column for schema_name"},
                {RsMetadataAPIHelper::kSHOW_TABLES_table_name, SQL_C_CHAR,
                 cur.table_name, sizeof(cur.table_name), &cur.table_name_Len,
                 "Failed to bind column for table_name"},
                {RsMetadataAPIHelper::kSHOW_TABLES_table_type, SQL_C_CHAR,
                 cur.table_type, sizeof(cur.table_type), &cur.table_type_Len,
                 "Failed to bind column for table_type"},
                {RsMetadataAPIHelper::kSHOW_TABLES_remarks, SQL_C_CHAR,
                 cur.remarks, sizeof(cur.remarks), &cur.remarks_Len,
                 "Failed to bind column for remarks"},
                 {RsMetadataAPIHelper::kSHOW_TABLES_owner, SQL_C_CHAR,
                 cur.owner, sizeof(cur.owner), &cur.owner_Len,
                 "Failed to bind column for owner"},
                {RsMetadataAPIHelper::kSHOW_TABLES_last_altered_time, SQL_C_CHAR,
                 cur.last_altered_time, sizeof(cur.last_altered_time), &cur.last_altered_time_Len,
                 "Failed to bind column for last_altered_time"},
                {RsMetadataAPIHelper::kSHOW_TABLES_last_modified_time, SQL_C_CHAR,
                 cur.last_modified_time, sizeof(cur.last_modified_time), &cur.last_modified_time_Len,
                 "Failed to bind column for last_modified_time"},
                {RsMetadataAPIHelper::kSHOW_TABLES_dist_style, SQL_C_CHAR,
                 cur.dist_style, sizeof(cur.dist_style), &cur.dist_style_Len,
                 "Failed to bind column for dist_style"},
                {RsMetadataAPIHelper::kSHOW_TABLES_table_subtype, SQL_C_CHAR,
                 cur.table_subtype, sizeof(cur.table_subtype), &cur.table_subtype_Len,
                 "Failed to bind column for table_subtype"}
            };

            rc = columnBindingHelper(bindings);
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR(m_operationName, "Failed to bind result column");
                return rc;
            }

            while (SQL_SUCCEEDED(rc = RS_STMT_INFO::RS_SQLFetchScroll(
                                    m_pStmt, SQL_FETCH_NEXT, 0))) {
                m_intermediateRS.emplace_back(cur);
            }
        }
        RS_LOG_TRACE(m_operationName, "number of tables: %d", m_intermediateRS.size());
        return (rc == SQL_NO_DATA) ? SQL_SUCCESS : rc;
    }

    //-------------------------------------------------------------------------
    // ShowColumnsHelper Implementation
    //-------------------------------------------------------------------------
    ShowColumnsHelper::ShowColumnsHelper(SQLHSTMT phstmt, const std::string &catalog,
                    const std::string &schema, const std::string &table,
                    const std::string &column,
                    std::vector<SHOWCOLUMNSResult> &intermediateRS)
        : ShowDiscoveryBase(phstmt, "ShowColumnsHelper"), m_catalog(catalog),
            m_schema(schema), m_table(table), m_column(column),
            m_intermediateRS(intermediateRS) {}

    SQLRETURN ShowColumnsHelper::execute() {
        RS_LOG_TRACE(m_operationName,
                        "Starting operation with parameter catalog = \"%s\", "
                        "schema = \"%s\", table = \"%s\", column = \"%s\"",
                        m_catalog.c_str(), m_schema.c_str(), m_table.c_str(),
                        m_column.c_str());

        // Input parameter validation
        if (m_catalog.empty() || m_schema.empty() || m_table.empty()) {
            RS_LOG_ERROR(m_operationName,
                            "Required parameters catalog/schema/table should "
                            "not be empty");
            return SQL_ERROR;
        }

        // Prepare and execute query
        std::vector<std::string> params = {m_catalog, m_schema, m_table};
        std::string query;
        if (m_column.empty()) {
            query = RsMetadataAPIHelper::kshowColumnsQuery;
        } else {
            params.push_back(m_column);
            query = RsMetadataAPIHelper::kshowColumnsLikeQuery;
        }

        SQLRETURN rc = prepareBindAndExecuteQuery(query, params);
        if (!SQL_SUCCEEDED(rc)) {
            RS_LOG_ERROR(m_operationName, "Failed to execute query: \"%s\".",
                        query.c_str());
            return rc;
        }

        // Process results
        {
            ColumnBindingCleanup guard(m_stmt);

            SHOWCOLUMNSResult cur;
            std::vector<ColumnBinding> bindings = {
                {RsMetadataAPIHelper::kSHOW_COLUMNS_database_name, SQL_C_CHAR,
                cur.database_name, sizeof(cur.database_name), &cur.database_name_Len,
                "Failed to bind column for database_name"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_schema_name, SQL_C_CHAR,
                cur.schema_name, sizeof(cur.schema_name), &cur.schema_name_Len,
                "Failed to bind column for schema_name"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_table_name, SQL_C_CHAR,
                cur.table_name, sizeof(cur.table_name), &cur.table_name_Len,
                "Failed to bind column for table_name"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_column_name, SQL_C_CHAR,
                cur.column_name, sizeof(cur.column_name), &cur.column_name_Len,
                "Failed to bind column for column_name"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_ordinal_position, SQL_C_SSHORT,
                &cur.ordinal_position, sizeof(cur.ordinal_position), &cur.ordinal_position_Len,
                "Failed to bind column for ordinal_position"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_column_default, SQL_C_CHAR,
                cur.column_default, sizeof(cur.column_default), &cur.column_default_Len,
                "Failed to bind column for column_default"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_is_nullable, SQL_C_CHAR,
                cur.is_nullable, sizeof(cur.is_nullable), &cur.is_nullable_Len,
                "Failed to bind column for is_nullable"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_data_type, SQL_C_CHAR,
                cur.data_type, sizeof(cur.data_type), &cur.data_type_Len,
                "Failed to bind column for data_type"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_character_maximum_length, SQL_C_SSHORT,
                &cur.character_maximum_length, sizeof(cur.character_maximum_length), &cur.character_maximum_length_Len,
                "Failed to bind column for character_maximum_length"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_numeric_precision, SQL_C_SSHORT,
                &cur.numeric_precision, sizeof(cur.numeric_precision), &cur.numeric_precision_Len,
                "Failed to bind column for numeric_precision"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_numeric_scale, SQL_C_SSHORT,
                &cur.numeric_scale, sizeof(cur.numeric_scale), &cur.numeric_scale_Len,
                "Failed to bind column for numeric_scale"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_remarks, SQL_C_CHAR,
                cur.remarks, sizeof(cur.remarks), &cur.remarks_Len,
                "Failed to bind column for remarks"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_sort_key_type, SQL_C_CHAR,
                cur.sort_key_type, sizeof(cur.sort_key_type), &cur.sort_key_type_Len,
                "Failed to bind column for sort_key_type"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_sort_key, SQL_C_SLONG,
                &cur.sort_key, sizeof(cur.sort_key), &cur.sort_key_Len,
                "Failed to bind column for sort_key"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_dist_key, SQL_C_SLONG,
                &cur.dist_key, sizeof(cur.dist_key), &cur.dist_key_Len,
                "Failed to bind column for dist_key"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_encoding, SQL_C_CHAR,
                cur.encoding, sizeof(cur.encoding), &cur.encoding_Len,
                "Failed to bind column for encoding"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_collation, SQL_C_CHAR,
                cur.collation, sizeof(cur.collation), &cur.collation_Len,
                "Failed to bind column for collation"}
            };

            rc = columnBindingHelper(bindings);
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR(m_operationName, "Failed to bind result column");
                return rc;
            }

            while (SQL_SUCCEEDED(rc = RS_STMT_INFO::RS_SQLFetchScroll(
                                    m_pStmt, SQL_FETCH_NEXT, 0))) {
                m_intermediateRS.emplace_back(cur);
            }
        }
        RS_LOG_TRACE(m_operationName, "number of columns: %d", m_intermediateRS.size());
        return (rc == SQL_NO_DATA) ? SQL_SUCCESS : rc;
    }

    //-------------------------------------------------------------------------
    // ShowConstraintsPkHelper Implementation
    //-------------------------------------------------------------------------
    ShowConstraintsPkHelper::ShowConstraintsPkHelper(SQLHSTMT phstmt, const std::string &catalog,
                    const std::string &schema, const std::string &table,
                    std::vector<SHOWCONSTRAINTSPRIMARYKEYSResult> &intermediateRS)
        : ShowDiscoveryBase(phstmt, "ShowConstraintsPkHelper"), m_catalog(catalog),
            m_schema(schema), m_table(table),
            m_intermediateRS(intermediateRS) {}

    SQLRETURN ShowConstraintsPkHelper::execute() {
        RS_LOG_TRACE(m_operationName,
                        "Starting operation with parameter catalog = \"%s\", "
                        "schema = \"%s\", table = \"%s\"",
                        m_catalog.c_str(), m_schema.c_str(), m_table.c_str());

        // Input parameter validation
        if (m_catalog.empty() || m_schema.empty() || m_table.empty()) {
            RS_LOG_ERROR(m_operationName,
                            "Required parameters catalog/schema/table should "
                            "not be empty");
            return SQL_ERROR;
        }

        // Prepare and execute query
        std::vector<std::string> params = {m_catalog, m_schema, m_table};
        std::string query = RsMetadataAPIHelper::kshowConstraintsPkQuery;

        SQLRETURN rc = prepareBindAndExecuteQuery(query, params);
        if (!SQL_SUCCEEDED(rc)) {
            RS_LOG_ERROR(m_operationName, "Failed to execute query: \"%s\".",
                        query.c_str());
            return rc;
        }

        // Process results
        {
            ColumnBindingCleanup guard(m_stmt);

            SHOWCONSTRAINTSPRIMARYKEYSResult cur;
            std::vector<ColumnBinding> bindings = {
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_PK_database_name, SQL_C_CHAR,
                cur.database_name, sizeof(cur.database_name), &cur.database_name_Len,
                "Failed to bind column for database_name"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_PK_schema_name, SQL_C_CHAR,
                cur.schema_name, sizeof(cur.schema_name), &cur.schema_name_Len,
                "Failed to bind column for schema_name"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_PK_table_name, SQL_C_CHAR,
                cur.table_name, sizeof(cur.table_name), &cur.table_name_Len,
                "Failed to bind column for table_name"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_PK_column_name, SQL_C_CHAR,
                cur.column_name, sizeof(cur.column_name), &cur.column_name_Len,
                "Failed to bind column for column_name"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_PK_key_seq, SQL_C_SSHORT,
                &cur.key_seq, sizeof(cur.key_seq), &cur.key_seq_Len,
                "Failed to bind column for key_seq"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_PK_pk_name, SQL_C_CHAR,
                cur.pk_name, sizeof(cur.pk_name), &cur.pk_name_Len,
                "Failed to bind column for pk_name"}
            };

            rc = columnBindingHelper(bindings);
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR(m_operationName, "Failed to bind result column");
                return rc;
            }

            while (SQL_SUCCEEDED(rc = RS_STMT_INFO::RS_SQLFetchScroll(
                                    m_pStmt, SQL_FETCH_NEXT, 0))) {
                m_intermediateRS.emplace_back(cur);
            }
        }
        RS_LOG_TRACE(m_operationName, "number of primary keys: %d", m_intermediateRS.size());
        return (rc == SQL_NO_DATA) ? SQL_SUCCESS : rc;
    }

    //-------------------------------------------------------------------------
    // ShowConstraintsFkHelper Implementation
    //-------------------------------------------------------------------------
    ShowConstraintsFkHelper::ShowConstraintsFkHelper(
        SQLHSTMT phstmt, const std::string &catalog,
        const std::string &schema, const std::string &table, bool isExported,
        std::vector<SHOWCONSTRAINTSFOREIGNKEYSResult> &intermediateRS)
        : ShowDiscoveryBase(phstmt, "ShowConstraintsFkHelper"), m_catalog(catalog),
            m_schema(schema), m_table(table), isExported(isExported),
            m_intermediateRS(intermediateRS) {}

    SQLRETURN ShowConstraintsFkHelper::execute() {
        RS_LOG_TRACE(m_operationName,
                        "Starting operation with parameter catalog = \"%s\", "
                        "schema = \"%s\", table = \"%s\", isExported = \"%d\"",
                        m_catalog.c_str(), m_schema.c_str(), m_table.c_str(), isExported);

        // Input parameter validation
        if (m_catalog.empty() || m_schema.empty() || m_table.empty()) {
            RS_LOG_ERROR(m_operationName,
                            "Required parameters catalog/schema/table should "
                            "not be empty");
            return SQL_ERROR;
        }

        // Prepare and execute query
        std::vector<std::string> params = {m_catalog, m_schema, m_table};
        const std::string query = isExported
            ? RsMetadataAPIHelper::kshowConstraintsFkExportQuery  // Exported Keys query
            : RsMetadataAPIHelper::kshowConstraintsFkQuery;    // Standard FK query

        SQLRETURN rc = prepareBindAndExecuteQuery(query, params);
        if (!SQL_SUCCEEDED(rc)) {
            RS_LOG_ERROR(m_operationName, "Failed to execute query: \"%s\".",
                        query.c_str());
            return rc;
        }

        // Process results
        {
            ColumnBindingCleanup guard(m_stmt);

            SHOWCONSTRAINTSFOREIGNKEYSResult cur;
            std::vector<ColumnBinding> bindings = {
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_pk_database_name, SQL_C_CHAR,
                cur.pk_table_cat, sizeof(cur.pk_table_cat), &cur.pk_table_cat_Len,
                "Failed to bind column for pk_table_cat"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_pk_schema_name, SQL_C_CHAR,
                cur.pk_table_schem, sizeof(cur.pk_table_schem), &cur.pk_table_schem_Len,
                "Failed to bind column for pk_table_schem"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_pk_table_name, SQL_C_CHAR,
                cur.pk_table_name, sizeof(cur.pk_table_name), &cur.pk_table_name_Len,
                "Failed to bind column for pk_table_name"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_pk_column_name, SQL_C_CHAR,
                cur.pk_column_name, sizeof(cur.pk_column_name), &cur.pk_column_name_Len,
                "Failed to bind column for pk_column_name"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_fk_database_name, SQL_C_CHAR,
                cur.fk_table_cat, sizeof(cur.fk_table_cat), &cur.fk_table_cat_Len,
                "Failed to bind column for fk_table_cat"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_fk_schema_name, SQL_C_CHAR,
                cur.fk_table_schem, sizeof(cur.fk_table_schem), &cur.fk_table_schem_Len,
                "Failed to bind column for fk_table_schem"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_fk_table_name, SQL_C_CHAR,
                cur.fk_table_name, sizeof(cur.fk_table_name), &cur.fk_table_name_Len,
                "Failed to bind column for fk_table_name"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_fk_column_name, SQL_C_CHAR,
                cur.fk_column_name, sizeof(cur.fk_column_name), &cur.fk_column_name_Len,
                "Failed to bind column for fk_column_name"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_key_seq, SQL_C_SSHORT,
                &cur.key_seq, sizeof(cur.key_seq), &cur.key_seq_Len,
                "Failed to bind column for key_seq"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_update_rule, SQL_C_SSHORT,
                &cur.update_rule, sizeof(cur.update_rule), &cur.update_rule_Len,
                "Failed to bind column for update_rule"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_delete_rule, SQL_C_SSHORT,
                &cur.delete_rule, sizeof(cur.delete_rule), &cur.delete_rule_Len,
                "Failed to bind column for delete_rule"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_fk_name, SQL_C_CHAR,
                cur.fk_name, sizeof(cur.fk_name), &cur.fk_name_Len,
                "Failed to bind column for fk_name"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_pk_name, SQL_C_CHAR,
                cur.pk_name, sizeof(cur.pk_name), &cur.pk_name_Len,
                "Failed to bind column for pk_name"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_deferrability, SQL_C_SSHORT,
                &cur.deferrability, sizeof(cur.deferrability), &cur.deferrability_Len,
                "Failed to bind column for deferrability"}
            };

            rc = columnBindingHelper(bindings);
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR(m_operationName, "Failed to bind result column");
                return rc;
            }

            while (SQL_SUCCEEDED(rc = RS_STMT_INFO::RS_SQLFetchScroll(
                                    m_pStmt, SQL_FETCH_NEXT, 0))) {
                m_intermediateRS.emplace_back(cur);
            }
        }
        RS_LOG_TRACE(m_operationName, "number of foreign keys: %d",
                    m_intermediateRS.size());
        return (rc == SQL_NO_DATA) ? SQL_SUCCESS : rc;
    }

    //-------------------------------------------------------------------------
    // ShowGrantsColumnHelper Implementation
    //-------------------------------------------------------------------------
    ShowGrantsColumnHelper::ShowGrantsColumnHelper(SQLHSTMT phstmt, const std::string &catalog,
                    const std::string &schema, const std::string &table,
                    const std::string &column,
                    std::vector<SHOWGRANTSCOLUMNResult> &intermediateRS)
        : ShowDiscoveryBase(phstmt, "ShowGrantsColumnHelper"), m_catalog(catalog),
            m_schema(schema), m_table(table), m_column(column),
            m_intermediateRS(intermediateRS) {}

    SQLRETURN ShowGrantsColumnHelper::execute() {
        RS_LOG_TRACE(m_operationName,
                        "Starting operation with parameter catalog = \"%s\", "
                        "schema = \"%s\", table = \"%s\", column = \"%s\"",
                        m_catalog.c_str(), m_schema.c_str(), m_table.c_str(),
                        m_column.c_str());

        // Input parameter validation
        if (m_catalog.empty() || m_schema.empty() || m_table.empty()) {
            RS_LOG_ERROR(m_operationName,
                            "Required parameters catalog/schema/table should "
                            "not be empty");
            return SQL_ERROR;
        }

        // Prepare and execute query
        std::vector<std::string> params = {m_catalog, m_schema, m_table};
        std::string query = RsMetadataAPIHelper::kshowGrantsColumnQuery;

        SQLRETURN rc = prepareBindAndExecuteQuery(query, params);
        if (!SQL_SUCCEEDED(rc)) {
            RS_LOG_ERROR(m_operationName, "Failed to execute query: \"%s\".",
                        query.c_str());
            return rc;
        }

        // Process results
        {
            ColumnBindingCleanup guard(m_stmt);

            SHOWGRANTSCOLUMNResult cur;
            std::vector<ColumnBinding> bindings = {
                {RsMetadataAPIHelper::kSHOW_GRANTS_database_name, SQL_C_CHAR,
                cur.table_cat, sizeof(cur.table_cat), &cur.table_cat_Len,
                "Failed to bind column for database_name"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_schema_name, SQL_C_CHAR,
                cur.table_schem, sizeof(cur.table_schem), &cur.table_schem_Len,
                "Failed to bind column for schema_name"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_table_name, SQL_C_CHAR,
                cur.table_name, sizeof(cur.table_name), &cur.table_name_Len,
                "Failed to bind column for table_name"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_column_name, SQL_C_CHAR,
                cur.column_name, sizeof(cur.column_name), &cur.column_name_Len,
                "Failed to bind column for column_name"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_grantor, SQL_C_CHAR,
                cur.grantor, sizeof(cur.grantor), &cur.grantor_Len,
                "Failed to bind column for grantor"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_identity_name, SQL_C_CHAR,
                cur.grantee, sizeof(cur.grantee), &cur.grantee_Len,
                "Failed to bind column for identity_name"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_privilege_type, SQL_C_CHAR,
                cur.privilege, sizeof(cur.privilege), &cur.privilege_Len,
                "Failed to bind column for privilege_type"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_admin_option, SQL_C_BIT,
                &cur.admin_option, sizeof(cur.admin_option), &cur.admin_option_len,
                "Failed to bind column for admin_option"}
            };

            rc = columnBindingHelper(bindings);
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR(m_operationName, "Failed to bind result column");
                return rc;
            }

            std::vector<SHOWGRANTSCOLUMNResult> tempResults;

            while (SQL_SUCCEEDED(rc = RS_STMT_INFO::RS_SQLFetchScroll(
                                    m_pStmt, SQL_FETCH_NEXT, 0))) {
                // Apply pattern matching
                if(RsMetadataAPIHelper::patternMatch(char2String(cur.column_name), m_column)) {
                    tempResults.emplace_back(cur);
                }
            }

            // Sort the results based on privilege
            std::sort(tempResults.begin(), tempResults.end(),
                [](const SHOWGRANTSCOLUMNResult& a, const SHOWGRANTSCOLUMNResult& b) {
                    return char2String(a.privilege) < char2String(b.privilege);
            });

            m_intermediateRS.insert(m_intermediateRS.end(),
                          tempResults.begin(),
                          tempResults.end());
        }
        RS_LOG_TRACE(m_operationName, "number of column privileges: %d", m_intermediateRS.size());
        return (rc == SQL_NO_DATA) ? SQL_SUCCESS : rc;
    }

    //-------------------------------------------------------------------------
    // ShowGrantsTableHelper Implementation
    //-------------------------------------------------------------------------
    ShowGrantsTableHelper::ShowGrantsTableHelper(SQLHSTMT phstmt, const std::string &catalog,
                    const std::string &schema, const std::string &table,
                    std::vector<SHOWGRANTSTABLEResult> &intermediateRS)
        : ShowDiscoveryBase(phstmt, "ShowGrantsTableHelper"), m_catalog(catalog),
            m_schema(schema), m_table(table),
            m_intermediateRS(intermediateRS) {}

    SQLRETURN ShowGrantsTableHelper::execute() {
        RS_LOG_TRACE(m_operationName,
                        "Starting operation with parameter catalog = \"%s\", "
                        "schema = \"%s\", table = \"%s\"",
                        m_catalog.c_str(), m_schema.c_str(), m_table.c_str());

        // Input parameter validation
        if (m_catalog.empty() || m_schema.empty() || m_table.empty()) {
            RS_LOG_ERROR(m_operationName,
                            "Required parameters catalog/schema/table should "
                            "not be empty");
            return SQL_ERROR;
        }

        // Prepare and execute query
        std::vector<std::string> params = {m_catalog, m_schema, m_table};
        std::string query = RsMetadataAPIHelper::kshowGrantsTableQuery;

        SQLRETURN rc = prepareBindAndExecuteQuery(query, params);
        if (!SQL_SUCCEEDED(rc)) {
            RS_LOG_ERROR(m_operationName, "Failed to execute query: \"%s\".",
                        query.c_str());
            return rc;
        }

        // Process results
        {
            ColumnBindingCleanup guard(m_stmt);

            SHOWGRANTSTABLEResult cur;
            std::vector<ColumnBinding> bindings = {
                {RsMetadataAPIHelper::kSHOW_GRANTS_database_name, SQL_C_CHAR,
                cur.table_cat, sizeof(cur.table_cat), &cur.table_cat_Len,
                "Failed to bind column for database_name"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_schema_name, SQL_C_CHAR,
                cur.table_schem, sizeof(cur.table_schem), &cur.table_schem_Len,
                "Failed to bind column for schema_name"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_object_name, SQL_C_CHAR,
                cur.table_name, sizeof(cur.table_name), &cur.table_name_Len,
                "Failed to bind column for table_name"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_grantor, SQL_C_CHAR,
                cur.grantor, sizeof(cur.grantor), &cur.grantor_Len,
                "Failed to bind column for grantor"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_identity_name, SQL_C_CHAR,
                cur.grantee, sizeof(cur.grantee), &cur.grantee_Len,
                "Failed to bind column for identity_name"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_privilege_type, SQL_C_CHAR,
                cur.privilege, sizeof(cur.privilege), &cur.privilege_Len,
                "Failed to bind column for privilege_type"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_admin_option, SQL_C_BIT,
                &cur.admin_option, sizeof(cur.admin_option), &cur.admin_option_len,
                "Failed to bind column for admin_option"}
            };

            rc = columnBindingHelper(bindings);
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR(m_operationName, "Failed to bind result column");
                return rc;
            }

            std::vector<SHOWGRANTSTABLEResult> tempResults;

            while (SQL_SUCCEEDED(rc = RS_STMT_INFO::RS_SQLFetchScroll(
                                    m_pStmt, SQL_FETCH_NEXT, 0))) {
                tempResults.emplace_back(cur);
            }

            // Sort the results based on privilege
            std::sort(tempResults.begin(), tempResults.end(),
                [](const SHOWGRANTSTABLEResult& a, const SHOWGRANTSTABLEResult& b) {
                    return char2String(a.privilege) < char2String(b.privilege);
            });

            m_intermediateRS.insert(m_intermediateRS.end(),
                          tempResults.begin(),
                          tempResults.end());
        }
        RS_LOG_TRACE(m_operationName, "number of table privileges: %d", m_intermediateRS.size());
        return (rc == SQL_NO_DATA) ? SQL_SUCCESS : rc;
    }

    //-------------------------------------------------------------------------
    // ShowProceduresFunctionsHelper Implementation
    //-------------------------------------------------------------------------
    ShowProceduresFunctionsHelper::ShowProceduresFunctionsHelper(SQLHSTMT phstmt, const std::string &catalog,
                    const std::string &schema, const std::string &procedure_function,
                    bool isProcedures,
                    std::vector<SHOWPROCEDURESFUNCTIONSResult> &intermediateRS)
        : ShowDiscoveryBase(phstmt, "ShowProceduresFunctionsHelper"), m_catalog(catalog),
            m_schema(schema), m_procedure_function(procedure_function),
            isProcedures(isProcedures), m_intermediateRS(intermediateRS) {}

    SQLRETURN ShowProceduresFunctionsHelper::execute() {
        RS_LOG_TRACE(m_operationName,
                        "Starting operation with parameter catalog = \"%s\", "
                        "schema = \"%s\", procedure/function = \"%s\", isProcedures = \"%d\"",
                        m_catalog.c_str(), m_schema.c_str(), m_procedure_function.c_str(), isProcedures);

        // Input parameter validation
        if (m_catalog.empty() || m_schema.empty()) {
            RS_LOG_ERROR(m_operationName,
                            "Required parameters catalog/schema should "
                            "not be empty");
            return SQL_ERROR;
        }

        // Prepare and execute query
        std::vector<std::string> params = {m_catalog, m_schema};
        std::string query;
        if (m_procedure_function.empty()) {
            query = isProcedures ? RsMetadataAPIHelper::kshowProceduresQuery
                                    : RsMetadataAPIHelper::kshowFunctionsQuery;
        } else {
            params.push_back(m_procedure_function);
            query = isProcedures
                        ? RsMetadataAPIHelper::kshowProceduresLikeQuery
                        : RsMetadataAPIHelper::kshowFunctionsLikeQuery;
        }

        SQLRETURN rc = prepareBindAndExecuteQuery(query, params);
        if (!SQL_SUCCEEDED(rc)) {
            RS_LOG_ERROR(m_operationName, "Failed to execute query: \"%s\".",
                        query.c_str());
            return rc;
        }

        // Process results
        {
            ColumnBindingCleanup guard(m_stmt);

            SHOWPROCEDURESFUNCTIONSResult cur;
            std::string database_name = isProcedures 
                ? RsMetadataAPIHelper::kSHOW_PROCEDURES_database_name
                : RsMetadataAPIHelper::kSHOW_FUNCTIONS_database_name;

            std::string schema_name = isProcedures
                ? RsMetadataAPIHelper::kSHOW_PROCEDURES_schema_name
                : RsMetadataAPIHelper::kSHOW_FUNCTIONS_schema_name;

            std::string procedure_function_name = isProcedures
                ? RsMetadataAPIHelper::kSHOW_PROCEDURES_procedure_name
                : RsMetadataAPIHelper::kSHOW_FUNCTIONS_function_name;

            std::string argument_list = isProcedures
                ? RsMetadataAPIHelper::kSHOW_PROCEDURES_argument_list
                : RsMetadataAPIHelper::kSHOW_FUNCTIONS_argument_list;

            std::vector<ColumnBinding> bindings = {
                {database_name, SQL_C_CHAR,
                cur.object_cat, sizeof(cur.object_cat), &cur.object_cat_Len,
                "Failed to bind column for database_name"},
                {schema_name, SQL_C_CHAR,
                cur.object_schem, sizeof(cur.object_schem), &cur.object_schem_Len,
                "Failed to bind column for schema_name"},
                {procedure_function_name, SQL_C_CHAR,
                cur.object_name, sizeof(cur.object_name), &cur.object_name_Len,
                "Failed to bind column for procedure_name / function_name"},
                {argument_list, SQL_C_CHAR,
                cur.argument_list, sizeof(cur.argument_list), &cur.argument_list_Len,
                "Failed to bind column for argument list"}
            };

            rc = columnBindingHelper(bindings);
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR(m_operationName, "Failed to bind result column");
                return rc;
            }

            cur.object_type = isProcedures ? SQL_PT_PROCEDURE : SQL_PT_FUNCTION;
            cur.object_type_Len = sizeof(SQLSMALLINT);

            while (SQL_SUCCEEDED(rc = RS_STMT_INFO::RS_SQLFetchScroll(
                                    m_pStmt, SQL_FETCH_NEXT, 0))) {
                m_intermediateRS.emplace_back(cur);
            }
        }
        RS_LOG_TRACE(m_operationName, "number of procedures/functions: %d", m_intermediateRS.size());
        return (rc == SQL_NO_DATA) ? SQL_SUCCESS : rc;
    }

    //-------------------------------------------------------------------------
    // ShowProcedureFunctionColumnsHelper Implementation
    //-------------------------------------------------------------------------
    ShowProcedureFunctionColumnsHelper::ShowProcedureFunctionColumnsHelper(SQLHSTMT phstmt, const std::string &catalog,
                    const std::string &schema, const std::string &procedure_function,
                    const std::string &column, const std::string &sqlQuery,
                    const std::vector<std::string> &argumentList, bool isProcedure,
                    std::vector<SHOWCOLUMNSResult> &intermediateRS)
        : ShowDiscoveryBase(phstmt, "ShowProcedureFunctionColumnsHelper"), m_catalog(catalog),
            m_schema(schema), m_procedure_function(procedure_function), m_column(column),
            m_sqlQuery(sqlQuery), m_argumentList(argumentList), isProcedure(isProcedure),
            m_intermediateRS(intermediateRS) {}

    SQLRETURN ShowProcedureFunctionColumnsHelper::execute() {
        RS_LOG_TRACE(m_operationName,
                        "Starting operation with parameter catalog = \"%s\", "
                        "schema = \"%s\", procedure/function = \"%s\", column "
                        "= \"%s\", isProcedures = \"%d\"",
                        m_catalog.c_str(), m_schema.c_str(),
                        m_procedure_function.c_str(), m_column.c_str(),
                        isProcedure);

        // Input parameter validation
        if (m_catalog.empty() || m_schema.empty() || m_procedure_function.empty()) {
            RS_LOG_ERROR(m_operationName,
                            "Required parameters "
                            "catalog/schema/procedure/function should "
                            "not be empty");
            return SQL_ERROR;
        }

        // Prepare and execute query
        std::vector<std::string> params = {m_catalog, m_schema, m_procedure_function};
        params.insert(params.end(), m_argumentList.begin(), m_argumentList.end());
        if (!m_column.empty()) {
            params.push_back(m_column);
        }

        SQLRETURN rc = prepareBindAndExecuteQuery(m_sqlQuery, params);
        if (!SQL_SUCCEEDED(rc)) {
            RS_LOG_ERROR(m_operationName, "Failed to execute query: \"%s\".",
                        m_sqlQuery.c_str());
            return rc;
        }

        // Process results
        {
            ColumnBindingCleanup guard(m_stmt);

            SHOWCOLUMNSResult cur;
            std::string object_name = isProcedure
                ? RsMetadataAPIHelper::kSHOW_PARAMETERS_procedure_name
                : RsMetadataAPIHelper::kSHOW_PARAMETERS_function_name;

            std::vector<ColumnBinding> bindings = {
                {RsMetadataAPIHelper::kSHOW_PARAMETERS_database_name, SQL_C_CHAR,
                cur.database_name, sizeof(cur.database_name), &cur.database_name_Len,
                "Failed to bind column for database_name"},
                {RsMetadataAPIHelper::kSHOW_PARAMETERS_schema_name, SQL_C_CHAR,
                cur.schema_name, sizeof(cur.schema_name), &cur.schema_name_Len,
                "Failed to bind column for schema_name"},
                {object_name, SQL_C_CHAR,
                cur.procedure_function_name, sizeof(cur.procedure_function_name), &cur.procedure_function_name_Len,
                "Failed to bind column for procedure/function name"},
                {RsMetadataAPIHelper::kSHOW_PARAMETERS_parameter_name, SQL_C_CHAR,
                cur.column_name, sizeof(cur.column_name), &cur.column_name_Len,
                "Failed to bind column for column_name"},
                {RsMetadataAPIHelper::kSHOW_PARAMETERS_parameter_type, SQL_C_CHAR,
                cur.parameter_type, sizeof(cur.parameter_type), &cur.parameter_type_Len,
                "Failed to bind column for parameter_type"},
                {RsMetadataAPIHelper::kSHOW_PARAMETERS_ordinal_position, SQL_C_SSHORT,
                &cur.ordinal_position, sizeof(cur.ordinal_position), &cur.ordinal_position_Len,
                "Failed to bind column for ordinal_position"},
                {RsMetadataAPIHelper::kSHOW_PARAMETERS_data_type, SQL_C_CHAR,
                cur.data_type, sizeof(cur.data_type), &cur.data_type_Len,
                "Failed to bind column for data_type"},
                {RsMetadataAPIHelper::kSHOW_PARAMETERS_character_maximum_length, SQL_C_SSHORT,
                &cur.character_maximum_length, sizeof(cur.character_maximum_length), &cur.character_maximum_length_Len,
                "Failed to bind column for character_maximum_length"},
                {RsMetadataAPIHelper::kSHOW_PARAMETERS_numeric_precision, SQL_C_SSHORT,
                &cur.numeric_precision, sizeof(cur.numeric_precision), &cur.numeric_precision_Len,
                "Failed to bind column for numeric_precision"},
                {RsMetadataAPIHelper::kSHOW_PARAMETERS_numeric_scale, SQL_C_SSHORT,
                &cur.numeric_scale, sizeof(cur.numeric_scale), &cur.numeric_scale_Len,
                "Failed to bind column for numeric_scale"}
            };

            rc = columnBindingHelper(bindings);
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR(m_operationName, "Failed to bind result column");
                return rc;
            }

            while (SQL_SUCCEEDED(rc = RS_STMT_INFO::RS_SQLFetchScroll(
                                    m_pStmt, SQL_FETCH_NEXT, 0))) {
                m_intermediateRS.emplace_back(cur);
            }
        }
        RS_LOG_TRACE(m_operationName, "number of procedure/function columns: %d", m_intermediateRS.size());
        return (rc == SQL_NO_DATA) ? SQL_SUCCESS : rc;
    }
}
