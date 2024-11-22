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
#include "rsodbc.h"
#include "rstrace.h"
#include "rsunicode.h"
#include "rsutil.h"

/*----------------
 * RsMetadataServerAPIHelper
 *
 * A class which contains helper function related to Server API SHOW command
 * ----------------
 */
class RsMetadataServerAPIHelper {
  public:
    /* ----------------
     * getCatalogs
     *
     * helper function to retrieve a list of catalog for given catalog name
     * (pattern)
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   catalogName (const std::string &): catalog name (pattern)
     *   catalogs (std::vector<std::string> &): the vector to store the
     *     result
     *   isSingleDatabaseMetaData (bool): a boolean to determine returning
     *     result from single database or all database
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN getCatalogs(SQLHSTMT phstmt,
                                 const std::string &catalogName,
                                 std::vector<std::string> &catalogs,
                                 bool isSingleDatabaseMetaData);

    /* ----------------
     * getSchemas
     *
     * helper function to retrieve a list of schema for given catalog name and
     * schema name (pattern)
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   catalogName (const std::string &): catalog name
     *   schemaName (const std::string &): schema name (pattern)
     *   schemas (std::vector<std::string> &): the vector to store the
     *     result
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN getSchemas(SQLHSTMT phstmt, const std::string &catalogName,
                                const std::string &schemaName,
                                std::vector<std::string> &schemas);

    /* ----------------
     * getTables
     *
     * helper function to retrieve a list of table for given catalog name,
     * schema name and table name (pattern)
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   catalogName (const std::string &): catalog name
     *   schemaName (const std::string &): schema name
     *   tableName (const std::string &): table name (pattern)
     *   tables (std::vector<std::string> &): the vector to store the
     *     result
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN getTables(SQLHSTMT phstmt, const std::string &catalogName,
                               const std::string &schemaName,
                               const std::string &tableName,
                               std::vector<std::string> &tables);

    /* ----------------
     * sqlCatalogsServerAPI
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
    sqlCatalogsServerAPI(SQLHSTMT phstmt,
                         std::vector<std::string> &intermediateRS,
                         bool isSingleDatabaseMetaData);

    /* ----------------
     * sqlSchemasServerAPI
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
    sqlSchemasServerAPI(SQLHSTMT phstmt,
                        std::vector<SHOWSCHEMASResult> &intermediateRS,
                        bool isSingleDatabaseMetaData);

    /* ----------------
     * sqlTablesServerAPI
     *
     * helper function to return intermediate result set for SQLTables
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   catalogName (const std::string &): catalog name (pattern)
     *   schemaName (const std::string &): schema name (pattern)
     *   tableName (const std::string &): table name (pattern)
     *   retEmpty (bool): a boolean to determine return empty result set
     *     directly without calling any Server API SHOW command
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
    sqlTablesServerAPI(SQLHSTMT phstmt, const std::string &catalogName,
                       const std::string &schemaName,
                       const std::string &tableName, bool retEmpty,
                       std::vector<SHOWTABLESResult> &intermediateRS,
                       bool isSingleDatabaseMetaData);

    /* ----------------
     * sqlColumnsServerAPI
     *
     * helper function to return intermediate result set for SQLColumns
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   catalogName (const std::string &): catalog name (pattern)
     *   schemaName (const std::string &): schema name (pattern)
     *   tableName (const std::string &): table name (pattern)
     *   columnName (const std::string &): column name (pattern)
     *   retEmpty (bool): a boolean to determine return empty result set
     *     directly without calling any Server API SHOW command
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
    sqlColumnsServerAPI(SQLHSTMT phstmt, const std::string &catalogName,
                        const std::string &schemaName,
                        const std::string &tableName,
                        const std::string &columnName, bool retEmpty,
                        std::vector<SHOWCOLUMNSResult> &intermediateRS,
                        bool isSingleDatabaseMetaData);

    /* ----------------
     * call_show_schema
     *
     * helper function to call SHOW SCHEMAS and return a full result instead of
     * just return a list of schema
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   catalog (const std::string &): Exact name of catalog
     *   intermediateRS (std::vector<SHOWSCHEMASResult> &): a vector to store
     *     intermediate result set for post-processing
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN
    call_show_schema(SQLHSTMT phstmt, const std::string &catalog,
                     std::vector<SHOWSCHEMASResult> &intermediateRS);

    /* ----------------
     * call_show_table
     *
     * helper function to call SHOW TABLES and return a full result instead of
     * just return a list of table
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   catalog (const std::string &): Exact name of catalog
     *   schema (const std::string &): Exact name of schema
     *   table (const std::string &): Exact name of table
     *   intermediateRS (std::vector<SHOWTABLESResult> &): a vector to store
     *     intermediate result set for post-processing
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN
    call_show_table(SQLHSTMT phstmt, const std::string &catalog,
                    const std::string &schema, const std::string &table,
                    std::vector<SHOWTABLESResult> &intermediateRS);

    /* ----------------
     * call_show_column
     *
     * helper function to call SHOW COLUMNS and return a full result instead of
     * just return a list of column
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   catalog (const std::string &): Exact name of catalog
     *   schema (const std::string &): Exact name of schema
     *   table (const std::string &): Exact name of table
     *   column (const std::string &): Exact name of column
     *   intermediateRS (std::vector<SHOWCOLUMNSResult> &): a vector to store
     *     intermediate result set for post-processing
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN
    call_show_column(SQLHSTMT phstmt, const std::string &catalog,
                     const std::string &schema, const std::string &table,
                     const std::string &column,
                     std::vector<SHOWCOLUMNSResult> &intermediateRS);
};

#endif // __METADATASERVERAPIHELPER_H__