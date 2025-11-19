/*-------------------------------------------------------------------------
 *
 * Copyright(c) 2024, Amazon.com, Inc. or Its Affiliates. All rights reserved.
 *
 * Author: stinghsu
 *-------------------------------------------------------------------------
 */

#pragma once

#ifndef __RS_METADATASERVERAPIHELPER_H__

#define __RS_METADATASERVERAPIHELPER_H__

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rsMetadataAPIHelper.h"
#include "rsMetadataServerProxyHelper.h"
#include "rsodbc.h"
#include "rstrace.h"
#include "rsunicode.h"
#include "rsutil.h"
#include "rsprepare.h"
#include "rsparameter.h"

/*----------------
 * RsMetadataServerProxy
 *
 * A class which contains helper function related to Server API SHOW command
 * ----------------
 */
class RsMetadataServerProxy {
  public:
    /* ----------------
     * sqlCatalogs
     *
     * helper function to return intermediate result set for SQLTables special
     * call to retrieve a list of catalog
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   intermediateRS (std::vector<std::string> &): a vector to store
     *     intermediate result set for post-processing
     *   isSingleDatabaseMetaData (bool): a boolean to determine returning
     *     result from single database or all database
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN
    sqlCatalogs(SQLHSTMT phstmt,
                         std::vector<std::string> &intermediateRS,
                         bool isSingleDatabaseMetaData);

    /* ----------------
     * sqlSchemas
     *
     * helper function to return intermediate result set for SQLTables special
     * call to retrieve a list of schema
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   intermediateRS (std::vector<SHOWSCHEMASResult> &): a vector to store
     *     intermediate result set for post-processing
     *   isSingleDatabaseMetaData (bool): a boolean to determine returning
     *     result from single database or all database
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN
    sqlSchemas(SQLHSTMT phstmt,
                        std::vector<SHOWSCHEMASResult> &intermediateRS,
                        bool isSingleDatabaseMetaData);

    /* ----------------
     * sqlTables
     *
     * helper function to return intermediate result set for SQLTables
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   catalogName (const std::string &): catalog name (pattern)
     *   schemaName (const std::string &): schema name (pattern)
     *   tableName (const std::string &): table name (pattern)
     *   intermediateRS (std::vector<SHOWTABLESResult> &): a vector to store
     *     intermediate result set for post-processing
     *   isSingleDatabaseMetaData (bool): a boolean to determine returning
     *     result from single database or all database
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN
    sqlTables(SQLHSTMT phstmt, const std::string &catalogName,
                       const std::string &schemaName,
                       const std::string &tableName,
                       std::vector<SHOWTABLESResult> &intermediateRS,
                       bool isSingleDatabaseMetaData);

    /* ----------------
     * sqlColumns
     *
     * helper function to return intermediate result set for SQLColumns
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   catalogName (const std::string &): catalog name (pattern)
     *   schemaName (const std::string &): schema name (pattern)
     *   tableName (const std::string &): table name (pattern)
     *   columnName (const std::string &): column name (pattern)
     *   intermediateRS (std::vector<SHOWCOLUMNSResult> &): a vector to store
     *     intermediate result set for post-processing
     *   isSingleDatabaseMetaData (bool): a boolean to determine returning
     *     result from single database or all database
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN
    sqlColumns(SQLHSTMT phstmt, const std::string &catalogName,
                        const std::string &schemaName,
                        const std::string &tableName,
                        const std::string &columnName,
                        std::vector<SHOWCOLUMNSResult> &intermediateRS,
                        bool isSingleDatabaseMetaData);

    /* ----------------
     * sqlPrimaryKeys
     *
     * Helper function to return intermediate result set for SQLPrimaryKeys
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   catalogName (const std::string &): catalog name
     *   schemaName (const std::string &): schema name
     *   tableName (const std::string &): table name
     *   intermediateRS (std::vector<SHOWCONSTRAINTSPRIMARYKEYSResult> &): a vector to store
     *     intermediate result set for post-processing
     *   isSingleDatabaseMetaData (bool): a boolean to determine returning
     *     result from single database or all database
     *
     * Return:
     *   SQLRETURN: SQL_SUCCESS if successful, SQL_ERROR if failed
     *
     * Notes:
     *   - Returns primary key columns for the specified table(s)
     *   - If catalog/schema/table names are empty, searches all respective objects
     *   - Results include column names, key sequence numbers, and constraint names
     * ----------------
     */
    static SQLRETURN
    sqlPrimaryKeys(SQLHSTMT phstmt, const std::string &catalogName,
                  const std::string &schemaName,
                  const std::string &tableName,
                  std::vector<SHOWCONSTRAINTSPRIMARYKEYSResult> &intermediateRS,
                  bool isSingleDatabaseMetaData);

