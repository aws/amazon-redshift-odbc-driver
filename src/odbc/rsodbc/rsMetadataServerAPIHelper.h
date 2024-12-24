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
#include "rsprepare.h"
#include "rsparameter.h"

/*----------------
 * RsMetadataServerAPIHelper
 *
 * A class which contains helper function related to Server API SHOW command
 * ----------------
 */
class RsMetadataServerAPIHelper {
  public:
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
     * callShowDatabases
     *
     * helper function to call SHOW DATABASES and return a list of catalog
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   catalog (const std::string &): catalog name / pattern
     *   catalogs (std::vector<std::string>): a vector to store catalog list
     *   isSingleDatabaseMetaData (bool): a boolean to determine returning
     *     result from single database or all database
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN callShowDatabases(SQLHSTMT phstmt, const std::string &catalog, std::vector<std::string> &catalogs, bool isSingleDatabaseMetaData);

    /* ----------------
     * callShowSchemas
     *
     * helper function to call SHOW SCHEMAS and return intermediate result set
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   catalog (const std::string &): Exact name of catalog
     *   schema (const std::string&): schema name / pattern
     *   intermediateRS (std::vector<SHOWSCHEMASResult> &): a vector to store
     *     intermediate result set for post-processing
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN
    callShowSchemas(SQLHSTMT phstmt, const std::string &catalog, const std::string &schema, 
                     std::vector<SHOWSCHEMASResult> &intermediateRS);

    /* ----------------
     * callShowTables
     *
     * helper function to call SHOW TABLES and return intermediate result set
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   catalog (const std::string &): Exact name of catalog
     *   schema (const std::string &): Exact name of schema
     *   table (const std::string &): table name / pattern
     *   intermediateRS (std::vector<SHOWTABLESResult> &): a vector to store
     *     intermediate result set for post-processing
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN
    callShowTables(SQLHSTMT phstmt, const std::string &catalog,
                    const std::string &schema, const std::string &table,
                    std::vector<SHOWTABLESResult> &intermediateRS);

    /* ----------------
     * callShowColumns
     *
     * helper function to call SHOW COLUMNS and return intermediate result set
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   catalog (const std::string &): Exact name of catalog
     *   schema (const std::string &): Exact name of schema
     *   table (const std::string &): Exact name of table
     *   column (const std::string &): column name / pattern
     *   intermediateRS (std::vector<SHOWCOLUMNSResult> &): a vector to store
     *     intermediate result set for post-processing
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN
    callShowColumns(SQLHSTMT phstmt, const std::string &catalog,
                     const std::string &schema, const std::string &table,
                     const std::string &column,
                     std::vector<SHOWCOLUMNSResult> &intermediateRS);

    /* ----------------
     * callQuoteFunc
     *
     * helper function to call QUOTE* function to do proper quoting
     * and escaping for identifier and literal
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   input (const std::string &): the input string to be quoted
     *   output (const std::string &): quoted input string
     *   quotedQuery (const std::string &): sql query for QUOTE function
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN callQuoteFunc(SQLHSTMT phstmt, const std::string &input, std::string &output, const std::string &quotedQuery);
};

#endif // __METADATASERVERAPIHELPER_H__