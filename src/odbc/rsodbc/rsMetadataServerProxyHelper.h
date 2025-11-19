/*-------------------------------------------------------------------------
 *
 * Copyright(c) 2025, Amazon.com, Inc. or Its Affiliates. All rights reserved.
 *
 * Author: stinghsu
 *-------------------------------------------------------------------------
 */

#ifndef RS_METADATA_SERVER_PROXY_HELPERS_H
#define RS_METADATA_SERVER_PROXY_HELPERS_H

#include "rsMetadataServerProxy.h"
#include "rsexecute.h"
#include "rsutil.h"
#include "rserror.h"


struct ColumnBinding {
    const std::string& columnName;
    SQLSMALLINT targetType;
    SQLPOINTER targetValue;
    SQLLEN bufferLength;
    SQLLEN* strLengthPtr;
    char* errorMsg;
};

/**
 * @namespace RsMetadataServerProxyHelpers
 * @brief Internal implementation namespace for SHOW command metadata discovery helpers
 * 
 * This namespace encapsulates implementation details for executing database
 * metadata discovery operations using SHOW commands (SHOW DATABASES, SHOW SCHEMAS, ...)
 * 
 * **Key Design Patterns:**
 * - Template Method: Base class defines execution flow, derived classes implement specifics
 * - RAII: ColumnBindingCleanup ensures automatic cleanup of ODBC column bindings
 * - Error Handling: Comprehensive SQLRETURN checking with detailed diagnostic logging
 */
namespace RsMetadataServerProxyHelpers {
     /**
     * @class ShowDiscoveryBase
     * @brief Abstract base class providing common ODBC operations for SHOW command execution
     * 
     * Implements the Template Method pattern for SHOW command workflows:
     * prepare → bind parameters → execute → process results → cleanup.
     * Provides standardized error handling and resource management for all SHOW operations.
     * 
     * @note Uses protected inheritance to enforce implementation-only usage by derived classes
     */
    class ShowDiscoveryBase {
    protected:
        SQLHSTMT m_stmt;             // ODBC statement handle for query execution
        RS_STMT_INFO* m_pStmt;       // Redshift-specific statement info wrapper
        const char* m_operationName; // Operation identifier for logging/debugging

        ShowDiscoveryBase(SQLHSTMT stmt, const char *operationName);
        SQLRETURN prepareBindAndExecuteQuery(const std::string& query, 
                                           const std::vector<std::string> &inputParameters);
        std::string getErrorMessage(SQLHSTMT phstmt);
        SQLRETURN columnBindingHelper(std::vector<ColumnBinding>& bindings);

        /**
         * @class ColumnBindingCleanup
         * @brief RAII guard for automatic ODBC column binding cleanup
         * 
         * Ensures column bindings are properly released both on construction
         * and destruction, preventing resource leaks and binding conflicts.
         * Uses constructor/destructor pattern for exception-safe cleanup.
         */
        class ColumnBindingCleanup {
            SQLHSTMT& m_stmt;
        public:
            explicit ColumnBindingCleanup(SQLHSTMT& stmt);
            ~ColumnBindingCleanup();
        private:
            void cleanup();
        };
    public:
         virtual SQLRETURN execute() = 0;
    };

    /**
     * @class ShowDatabasesHelper
     * @brief Concrete implementation for SHOW DATABASES metadata operations
     * 
     * Handles database/catalog enumeration with support for:
     * - Pattern matching (LIKE clause) when catalog filter is provided
     * - Single-database metadata mode (returns only current database)
     * - Full catalog listing when no filter is specified
     * 
     */
    class ShowDatabasesHelper : private ShowDiscoveryBase {
        const std::string& m_catalog;
        std::vector<std::string>& m_catalogs;
        bool m_isSingleDatabaseMetaData;
    public:
        ShowDatabasesHelper(SQLHSTMT stmt, const std::string& catalog,
                          std::vector<std::string>& catalogs,
                          bool isSingleDatabaseMetaData);
        SQLRETURN execute() override;
    };