    /* ----------------
     * sqlForeignKeys
     *
     * Helper function to return intermediate result set for SQLForeignKeys
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   pkCatalogName (const std::string &): primary key table's catalog name
     *   pkSchemaName (const std::string &): primary key table's schema name
     *   pkTableName (const std::string &): primary key table's name
     *   fkCatalogName (const std::string &): foreign key table's catalog name
     *   fkSchemaName (const std::string &): foreign key table's schema name
     *   fkTableName (const std::string &): foreign key table's name
     *   intermediateRS (std::vector<SHOWCONSTRAINTSFOREIGNKEYSResult> &): a vector to store
     *     intermediate result set for post-processing
     *   isSingleDatabaseMetaData (bool): a boolean to determine returning
     *     result from single database or all database
     *
     * Return:
     *   SQLRETURN: SQL_SUCCESS if successful, SQL_ERROR if failed
     *
     * Notes:
     *   - Three usage patterns:
     *     1. If only pkTableName specified: returns exported keys (FKs that reference this table)
     *     2. If only fkTableName specified: returns imported keys (FKs in this table)
     *     3. If both specified: returns the specific relationship between the tables
     *   - Results include both PK and FK column information, constraint names, and update/delete rules
     * ----------------
     */
    static SQLRETURN
    sqlForeignKeys(SQLHSTMT phstmt,
                  const std::string &pkCatalogName,
                  const std::string &pkSchemaName,
                  const std::string &pkTableName,
                  const std::string &fkCatalogName,
                  const std::string &fkSchemaName,
                  const std::string &fkTableName,
                  std::vector<SHOWCONSTRAINTSFOREIGNKEYSResult> &intermediateRS,
                  bool isSingleDatabaseMetaData);

