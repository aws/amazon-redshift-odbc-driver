/*-------------------------------------------------------------------------
 *
 * Copyright(c) 2024, Amazon.com, Inc. or Its Affiliates. All rights reserved.
 *
 * Author: stinghsu
 *-------------------------------------------------------------------------
 */

#pragma once

#ifndef __RS_METADATAAPIPOSTPROCESSING_H__

#define __RS_METADATAAPIPOSTPROCESSING_H__

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rsMetadataAPIHelper.h"
#include "rsdesc.h"
#include "rsodbc.h"
#include "rstrace.h"
#include "rsunicode.h"
#include "rsutil.h"


namespace PostProcessorLoggings {
    static const char* START_TRACE_MSG = "Start post-processing";
    static const char* END_TRACE_MSG = "End post-processing";
    static const char* INVALID_HANDLER = "Invalid statement handle provided";
    static const char* COLUMN_FIELD_INITIALIZE_FAILED = "Fail to initialize column field";
    static const char* RESULTSET_CREATION_FAILED = "Error when create customized result set";
}

/*----------------
 * RsMetadataAPIPostProcessor
 *
 * A class which contains helper function related to post-processing
 * ----------------
 */
class RsMetadataAPIPostProcessor {
  public:
    RsMetadataAPIPostProcessor(RS_STMT_INFO* stmt);
    static const RsMetadataAPIHelper metadataAPIHelper;

    /* ----------------
     * sqlCatalogsPostProcessing
     *
     * helper function to apply post-processing on intermediate result set for
     * SQLTables special call to retrieve catalog list
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   intermediateRS (const std::vector<std::string> &): intermediate result
     *     set from RsMetadataServerProxy::sqlCatalogs
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN
    sqlCatalogsPostProcessing(SQLHSTMT phstmt,
                              const std::vector<std::string> &intermediateRS);

    /* ----------------
     * sqlSchemasPostProcessing
     *
     * helper function to apply post-processing on intermediate result set for
     * SQLTables special call to retrieve schema list
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   intermediateRS (const std::vector<SHOWSCHEMASResult> &): intermediate
     *     result set from RsMetadataServerProxy::sqlSchemas
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN sqlSchemasPostProcessing(
        SQLHSTMT phstmt, const std::vector<SHOWSCHEMASResult> &intermediateRS);

    /* ----------------
     * sqlTableTypesPostProcessing
     *
     * helper function to apply post-processing on pre-defined table type list
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN sqlTableTypesPostProcessing(SQLHSTMT phstmt);

    /* ----------------
     * sqlTablesPostProcessing
     *
     * helper function to apply post-processing on intermediate result set for
     * SQLTables
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   intermediateRS (const std::vector<SHOWTABLESResult> &): intermediate
     *     result set from RsMetadataServerProxy::sqlTables
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN sqlTablesPostProcessing(
        SQLHSTMT phstmt, std::string pTableType,
        const std::vector<SHOWTABLESResult> &intermediateRS);

    /* ----------------
     * sqlColumnsPostProcessing
     *
     * helper function to apply post-processing on intermediate result set for
     * SQLColumns
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   intermediateRS (const std::vector<SHOWCOLUMNSResult> &): intermediate
     *     result set from RsMetadataServerProxy::sqlColumns
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN sqlColumnsPostProcessing(
        SQLHSTMT phstmt, const std::vector<SHOWCOLUMNSResult> &intermediateRS);

    /* ----------------
     * sqlPrimaryKeysPostProcessing
     *
     * helper function to apply post-processing on intermediate result set for
     * SQLPrimaryKeys
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   intermediateRS (const std::vector<SHOWCONSTRAINTSPRIMARYKEYSResult> &): 
     *     intermediate result set from RsMetadataServerProxy::sqlPrimaryKeys
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN sqlPrimaryKeysPostProcessing(
        SQLHSTMT phstmt, const std::vector<SHOWCONSTRAINTSPRIMARYKEYSResult> &intermediateRS);

    /* ----------------
     * sqlForeignKeysPostProcessing
     *
     * Helper function to apply post-processing on intermediate result set for
     * SQLForeignKeys
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   intermediateRS (const std::vector<SHOWCONSTRAINTSFOREIGNKEYSResult> &):
     *     intermediate result set to be processed
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN sqlForeignKeysPostProcessing(
        SQLHSTMT phstmt,
        const std::vector<SHOWCONSTRAINTSFOREIGNKEYSResult> &intermediateRS);

    /* ----------------
     * sqlSpecialColumnsPostProcessing
     *
     * Helper function to apply post-processing on intermediate result set for
     * SQLSpecialColumns
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   intermediateRS (const std::vector<SHOWCOLUMNSResult> &):
     *     intermediate result set to be processed
     *   identifierType: Type of identifier (SQL_BEST_ROWID or SQL_ROWVER)
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN sqlSpecialColumnsPostProcessing(
        SQLHSTMT phstmt,
        const std::vector<SHOWCOLUMNSResult> &intermediateRS,
        SQLUSMALLINT identifierType);

    /* ----------------
     * sqlColumnPrivilegesPostProcessing
     *
     * Helper function to apply post-processing on intermediate result set for
     * SQLColumnPrivileges
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   intermediateRS (const std::vector<SHOWGRANTSCOLUMNResult> &):
     *     intermediate result set to be processed
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN sqlColumnPrivilegesPostProcessing(
        SQLHSTMT phstmt,
        const std::vector<SHOWGRANTSCOLUMNResult> &intermediateRS);

    /* ----------------
     * sqlTablePrivilegesPostProcessing
     *
     * Helper function to apply post-processing on intermediate result set for
     * SQLTablePrivileges
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   intermediateRS (const std::vector<SHOWGRANTSTABLEResult> &):
     *     intermediate result set to be processed
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN sqlTablePrivilegesPostProcessing(
        SQLHSTMT phstmt,
        const std::vector<SHOWGRANTSTABLEResult> &intermediateRS);

    /* ----------------
     * sqlProceduresPostProcessing
     *
     * Helper function to apply post-processing on intermediate result set for
     * SQLProcedures
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   intermediateRS (const std::vector<SHOWPROCEDURESFUNCTIONSResult> &):
     *     intermediate result set to be processed
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN sqlProceduresPostProcessing(
        SQLHSTMT phstmt,
        const std::vector<SHOWPROCEDURESFUNCTIONSResult> &intermediateRS);

    /* ----------------
     * sqlProcedureColumnsPostProcessing
     *
     * Helper function to apply post-processing on intermediate result set for
     * SQLProcedureColumns
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   intermediateRS (const std::vector<SHOWCOLUMNSResult> &):
     *     intermediate result set to be processed
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN sqlProcedureColumnsPostProcessing(
        SQLHSTMT phstmt, const std::vector<SHOWCOLUMNSResult> &intermediateRS);
};

#endif // __RS_METADATAAPIPOSTPROCESSING_H__