    /**
     * @class ShowSchemasHelper
     * @brief Concrete implementation for SHOW SCHEMAS metadata operations
     * 
     * Retrieves schema information for a specified catalog with optional
     * schema name pattern matching.
     */
    class ShowSchemasHelper : private ShowDiscoveryBase {
        const std::string& m_catalog;
        const std::string& m_schema;
        std::vector<SHOWSCHEMASResult> &m_intermediateRS;
    public:
        ShowSchemasHelper(SQLHSTMT phstmt, const std::string &catalog,
                        const std::string &schema,
                        std::vector<SHOWSCHEMASResult> &intermediateRS);

        SQLRETURN execute() override;
    };

    /**
     * @class ShowTablesHelper
     * @brief Concrete implementation for SHOW TABLES metadata operations
     * 
     * Retrieves table information for a specified catalog & schema with
     * optional table name pattern matching.
     */
    class ShowTablesHelper : private ShowDiscoveryBase {
        const std::string& m_catalog;
        const std::string& m_schema;
        const std::string& m_table;
        std::vector<SHOWTABLESResult> &m_intermediateRS;
    public:
        ShowTablesHelper(SQLHSTMT phstmt, const std::string &catalog,
                        const std::string &schema, const std::string &table,
                        std::vector<SHOWTABLESResult> &intermediateRS);

        SQLRETURN execute() override;
    };

    /**
     * @class ShowColumnsHelper
     * @brief Concrete implementation for SHOW COLUMNS metadata operations
     * 
     * Retrieves column information for a specified catalog & schema & table
     * with optional column name pattern matching.
     */
    class ShowColumnsHelper : private ShowDiscoveryBase {
        const std::string& m_catalog;
        const std::string& m_schema;
        const std::string& m_table;
        const std::string& m_column;
        std::vector<SHOWCOLUMNSResult> &m_intermediateRS;
    public:
        ShowColumnsHelper(SQLHSTMT phstmt, const std::string &catalog,
                        const std::string &schema, const std::string &table,
                        const std::string &column,
                        std::vector<SHOWCOLUMNSResult> &intermediateRS);

        SQLRETURN execute() override;
    };

    /**
     * @class ShowConstraintsPkHelper
     * @brief Concrete implementation for SHOW CONSTRAINTS PRIMARY metadata operations
     * 
     * Retrieves primary keys information for a specified catalog & schema & table
     */
    class ShowConstraintsPkHelper : private ShowDiscoveryBase {
        const std::string& m_catalog;
        const std::string& m_schema;
        const std::string& m_table;
        std::vector<SHOWCONSTRAINTSPRIMARYKEYSResult> &m_intermediateRS;
    public:
        ShowConstraintsPkHelper(SQLHSTMT phstmt, const std::string &catalog,
                        const std::string &schema, const std::string &table,
                        std::vector<SHOWCONSTRAINTSPRIMARYKEYSResult> &intermediateRS);

        SQLRETURN execute() override;
    };

    /**
     * @class ShowConstraintsFkHelper
     * @brief Concrete implementation for SHOW CONSTRAINTS FOREIGN metadata operations
     * 
     * Retrieves foreign key constraint information for a specified catalog & schema
     * & table, supporting both imported foreign keys (references from this table to others)
     * and exported foreign keys (references from other tables to this table).
     */
    class ShowConstraintsFkHelper : private ShowDiscoveryBase {
        const std::string& m_catalog;
        const std::string& m_schema;
        const std::string& m_table;
        const bool isExported;
        std::vector<SHOWCONSTRAINTSFOREIGNKEYSResult> &m_intermediateRS;
    public:
        ShowConstraintsFkHelper(
            SQLHSTMT phstmt, const std::string &catalog,
            const std::string &schema, const std::string &table, bool isExported,
            std::vector<SHOWCONSTRAINTSFOREIGNKEYSResult> &intermediateRS);

        SQLRETURN execute() override;
    };