    /* ----------------
     * sqlSpecialColumns
     *
     * Helper function to return intermediate result set for SQLSpecialColumns
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   catalogName (const std::string &): catalog name
     *   schemaName (const std::string &): schema name
     *   tableName (const std::string &): table name
     *   intermediateRS (std::vector<SHOWCOLUMNSResult> &): a vector to store
     *     intermediate result set for post-processing
     *   isSingleDatabaseMetaData (bool): a boolean to determine returning
     *     result from single database or all database
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN
    sqlSpecialColumns(SQLHSTMT phstmt,
                    const std::string &catalogName,
                    const std::string &schemaName,
                    const std::string &tableName,
                    std::vector<SHOWCOLUMNSResult> &intermediateRS,
                    bool isSingleDatabaseMetaData);

    /* ----------------
     * sqlColumnPrivileges
     *
     * Helper function to return intermediate result set for SQLColumnPrivileges
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   catalogName (const std::string &): catalog name
     *   schemaName (const std::string &): schema name
     *   tableName (const std::string &): table name
     *   columnName (const std::string &): column name
     *   intermediateRS (std::vector<SHOWGRANTSCOLUMNResult> &): a vector to store
     *     intermediate result set for post-processing
     *   isSingleDatabaseMetaData (bool): a boolean to determine returning
     *     result from single database or all database
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN
    sqlColumnPrivileges(SQLHSTMT phstmt, const std::string &catalogName,
                      const std::string &schemaName,
                      const std::string &tableName,
                      const std::string &columnName,
                      std::vector<SHOWGRANTSCOLUMNResult> &intermediateRS,
                      bool isSingleDatabaseMetaData);

    /* ----------------
     * sqlTablePrivileges
     *
     * Helper function to return intermediate result set for SQLTablePrivileges
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   catalogName (const std::string &): catalog name
     *   schemaName (const std::string &): schema name
     *   tableName (const std::string &): table name
     *   intermediateRS (std::vector<SHOWGRANTSTABLEResult> &): a vector to store
     *     intermediate result set for post-processing
     *   isSingleDatabaseMetaData (bool): a boolean to determine returning
     *     result from single database or all database
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN
    sqlTablePrivileges(SQLHSTMT phstmt, const std::string &catalogName,
                      const std::string &schemaName,
                      const std::string &tableName,
                      std::vector<SHOWGRANTSTABLEResult> &intermediateRS,
                      bool isSingleDatabaseMetaData);

    /* ----------------
     * sqlProcedures
     *
     * Helper function to return intermediate result set for SQLProcedures
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   catalogName (const std::string &): catalog name
     *   schemaName (const std::string &): schema name
     *   procName (const std::string &): procedure name
     *   intermediateRS (std::vector<SHOWPROCEDURESFUNCTIONSResult> &): Vector to store
     *     intermediate result set containing both procedures and functions.
     *     Each entry includes catalog, schema, name, and type
     *     (SQL_PT_PROCEDURE or SQL_PT_FUNCTION)
     *   isSingleDatabaseMetaData (bool): a boolean to determine returning
     *     result from single database or all database
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN
    sqlProcedures(SQLHSTMT phstmt, const std::string &catalogName,
                  const std::string &schemaName,
                  const std::string &procName,
                  std::vector<SHOWPROCEDURESFUNCTIONSResult> &intermediateRS,
                  bool isSingleDatabaseMetaData);

    /* ----------------
     * sqlProcedureColumns
     *
     * Helper function to return intermediate result set for SQLProcedureColumns
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   catalogName (const std::string &): catalog name
     *   schemaName (const std::string &): schema name
     *   procName (const std::string &): procedure name
     *   columnName (const std::string &): column name
     *   intermediateRS (std::vector<SHOWCOLUMNSResult> &): a vector to store
     *     intermediate result set for post-processing
     *   isSingleDatabaseMetaData (bool): a boolean to determine returning
     *     result from single database or all database
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN
    sqlProcedureColumns(SQLHSTMT phstmt, const std::string &catalogName,
                      const std::string &schemaName,
                      const std::string &procName,
                      const std::string &columnName,
                      std::vector<SHOWCOLUMNSResult> &intermediateRS,
                      bool isSingleDatabaseMetaData);

  private:
    static SQLRETURN processKeysCase(
        SQLHSTMT phstmt,
        bool isExportedKeys,
        const std::string& catalogName,
        const std::string& schemaName,
        const std::string& tableName,
        std::vector<SHOWCONSTRAINTSFOREIGNKEYSResult>& intermediateRS,
        bool isSingleDatabaseMetaData);

    /**
      * @brief Checks if foreign key constraint metadata matches specified catalog/schema/table criteria
      *
      * Performs case-insensitive comparison of foreign key constraint metadata against
      * provided primary key identifiers. Empty criteria match any value.
      *
      * @param result The foreign key constraint metadata result to check
      * @param pkCatalog Primary key's catalog name to match against (empty matches any)
      * @param pkSchema Primary key's schema name to match against (empty matches any)
      * @param pkTable Primary key's table name to match against (empty matches any)
      *
      * @return true if the constraint metadata matches all non-empty criteria,
      *         false otherwise
      *
      * @note Comparisons are case-insensitive to match Redshift's identifier behavior
      * @note Maximum identifier length is limited to NAMEDATALEN-1 (63) bytes
    */
    static bool matchesConstraints(
        const SHOWCONSTRAINTSFOREIGNKEYSResult& result,
        const std::string &pkCatalog,
        const std::string &pkSchema,
        const std::string &pkTable);

    /**
     * Validates the lengths of provided names against maximum allowed length (NAMEDATALEN)
     *
     * @param validations A vector of pairs where:
     *                    - first: identifier/description of the name being validated
     *                    - second: actual name string to validate
     *
     * @return SQL_SUCCESS if all name lengths are valid
     *         SQL_ERROR if any name exceeds NAMEDATALEN
     */
    static SQLRETURN validateNameLengths(const std::vector<std::pair<std::string, std::string>>& validations);
};

#endif // __METADATASERVERAPIHELPER_H__
