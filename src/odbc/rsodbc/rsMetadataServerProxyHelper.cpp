/*-------------------------------------------------------------------------
 *
 * Copyright(c) 2025, Amazon.com, Inc. or Its Affiliates. All rights reserved.
 *
 * Author: stinghsu
 *-------------------------------------------------------------------------
 */

#include "rsMetadataServerProxyHelper.h"
#include <limits>

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

    std::optional<std::string> ShowDiscoveryBase::extractStringField(
        const SQLCHAR* buf, SQLLEN bufSize, SQLLEN len, const std::string& colName) {
        if (len == SQL_NULL_DATA) {
            return std::nullopt;  // preserve SQL NULL
        }
        if (len <= 0) {
            return std::string(); // genuine empty string
        }
        if (len < bufSize) {
            // Data fits in buffer — no truncation
            return std::string(reinterpret_cast<const char*>(buf), len);
        }
        // Truncation detected — resolve column index only when needed (avoid
        // per-row getIndex overhead for the common non-truncation path)
        SQLUSMALLINT colIndex = getIndex(m_pStmt, colName);
        RS_LOG_DEBUG("extractStringField",
                "Truncation detected for column %d: len=%lld, bufSize=%lld. "
                "Falling back to SQLGetData.",
                colIndex, (long long)len, (long long)bufSize);

        std::string result(len, '\0');
        SQLLEN actualLen = 0;
        SQLLEN pcbLenIndInternal = (std::numeric_limits<SQLLEN>::min)();
        SQLRETURN rc = RS_STMT_INFO::RS_SQLGetData(
            m_pStmt, colIndex, SQL_C_CHAR,
            result.data(), len + 1, &actualLen,
            TRUE, pcbLenIndInternal);
        if (SQL_SUCCEEDED(rc) && actualLen > 0 && actualLen != SQL_NULL_DATA) {
            result.resize(actualLen);
        } else {
            // Fallback: use whatever was in the buffer (truncated)
            RS_LOG_WARN("extractStringField",
                "SQLGetData fallback failed for column %d, using truncated data", colIndex);
            result = std::string(reinterpret_cast<const char*>(buf),
                                 std::min<SQLLEN>(len, bufSize - 1));
        }
        return result;
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

            // Local fixed buffers for SQLBindCol targets
            SQLCHAR buf_database_name[NAMEDATALEN] = {0};
            SQLLEN len_database_name = 0;
            SQLCHAR buf_schema_name[NAMEDATALEN] = {0};
            SQLLEN len_schema_name = 0;

            std::vector<ColumnBinding> bindings = {
                {RsMetadataAPIHelper::kSHOW_SCHEMAS_database_name, SQL_C_CHAR,
                 buf_database_name, sizeof(buf_database_name), &len_database_name,
                 "Failed to bind column for database_name"},
                {RsMetadataAPIHelper::kSHOW_SCHEMAS_schema_name, SQL_C_CHAR,
                 buf_schema_name, sizeof(buf_schema_name), &len_schema_name,
                 "Failed to bind column for schema_name"}
            };

            rc = columnBindingHelper(bindings);
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR(m_operationName, "Failed to bind result column");
                return rc;
            }

            while (SQL_SUCCEEDED(
                rc = RS_STMT_INFO::RS_SQLFetchScroll(m_pStmt, SQL_FETCH_NEXT, 0))) {
                SHOWSCHEMASResult cur;
                cur.database_name = extractStringField(buf_database_name, sizeof(buf_database_name), len_database_name, RsMetadataAPIHelper::kSHOW_SCHEMAS_database_name);
                cur.schema_name = extractStringField(buf_schema_name, sizeof(buf_schema_name), len_schema_name, RsMetadataAPIHelper::kSHOW_SCHEMAS_schema_name);
                m_intermediateRS.emplace_back(std::move(cur));
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

            // Local fixed buffers for SQLBindCol targets
            SQLCHAR buf_database_name[NAMEDATALEN] = {0};  SQLLEN len_database_name = 0;
            SQLCHAR buf_schema_name[NAMEDATALEN] = {0};     SQLLEN len_schema_name = 0;
            SQLCHAR buf_table_name[NAMEDATALEN] = {0};      SQLLEN len_table_name = 0;
            SQLCHAR buf_table_type[NAMEDATALEN] = {0};      SQLLEN len_table_type = 0;
            SQLCHAR buf_remarks[DEFAULT_MAX_REMARK_LEN] = {0};      SQLLEN len_remarks = 0;
            SQLCHAR buf_owner[NAMEDATALEN] = {0};           SQLLEN len_owner = 0;
            SQLCHAR buf_last_altered[NAMEDATALEN] = {0};    SQLLEN len_last_altered = 0;
            SQLCHAR buf_last_modified[NAMEDATALEN] = {0};   SQLLEN len_last_modified = 0;
            SQLCHAR buf_dist_style[NAMEDATALEN] = {0};      SQLLEN len_dist_style = 0;
            SQLCHAR buf_table_subtype[NAMEDATALEN] = {0};   SQLLEN len_table_subtype = 0;

            std::vector<ColumnBinding> bindings = {
                {RsMetadataAPIHelper::kSHOW_TABLES_database_name, SQL_C_CHAR,
                 buf_database_name, sizeof(buf_database_name), &len_database_name,
                 "Failed to bind column for database_name"},
                {RsMetadataAPIHelper::kSHOW_TABLES_schema_name, SQL_C_CHAR,
                 buf_schema_name, sizeof(buf_schema_name), &len_schema_name,
                 "Failed to bind column for schema_name"},
                {RsMetadataAPIHelper::kSHOW_TABLES_table_name, SQL_C_CHAR,
                 buf_table_name, sizeof(buf_table_name), &len_table_name,
                 "Failed to bind column for table_name"},
                {RsMetadataAPIHelper::kSHOW_TABLES_table_type, SQL_C_CHAR,
                 buf_table_type, sizeof(buf_table_type), &len_table_type,
                 "Failed to bind column for table_type"},
                {RsMetadataAPIHelper::kSHOW_TABLES_remarks, SQL_C_CHAR,
                 buf_remarks, sizeof(buf_remarks), &len_remarks,
                 "Failed to bind column for remarks"},
                 {RsMetadataAPIHelper::kSHOW_TABLES_owner, SQL_C_CHAR,
                 buf_owner, sizeof(buf_owner), &len_owner,
                 "Failed to bind column for owner"},
                {RsMetadataAPIHelper::kSHOW_TABLES_last_altered_time, SQL_C_CHAR,
                 buf_last_altered, sizeof(buf_last_altered), &len_last_altered,
                 "Failed to bind column for last_altered_time"},
                {RsMetadataAPIHelper::kSHOW_TABLES_last_modified_time, SQL_C_CHAR,
                 buf_last_modified, sizeof(buf_last_modified), &len_last_modified,
                 "Failed to bind column for last_modified_time"},
                {RsMetadataAPIHelper::kSHOW_TABLES_dist_style, SQL_C_CHAR,
                 buf_dist_style, sizeof(buf_dist_style), &len_dist_style,
                 "Failed to bind column for dist_style"},
                {RsMetadataAPIHelper::kSHOW_TABLES_table_subtype, SQL_C_CHAR,
                 buf_table_subtype, sizeof(buf_table_subtype), &len_table_subtype,
                 "Failed to bind column for table_subtype"}
            };

            rc = columnBindingHelper(bindings);
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR(m_operationName, "Failed to bind result column");
                return rc;
            }

            while (SQL_SUCCEEDED(rc = RS_STMT_INFO::RS_SQLFetchScroll(
                                    m_pStmt, SQL_FETCH_NEXT, 0))) {
                SHOWTABLESResult cur;
                cur.database_name    = extractStringField(buf_database_name, sizeof(buf_database_name), len_database_name, RsMetadataAPIHelper::kSHOW_TABLES_database_name);
                cur.schema_name      = extractStringField(buf_schema_name, sizeof(buf_schema_name), len_schema_name, RsMetadataAPIHelper::kSHOW_TABLES_schema_name);
                cur.table_name       = extractStringField(buf_table_name, sizeof(buf_table_name), len_table_name, RsMetadataAPIHelper::kSHOW_TABLES_table_name);
                cur.table_type       = extractStringField(buf_table_type, sizeof(buf_table_type), len_table_type, RsMetadataAPIHelper::kSHOW_TABLES_table_type);
                cur.remarks          = extractStringField(buf_remarks, sizeof(buf_remarks), len_remarks, RsMetadataAPIHelper::kSHOW_TABLES_remarks);
                cur.owner            = extractStringField(buf_owner, sizeof(buf_owner), len_owner, RsMetadataAPIHelper::kSHOW_TABLES_owner);
                cur.last_altered_time = extractStringField(buf_last_altered, sizeof(buf_last_altered), len_last_altered, RsMetadataAPIHelper::kSHOW_TABLES_last_altered_time);
                cur.last_modified_time = extractStringField(buf_last_modified, sizeof(buf_last_modified), len_last_modified, RsMetadataAPIHelper::kSHOW_TABLES_last_modified_time);
                cur.dist_style       = extractStringField(buf_dist_style, sizeof(buf_dist_style), len_dist_style, RsMetadataAPIHelper::kSHOW_TABLES_dist_style);
                cur.table_subtype    = extractStringField(buf_table_subtype, sizeof(buf_table_subtype), len_table_subtype, RsMetadataAPIHelper::kSHOW_TABLES_table_subtype);
                m_intermediateRS.emplace_back(std::move(cur));
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

            // Local fixed buffers for SQLBindCol targets
            SQLCHAR buf_database_name[NAMEDATALEN] = {0};       SQLLEN len_database_name = 0;
            SQLCHAR buf_schema_name[NAMEDATALEN] = {0};         SQLLEN len_schema_name = 0;
            SQLCHAR buf_table_name[NAMEDATALEN] = {0};          SQLLEN len_table_name = 0;
            SQLCHAR buf_column_name[NAMEDATALEN] = {0};         SQLLEN len_column_name = 0;
            SQLSMALLINT ordinal_position = 0;                   SQLLEN ordinal_position_Len = 0;
            SQLCHAR buf_column_default[MAX_COLUMN_DEF_LEN] = {0}; SQLLEN len_column_default = 0;
            SQLCHAR buf_is_nullable[NAMEDATALEN] = {0};         SQLLEN len_is_nullable = 0;
            SQLCHAR buf_data_type[NAMEDATALEN] = {0};           SQLLEN len_data_type = 0;
            SQLINTEGER character_maximum_length = 0;             SQLLEN character_maximum_length_Len = 0;
            SQLSMALLINT numeric_precision = 0;                  SQLLEN numeric_precision_Len = 0;
            SQLSMALLINT numeric_scale = 0;                      SQLLEN numeric_scale_Len = 0;
            SQLCHAR buf_remarks[DEFAULT_MAX_REMARK_LEN] = {0};         SQLLEN len_remarks = 0;
            SQLCHAR buf_sort_key_type[NAMEDATALEN] = {0};       SQLLEN len_sort_key_type = 0;
            SQLINTEGER sort_key = 0;                            SQLLEN sort_key_Len = 0;
            SQLINTEGER dist_key = 0;                            SQLLEN dist_key_Len = 0;
            SQLCHAR buf_encoding[NAMEDATALEN] = {0};            SQLLEN len_encoding = 0;
            SQLCHAR buf_collation[NAMEDATALEN] = {0};           SQLLEN len_collation = 0;

            std::vector<ColumnBinding> bindings = {
                {RsMetadataAPIHelper::kSHOW_COLUMNS_database_name, SQL_C_CHAR,
                buf_database_name, sizeof(buf_database_name), &len_database_name,
                "Failed to bind column for database_name"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_schema_name, SQL_C_CHAR,
                buf_schema_name, sizeof(buf_schema_name), &len_schema_name,
                "Failed to bind column for schema_name"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_table_name, SQL_C_CHAR,
                buf_table_name, sizeof(buf_table_name), &len_table_name,
                "Failed to bind column for table_name"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_column_name, SQL_C_CHAR,
                buf_column_name, sizeof(buf_column_name), &len_column_name,
                "Failed to bind column for column_name"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_ordinal_position, SQL_C_SSHORT,
                &ordinal_position, sizeof(ordinal_position), &ordinal_position_Len,
                "Failed to bind column for ordinal_position"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_column_default, SQL_C_CHAR,
                buf_column_default, sizeof(buf_column_default), &len_column_default,
                "Failed to bind column for column_default"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_is_nullable, SQL_C_CHAR,
                buf_is_nullable, sizeof(buf_is_nullable), &len_is_nullable,
                "Failed to bind column for is_nullable"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_data_type, SQL_C_CHAR,
                buf_data_type, sizeof(buf_data_type), &len_data_type,
                "Failed to bind column for data_type"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_character_maximum_length, SQL_C_SLONG,
                &character_maximum_length, sizeof(character_maximum_length), &character_maximum_length_Len,
                "Failed to bind column for character_maximum_length"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_numeric_precision, SQL_C_SSHORT,
                &numeric_precision, sizeof(numeric_precision), &numeric_precision_Len,
                "Failed to bind column for numeric_precision"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_numeric_scale, SQL_C_SSHORT,
                &numeric_scale, sizeof(numeric_scale), &numeric_scale_Len,
                "Failed to bind column for numeric_scale"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_remarks, SQL_C_CHAR,
                buf_remarks, sizeof(buf_remarks), &len_remarks,
                "Failed to bind column for remarks"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_sort_key_type, SQL_C_CHAR,
                buf_sort_key_type, sizeof(buf_sort_key_type), &len_sort_key_type,
                "Failed to bind column for sort_key_type"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_sort_key, SQL_C_SLONG,
                &sort_key, sizeof(sort_key), &sort_key_Len,
                "Failed to bind column for sort_key"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_dist_key, SQL_C_SLONG,
                &dist_key, sizeof(dist_key), &dist_key_Len,
                "Failed to bind column for dist_key"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_encoding, SQL_C_CHAR,
                buf_encoding, sizeof(buf_encoding), &len_encoding,
                "Failed to bind column for encoding"},
                {RsMetadataAPIHelper::kSHOW_COLUMNS_collation, SQL_C_CHAR,
                buf_collation, sizeof(buf_collation), &len_collation,
                "Failed to bind column for collation"}
            };

            rc = columnBindingHelper(bindings);
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR(m_operationName, "Failed to bind result column");
                return rc;
            }

            while (SQL_SUCCEEDED(rc = RS_STMT_INFO::RS_SQLFetchScroll(
                                    m_pStmt, SQL_FETCH_NEXT, 0))) {
                SHOWCOLUMNSResult cur;
                cur.database_name = extractStringField(buf_database_name, sizeof(buf_database_name), len_database_name, RsMetadataAPIHelper::kSHOW_COLUMNS_database_name);
                cur.schema_name = extractStringField(buf_schema_name, sizeof(buf_schema_name), len_schema_name, RsMetadataAPIHelper::kSHOW_COLUMNS_schema_name);
                cur.table_name = extractStringField(buf_table_name, sizeof(buf_table_name), len_table_name, RsMetadataAPIHelper::kSHOW_COLUMNS_table_name);
                cur.column_name = extractStringField(buf_column_name, sizeof(buf_column_name), len_column_name, RsMetadataAPIHelper::kSHOW_COLUMNS_column_name);
                cur.ordinal_position = ordinal_position_Len == SQL_NULL_DATA ? 0 : ordinal_position;
                cur.ordinal_position_Len = ordinal_position_Len;
                cur.column_default = extractStringField(buf_column_default, sizeof(buf_column_default), len_column_default, RsMetadataAPIHelper::kSHOW_COLUMNS_column_default);
                cur.is_nullable = extractStringField(buf_is_nullable, sizeof(buf_is_nullable), len_is_nullable, RsMetadataAPIHelper::kSHOW_COLUMNS_is_nullable);
                cur.data_type = extractStringField(buf_data_type, sizeof(buf_data_type), len_data_type, RsMetadataAPIHelper::kSHOW_COLUMNS_data_type);
                cur.character_maximum_length = character_maximum_length_Len == SQL_NULL_DATA ? 0 : character_maximum_length;
                cur.character_maximum_length_Len = character_maximum_length_Len;
                cur.numeric_precision = numeric_precision_Len == SQL_NULL_DATA ? 0 : numeric_precision;
                cur.numeric_precision_Len = numeric_precision_Len;
                cur.numeric_scale = numeric_scale_Len == SQL_NULL_DATA ? 0 : numeric_scale;
                cur.numeric_scale_Len = numeric_scale_Len;
                cur.remarks = extractStringField(buf_remarks, sizeof(buf_remarks), len_remarks, RsMetadataAPIHelper::kSHOW_COLUMNS_remarks);
                cur.sort_key_type = extractStringField(buf_sort_key_type, sizeof(buf_sort_key_type), len_sort_key_type, RsMetadataAPIHelper::kSHOW_COLUMNS_sort_key_type);
                cur.sort_key = sort_key_Len == SQL_NULL_DATA ? 0 : sort_key;
                cur.sort_key_Len = sort_key_Len;
                cur.dist_key = dist_key_Len == SQL_NULL_DATA ? 0 : dist_key;
                cur.dist_key_Len = dist_key_Len;
                cur.encoding = extractStringField(buf_encoding, sizeof(buf_encoding), len_encoding, RsMetadataAPIHelper::kSHOW_COLUMNS_encoding);
                cur.collation = extractStringField(buf_collation, sizeof(buf_collation), len_collation, RsMetadataAPIHelper::kSHOW_COLUMNS_collation);
                m_intermediateRS.emplace_back(std::move(cur));
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

            // Local fixed buffers for SQLBindCol targets
            SQLCHAR buf_database_name[NAMEDATALEN] = {0};   SQLLEN len_database_name = 0;
            SQLCHAR buf_schema_name[NAMEDATALEN] = {0};     SQLLEN len_schema_name = 0;
            SQLCHAR buf_table_name[NAMEDATALEN] = {0};      SQLLEN len_table_name = 0;
            SQLCHAR buf_column_name[NAMEDATALEN] = {0};     SQLLEN len_column_name = 0;
            SQLSMALLINT key_seq = 0;                        SQLLEN key_seq_Len = 0;
            SQLCHAR buf_pk_name[NAMEDATALEN] = {0};         SQLLEN len_pk_name = 0;

            std::vector<ColumnBinding> bindings = {
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_PK_database_name, SQL_C_CHAR,
                buf_database_name, sizeof(buf_database_name), &len_database_name,
                "Failed to bind column for database_name"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_PK_schema_name, SQL_C_CHAR,
                buf_schema_name, sizeof(buf_schema_name), &len_schema_name,
                "Failed to bind column for schema_name"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_PK_table_name, SQL_C_CHAR,
                buf_table_name, sizeof(buf_table_name), &len_table_name,
                "Failed to bind column for table_name"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_PK_column_name, SQL_C_CHAR,
                buf_column_name, sizeof(buf_column_name), &len_column_name,
                "Failed to bind column for column_name"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_PK_key_seq, SQL_C_SSHORT,
                &key_seq, sizeof(key_seq), &key_seq_Len,
                "Failed to bind column for key_seq"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_PK_pk_name, SQL_C_CHAR,
                buf_pk_name, sizeof(buf_pk_name), &len_pk_name,
                "Failed to bind column for pk_name"}
            };

            rc = columnBindingHelper(bindings);
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR(m_operationName, "Failed to bind result column");
                return rc;
            }

            while (SQL_SUCCEEDED(rc = RS_STMT_INFO::RS_SQLFetchScroll(
                                    m_pStmt, SQL_FETCH_NEXT, 0))) {
                SHOWCONSTRAINTSPRIMARYKEYSResult cur;
                cur.database_name = extractStringField(buf_database_name, sizeof(buf_database_name), len_database_name, RsMetadataAPIHelper::kSHOW_CONSTRAINTS_PK_database_name);
                cur.schema_name = extractStringField(buf_schema_name, sizeof(buf_schema_name), len_schema_name, RsMetadataAPIHelper::kSHOW_CONSTRAINTS_PK_schema_name);
                cur.table_name = extractStringField(buf_table_name, sizeof(buf_table_name), len_table_name, RsMetadataAPIHelper::kSHOW_CONSTRAINTS_PK_table_name);
                cur.column_name = extractStringField(buf_column_name, sizeof(buf_column_name), len_column_name, RsMetadataAPIHelper::kSHOW_CONSTRAINTS_PK_column_name);
                cur.key_seq = key_seq_Len == SQL_NULL_DATA ? 0 : key_seq;
                cur.key_seq_Len = key_seq_Len;
                cur.pk_name = extractStringField(buf_pk_name, sizeof(buf_pk_name), len_pk_name,  RsMetadataAPIHelper::kSHOW_CONSTRAINTS_PK_pk_name);
                m_intermediateRS.emplace_back(std::move(cur));
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

            // Local fixed buffers for SQLBindCol targets
            SQLCHAR buf_pk_table_cat[NAMEDATALEN] = {0};      SQLLEN len_pk_table_cat = 0;
            SQLCHAR buf_pk_table_schem[NAMEDATALEN] = {0};    SQLLEN len_pk_table_schem = 0;
            SQLCHAR buf_pk_table_name[NAMEDATALEN] = {0};     SQLLEN len_pk_table_name = 0;
            SQLCHAR buf_pk_column_name[NAMEDATALEN] = {0};    SQLLEN len_pk_column_name = 0;
            SQLCHAR buf_fk_table_cat[NAMEDATALEN] = {0};      SQLLEN len_fk_table_cat = 0;
            SQLCHAR buf_fk_table_schem[NAMEDATALEN] = {0};    SQLLEN len_fk_table_schem = 0;
            SQLCHAR buf_fk_table_name[NAMEDATALEN] = {0};     SQLLEN len_fk_table_name = 0;
            SQLCHAR buf_fk_column_name[NAMEDATALEN] = {0};    SQLLEN len_fk_column_name = 0;
            SQLSMALLINT key_seq = 0;                          SQLLEN key_seq_Len = 0;
            SQLSMALLINT update_rule = 0;                      SQLLEN update_rule_Len = 0;
            SQLSMALLINT delete_rule = 0;                      SQLLEN delete_rule_Len = 0;
            SQLCHAR buf_fk_name[NAMEDATALEN] = {0};           SQLLEN len_fk_name = 0;
            SQLCHAR buf_pk_name[NAMEDATALEN] = {0};           SQLLEN len_pk_name = 0;
            SQLSMALLINT deferrability = 0;                    SQLLEN deferrability_Len = 0;

            std::vector<ColumnBinding> bindings = {
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_pk_database_name, SQL_C_CHAR,
                buf_pk_table_cat, sizeof(buf_pk_table_cat), &len_pk_table_cat,
                "Failed to bind column for pk_table_cat"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_pk_schema_name, SQL_C_CHAR,
                buf_pk_table_schem, sizeof(buf_pk_table_schem), &len_pk_table_schem,
                "Failed to bind column for pk_table_schem"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_pk_table_name, SQL_C_CHAR,
                buf_pk_table_name, sizeof(buf_pk_table_name), &len_pk_table_name,
                "Failed to bind column for pk_table_name"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_pk_column_name, SQL_C_CHAR,
                buf_pk_column_name, sizeof(buf_pk_column_name), &len_pk_column_name,
                "Failed to bind column for pk_column_name"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_fk_database_name, SQL_C_CHAR,
                buf_fk_table_cat, sizeof(buf_fk_table_cat), &len_fk_table_cat,
                "Failed to bind column for fk_table_cat"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_fk_schema_name, SQL_C_CHAR,
                buf_fk_table_schem, sizeof(buf_fk_table_schem), &len_fk_table_schem,
                "Failed to bind column for fk_table_schem"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_fk_table_name, SQL_C_CHAR,
                buf_fk_table_name, sizeof(buf_fk_table_name), &len_fk_table_name,
                "Failed to bind column for fk_table_name"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_fk_column_name, SQL_C_CHAR,
                buf_fk_column_name, sizeof(buf_fk_column_name), &len_fk_column_name,
                "Failed to bind column for fk_column_name"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_key_seq, SQL_C_SSHORT,
                &key_seq, sizeof(key_seq), &key_seq_Len,
                "Failed to bind column for key_seq"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_update_rule, SQL_C_SSHORT,
                &update_rule, sizeof(update_rule), &update_rule_Len,
                "Failed to bind column for update_rule"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_delete_rule, SQL_C_SSHORT,
                &delete_rule, sizeof(delete_rule), &delete_rule_Len,
                "Failed to bind column for delete_rule"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_fk_name, SQL_C_CHAR,
                buf_fk_name, sizeof(buf_fk_name), &len_fk_name,
                "Failed to bind column for fk_name"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_pk_name, SQL_C_CHAR,
                buf_pk_name, sizeof(buf_pk_name), &len_pk_name,
                "Failed to bind column for pk_name"},
                {RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_deferrability, SQL_C_SSHORT,
                &deferrability, sizeof(deferrability), &deferrability_Len,
                "Failed to bind column for deferrability"}
            };

            rc = columnBindingHelper(bindings);
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR(m_operationName, "Failed to bind result column");
                return rc;
            }

            while (SQL_SUCCEEDED(rc = RS_STMT_INFO::RS_SQLFetchScroll(
                                    m_pStmt, SQL_FETCH_NEXT, 0))) {
                SHOWCONSTRAINTSFOREIGNKEYSResult cur;
                cur.pk_table_cat = extractStringField(buf_pk_table_cat, sizeof(buf_pk_table_cat), len_pk_table_cat, RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_pk_database_name);
                cur.pk_table_schem = extractStringField(buf_pk_table_schem, sizeof(buf_pk_table_schem), len_pk_table_schem, RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_pk_schema_name);
                cur.pk_table_name = extractStringField(buf_pk_table_name, sizeof(buf_pk_table_name), len_pk_table_name, RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_pk_table_name);
                cur.pk_column_name = extractStringField(buf_pk_column_name, sizeof(buf_pk_column_name), len_pk_column_name, RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_pk_column_name);
                cur.fk_table_cat = extractStringField(buf_fk_table_cat, sizeof(buf_fk_table_cat), len_fk_table_cat, RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_fk_database_name);
                cur.fk_table_schem = extractStringField(buf_fk_table_schem, sizeof(buf_fk_table_schem), len_fk_table_schem, RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_fk_schema_name);
                cur.fk_table_name = extractStringField(buf_fk_table_name, sizeof(buf_fk_table_name), len_fk_table_name, RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_fk_table_name);
                cur.fk_column_name = extractStringField(buf_fk_column_name, sizeof(buf_fk_column_name), len_fk_column_name, RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_fk_column_name);
                cur.key_seq = key_seq_Len == SQL_NULL_DATA ? 0 : key_seq;
                cur.key_seq_Len = key_seq_Len;
                cur.update_rule =update_rule_Len == SQL_NULL_DATA ? 0 : update_rule;
                cur.update_rule_Len = update_rule_Len;
                cur.delete_rule = delete_rule_Len == SQL_NULL_DATA ? 0 : delete_rule;
                cur.delete_rule_Len = delete_rule_Len;
                cur.fk_name = extractStringField(buf_fk_name, sizeof(buf_fk_name), len_fk_name, RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_fk_name);
                cur.pk_name = extractStringField(buf_pk_name, sizeof(buf_pk_name), len_pk_name, RsMetadataAPIHelper::kSHOW_CONSTRAINTS_FK_pk_name);
                cur.deferrability = deferrability_Len == SQL_NULL_DATA ? 0 : deferrability;
                cur.deferrability_Len = deferrability_Len;
                m_intermediateRS.emplace_back(std::move(cur));
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

            // Local fixed buffers for SQLBindCol targets
            SQLCHAR buf_table_cat[NAMEDATALEN] = {0};       SQLLEN len_table_cat = 0;
            SQLCHAR buf_table_schem[NAMEDATALEN] = {0};     SQLLEN len_table_schem = 0;
            SQLCHAR buf_table_name[NAMEDATALEN] = {0};      SQLLEN len_table_name = 0;
            SQLCHAR buf_column_name[NAMEDATALEN] = {0};     SQLLEN len_column_name = 0;
            SQLCHAR buf_grantor[NAMEDATALEN] = {0};         SQLLEN len_grantor = 0;
            SQLCHAR buf_grantee[NAMEDATALEN] = {0};         SQLLEN len_grantee = 0;
            SQLCHAR buf_privilege[NAMEDATALEN] = {0};       SQLLEN len_privilege = 0;
            SQLSMALLINT admin_option = 0;                   SQLLEN admin_option_len = 0;

            std::vector<ColumnBinding> bindings = {
                {RsMetadataAPIHelper::kSHOW_GRANTS_database_name, SQL_C_CHAR,
                buf_table_cat, sizeof(buf_table_cat), &len_table_cat,
                "Failed to bind column for database_name"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_schema_name, SQL_C_CHAR,
                buf_table_schem, sizeof(buf_table_schem), &len_table_schem,
                "Failed to bind column for schema_name"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_table_name, SQL_C_CHAR,
                buf_table_name, sizeof(buf_table_name), &len_table_name,
                "Failed to bind column for table_name"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_column_name, SQL_C_CHAR,
                buf_column_name, sizeof(buf_column_name), &len_column_name,
                "Failed to bind column for column_name"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_grantor, SQL_C_CHAR,
                buf_grantor, sizeof(buf_grantor), &len_grantor,
                "Failed to bind column for grantor"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_identity_name, SQL_C_CHAR,
                buf_grantee, sizeof(buf_grantee), &len_grantee,
                "Failed to bind column for identity_name"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_privilege_type, SQL_C_CHAR,
                buf_privilege, sizeof(buf_privilege), &len_privilege,
                "Failed to bind column for privilege_type"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_admin_option, SQL_C_BIT,
                &admin_option, sizeof(admin_option), &admin_option_len,
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
                SHOWGRANTSCOLUMNResult cur;
                cur.table_cat = extractStringField(buf_table_cat, sizeof(buf_table_cat), len_table_cat, RsMetadataAPIHelper::kSHOW_GRANTS_database_name);
                cur.table_schem = extractStringField(buf_table_schem, sizeof(buf_table_schem), len_table_schem, RsMetadataAPIHelper::kSHOW_GRANTS_schema_name);
                cur.table_name = extractStringField(buf_table_name, sizeof(buf_table_name), len_table_name, RsMetadataAPIHelper::kSHOW_GRANTS_table_name);
                cur.column_name = extractStringField(buf_column_name, sizeof(buf_column_name), len_column_name, RsMetadataAPIHelper::kSHOW_GRANTS_column_name);
                cur.grantor = extractStringField(buf_grantor, sizeof(buf_grantor), len_grantor, RsMetadataAPIHelper::kSHOW_GRANTS_grantor);
                cur.grantee = extractStringField(buf_grantee, sizeof(buf_grantee), len_grantee, RsMetadataAPIHelper::kSHOW_GRANTS_identity_name);
                cur.privilege = extractStringField(buf_privilege, sizeof(buf_privilege), len_privilege, RsMetadataAPIHelper::kSHOW_GRANTS_privilege_type);
                cur.admin_option = admin_option_len == SQL_NULL_DATA ? 0 : admin_option;
                cur.admin_option_len = admin_option_len;

                // Apply pattern matching
                if(RsMetadataAPIHelper::patternMatch(cur.column_name.value_or(""), m_column)) {
                    tempResults.emplace_back(std::move(cur));
                }
                cur = SHOWGRANTSCOLUMNResult{}; // Reset for next row to avoid stale NULL values
            }

            // Sort the results based on privilege
            std::sort(tempResults.begin(), tempResults.end(),
                [](const SHOWGRANTSCOLUMNResult& a, const SHOWGRANTSCOLUMNResult& b) {
                    return a.privilege < b.privilege;
            });

            m_intermediateRS.insert(m_intermediateRS.end(),
                          std::make_move_iterator(tempResults.begin()),
                          std::make_move_iterator(tempResults.end()));
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

            // Local fixed buffers for SQLBindCol targets
            SQLCHAR buf_table_cat[NAMEDATALEN] = {0};       SQLLEN len_table_cat = 0;
            SQLCHAR buf_table_schem[NAMEDATALEN] = {0};     SQLLEN len_table_schem = 0;
            SQLCHAR buf_table_name[NAMEDATALEN] = {0};      SQLLEN len_table_name = 0;
            SQLCHAR buf_grantor[NAMEDATALEN] = {0};         SQLLEN len_grantor = 0;
            SQLCHAR buf_grantee[NAMEDATALEN] = {0};         SQLLEN len_grantee = 0;
            SQLCHAR buf_privilege[NAMEDATALEN] = {0};       SQLLEN len_privilege = 0;
            SQLSMALLINT admin_option = 0;                   SQLLEN admin_option_len = 0;

            std::vector<ColumnBinding> bindings = {
                {RsMetadataAPIHelper::kSHOW_GRANTS_database_name, SQL_C_CHAR,
                buf_table_cat, sizeof(buf_table_cat), &len_table_cat,
                "Failed to bind column for database_name"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_schema_name, SQL_C_CHAR,
                buf_table_schem, sizeof(buf_table_schem), &len_table_schem,
                "Failed to bind column for schema_name"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_object_name, SQL_C_CHAR,
                buf_table_name, sizeof(buf_table_name), &len_table_name,
                "Failed to bind column for table_name"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_grantor, SQL_C_CHAR,
                buf_grantor, sizeof(buf_grantor), &len_grantor,
                "Failed to bind column for grantor"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_identity_name, SQL_C_CHAR,
                buf_grantee, sizeof(buf_grantee), &len_grantee,
                "Failed to bind column for identity_name"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_privilege_type, SQL_C_CHAR,
                buf_privilege, sizeof(buf_privilege), &len_privilege,
                "Failed to bind column for privilege_type"},
                {RsMetadataAPIHelper::kSHOW_GRANTS_admin_option, SQL_C_BIT,
                &admin_option, sizeof(admin_option), &admin_option_len,
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
                SHOWGRANTSTABLEResult cur;
                cur.table_cat = extractStringField(buf_table_cat, sizeof(buf_table_cat), len_table_cat, RsMetadataAPIHelper::kSHOW_GRANTS_database_name);
                cur.table_schem = extractStringField(buf_table_schem, sizeof(buf_table_schem), len_table_schem, RsMetadataAPIHelper::kSHOW_GRANTS_schema_name);
                cur.table_name = extractStringField(buf_table_name, sizeof(buf_table_name), len_table_name, RsMetadataAPIHelper::kSHOW_GRANTS_object_name);
                cur.grantor = extractStringField(buf_grantor, sizeof(buf_grantor), len_grantor, RsMetadataAPIHelper::kSHOW_GRANTS_grantor);
                cur.grantee = extractStringField(buf_grantee, sizeof(buf_grantee), len_grantee, RsMetadataAPIHelper::kSHOW_GRANTS_identity_name);
                cur.privilege = extractStringField(buf_privilege, sizeof(buf_privilege), len_privilege, RsMetadataAPIHelper::kSHOW_GRANTS_privilege_type);
                cur.admin_option = admin_option_len == SQL_NULL_DATA ? 0 : admin_option;
                cur.admin_option_len = admin_option_len;
                tempResults.emplace_back(std::move(cur));
            }

            // Sort the results based on privilege
            std::sort(tempResults.begin(), tempResults.end(),
                [](const SHOWGRANTSTABLEResult& a, const SHOWGRANTSTABLEResult& b) {
                    return a.privilege < b.privilege;
            });

            m_intermediateRS.insert(m_intermediateRS.end(),
                          std::make_move_iterator(tempResults.begin()),
                          std::make_move_iterator(tempResults.end()));
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

            // Local fixed buffers for SQLBindCol targets
            SQLCHAR buf_object_cat[NAMEDATALEN] = {0};      SQLLEN len_object_cat = 0;
            SQLCHAR buf_object_schem[NAMEDATALEN] = {0};    SQLLEN len_object_schem = 0;
            SQLCHAR buf_object_name[NAMEDATALEN] = {0};     SQLLEN len_object_name = 0;
            SQLCHAR buf_argument_list[4096] = {0};          SQLLEN len_argument_list = 0;

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
                buf_object_cat, sizeof(buf_object_cat), &len_object_cat,
                "Failed to bind column for database_name"},
                {schema_name, SQL_C_CHAR,
                buf_object_schem, sizeof(buf_object_schem), &len_object_schem,
                "Failed to bind column for schema_name"},
                {procedure_function_name, SQL_C_CHAR,
                buf_object_name, sizeof(buf_object_name), &len_object_name,
                "Failed to bind column for procedure_name / function_name"},
                {argument_list, SQL_C_CHAR,
                buf_argument_list, sizeof(buf_argument_list), &len_argument_list,
                "Failed to bind column for argument list"}
            };

            rc = columnBindingHelper(bindings);
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR(m_operationName, "Failed to bind result column");
                return rc;
            }

            SQLSMALLINT object_type = isProcedures ? SQL_PT_PROCEDURE : SQL_PT_FUNCTION;

            while (SQL_SUCCEEDED(rc = RS_STMT_INFO::RS_SQLFetchScroll(
                                    m_pStmt, SQL_FETCH_NEXT, 0))) {
                SHOWPROCEDURESFUNCTIONSResult cur;
                cur.object_cat = extractStringField(buf_object_cat, sizeof(buf_object_cat), len_object_cat, database_name);
                cur.object_schem = extractStringField(buf_object_schem, sizeof(buf_object_schem), len_object_schem, schema_name);
                cur.object_name = extractStringField(buf_object_name, sizeof(buf_object_name), len_object_name, procedure_function_name);
                cur.argument_list = extractStringField(buf_argument_list, sizeof(buf_argument_list), len_argument_list, argument_list);
                cur.object_type = object_type;
                cur.object_type_Len = sizeof(SQLSMALLINT);
                m_intermediateRS.emplace_back(std::move(cur));
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

            // Local fixed buffers for SQLBindCol targets
            SQLCHAR buf_database_name[NAMEDATALEN] = {0};       SQLLEN len_database_name = 0;
            SQLCHAR buf_schema_name[NAMEDATALEN] = {0};         SQLLEN len_schema_name = 0;
            SQLCHAR buf_procedure_function_name[NAMEDATALEN] = {0}; SQLLEN len_procedure_function_name = 0;
            SQLCHAR buf_column_name[NAMEDATALEN] = {0};         SQLLEN len_column_name = 0;
            SQLCHAR buf_parameter_type[NAMEDATALEN] = {0};      SQLLEN len_parameter_type = 0;
            SQLSMALLINT ordinal_position = 0;                   SQLLEN ordinal_position_Len = 0;
            SQLCHAR buf_data_type[NAMEDATALEN] = {0};           SQLLEN len_data_type = 0;
            SQLINTEGER character_maximum_length = 0;             SQLLEN character_maximum_length_Len = 0;
            SQLSMALLINT numeric_precision = 0;                  SQLLEN numeric_precision_Len = 0;
            SQLSMALLINT numeric_scale = 0;                      SQLLEN numeric_scale_Len = 0;

            std::string object_name = isProcedure
                ? RsMetadataAPIHelper::kSHOW_PARAMETERS_procedure_name
                : RsMetadataAPIHelper::kSHOW_PARAMETERS_function_name;

            std::vector<ColumnBinding> bindings = {
                {RsMetadataAPIHelper::kSHOW_PARAMETERS_database_name, SQL_C_CHAR,
                buf_database_name, sizeof(buf_database_name), &len_database_name,
                "Failed to bind column for database_name"},
                {RsMetadataAPIHelper::kSHOW_PARAMETERS_schema_name, SQL_C_CHAR,
                buf_schema_name, sizeof(buf_schema_name), &len_schema_name,
                "Failed to bind column for schema_name"},
                {object_name, SQL_C_CHAR,
                buf_procedure_function_name, sizeof(buf_procedure_function_name), &len_procedure_function_name,
                "Failed to bind column for procedure/function name"},
                {RsMetadataAPIHelper::kSHOW_PARAMETERS_parameter_name, SQL_C_CHAR,
                buf_column_name, sizeof(buf_column_name), &len_column_name,
                "Failed to bind column for column_name"},
                {RsMetadataAPIHelper::kSHOW_PARAMETERS_parameter_type, SQL_C_CHAR,
                buf_parameter_type, sizeof(buf_parameter_type), &len_parameter_type,
                "Failed to bind column for parameter_type"},
                {RsMetadataAPIHelper::kSHOW_PARAMETERS_ordinal_position, SQL_C_SSHORT,
                &ordinal_position, sizeof(ordinal_position), &ordinal_position_Len,
                "Failed to bind column for ordinal_position"},
                {RsMetadataAPIHelper::kSHOW_PARAMETERS_data_type, SQL_C_CHAR,
                buf_data_type, sizeof(buf_data_type), &len_data_type,
                "Failed to bind column for data_type"},
                {RsMetadataAPIHelper::kSHOW_PARAMETERS_character_maximum_length, SQL_C_SLONG,
                &character_maximum_length, sizeof(character_maximum_length), &character_maximum_length_Len,
                "Failed to bind column for character_maximum_length"},
                {RsMetadataAPIHelper::kSHOW_PARAMETERS_numeric_precision, SQL_C_SSHORT,
                &numeric_precision, sizeof(numeric_precision), &numeric_precision_Len,
                "Failed to bind column for numeric_precision"},
                {RsMetadataAPIHelper::kSHOW_PARAMETERS_numeric_scale, SQL_C_SSHORT,
                &numeric_scale, sizeof(numeric_scale), &numeric_scale_Len,
                "Failed to bind column for numeric_scale"}
            };

            rc = columnBindingHelper(bindings);
            if (!SQL_SUCCEEDED(rc)) {
                RS_LOG_ERROR(m_operationName, "Failed to bind result column");
                return rc;
            }

            while (SQL_SUCCEEDED(rc = RS_STMT_INFO::RS_SQLFetchScroll(
                                    m_pStmt, SQL_FETCH_NEXT, 0))) {
                SHOWCOLUMNSResult cur;
                cur.database_name = extractStringField(buf_database_name, sizeof(buf_database_name), len_database_name, RsMetadataAPIHelper::kSHOW_PARAMETERS_database_name);
                cur.schema_name = extractStringField(buf_schema_name, sizeof(buf_schema_name), len_schema_name, RsMetadataAPIHelper::kSHOW_PARAMETERS_schema_name);
                cur.procedure_function_name = extractStringField(buf_procedure_function_name, sizeof(buf_procedure_function_name), len_procedure_function_name, object_name);
                cur.column_name = extractStringField(buf_column_name, sizeof(buf_column_name), len_column_name, RsMetadataAPIHelper::kSHOW_PARAMETERS_parameter_name);
                cur.parameter_type = extractStringField(buf_parameter_type, sizeof(buf_parameter_type), len_parameter_type, RsMetadataAPIHelper::kSHOW_PARAMETERS_parameter_type);
                cur.ordinal_position = ordinal_position_Len == SQL_NULL_DATA ? 0 : ordinal_position;
                cur.ordinal_position_Len = ordinal_position_Len;
                cur.data_type = extractStringField(buf_data_type, sizeof(buf_data_type), len_data_type, RsMetadataAPIHelper::kSHOW_PARAMETERS_data_type);
                cur.character_maximum_length = character_maximum_length_Len == SQL_NULL_DATA ? 0 : character_maximum_length;
                cur.character_maximum_length_Len = character_maximum_length_Len;
                cur.numeric_precision = numeric_precision_Len == SQL_NULL_DATA ? 0 : numeric_precision;
                cur.numeric_precision_Len = numeric_precision_Len;
                cur.numeric_scale = numeric_scale_Len == SQL_NULL_DATA ? 0 : numeric_scale;
                cur.numeric_scale_Len = numeric_scale_Len;
                m_intermediateRS.emplace_back(std::move(cur));
            }
        }
        RS_LOG_TRACE(m_operationName, "number of procedure/function columns: %d", m_intermediateRS.size());
        return (rc == SQL_NO_DATA) ? SQL_SUCCESS : rc;
    }
}