    /**
     * @class ShowGrantsColumnHelper
     * @brief Concrete implementation for SHOW COLUMN GRANTS metadata operations
     * 
     * Retrieves column privileges information for a specified catalog & schema
     * & table with optional column name pattern matching.
     */
    class ShowGrantsColumnHelper : private ShowDiscoveryBase {
        const std::string& m_catalog;
        const std::string& m_schema;
        const std::string& m_table;
        const std::string& m_column;
        std::vector<SHOWGRANTSCOLUMNResult> &m_intermediateRS;
    public:
        ShowGrantsColumnHelper(SQLHSTMT phstmt, const std::string &catalog,
                        const std::string &schema, const std::string &table,
                        const std::string &column,
                        std::vector<SHOWGRANTSCOLUMNResult> &intermediateRS);

        SQLRETURN execute() override;
    };

    /**
     * @class ShowGrantsTableHelper
     * @brief Concrete implementation for SHOW GRANTS metadata operations
     * 
     * Retrieves table privileges information for a specified catalog & schema
     * & table.
     */
    class ShowGrantsTableHelper : private ShowDiscoveryBase {
        const std::string& m_catalog;
        const std::string& m_schema;
        const std::string& m_table;
        std::vector<SHOWGRANTSTABLEResult> &m_intermediateRS;
    public:
        ShowGrantsTableHelper(SQLHSTMT phstmt, const std::string &catalog,
                        const std::string &schema, const std::string &table,
                        std::vector<SHOWGRANTSTABLEResult> &intermediateRS);

        SQLRETURN execute() override;
    };

    /**
     * @class ShowProceduresFunctionsHelper
     * @brief Concrete implementation for SHOW PROCEDURES/FUNCTIONS metadata operations
     * 
     * Unified helper for retrieving stored procedure and user-defined function metadata
     * from a specified catalog and schema. Supports both complete enumeration and
     * pattern-based filtering of procedure/function names. Uses a single implementation
     * to handle both object types with runtime switching based on the isProcedures flag.
     */
    class ShowProceduresFunctionsHelper : private ShowDiscoveryBase {
        const std::string& m_catalog;
        const std::string& m_schema;
        const std::string& m_procedure_function;
        bool isProcedures;
        std::vector<SHOWPROCEDURESFUNCTIONSResult> &m_intermediateRS;
    public:
        ShowProceduresFunctionsHelper(SQLHSTMT phstmt, const std::string &catalog,
                        const std::string &schema, const std::string &procedure_function,
                        bool isProcedures,
                        std::vector<SHOWPROCEDURESFUNCTIONSResult> &intermediateRS);

        SQLRETURN execute() override;
    };

    /**
     * @class ShowProcedureFunctionColumnsHelper
     * @brief Concrete implementation for SHOW COLUMNS metadata operations on procedures/functions
     * 
     * Retrieves detailed column/parameter metadata for stored procedures and user-defined
     * functions, including data types, nullability, precision, and ordinal positions.
     * Supports both complete parameter enumeration and pattern-based filtering of
     * specific columns/parameters within a procedure or function.
     */
    class ShowProcedureFunctionColumnsHelper : private ShowDiscoveryBase {
        const std::string& m_catalog;
        const std::string& m_schema;
        const std::string& m_procedure_function;
        const std::string& m_column;
        const std::string& m_sqlQuery;
        const std::vector<std::string>& m_argumentList;
        bool isProcedure;
        std::vector<SHOWCOLUMNSResult> &m_intermediateRS;
    public:
        ShowProcedureFunctionColumnsHelper(SQLHSTMT phstmt, const std::string &catalog,
                        const std::string &schema, const std::string &procedure_function,
                        const std::string &column, const std::string &sqlQuery,
                        const std::vector<std::string>& argumentList, bool isProcedure,
                        std::vector<SHOWCOLUMNSResult> &intermediateRS);

        SQLRETURN execute() override;
    };
}

#endif // RS_METADATA_SERVER_PROXY_HELPERS_H