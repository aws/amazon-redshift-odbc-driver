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

/*----------------
 * RsMetadataAPIPostProcessing
 *
 * A class which contains helper function related to post-processing
 * ----------------
 */
class RsMetadataAPIPostProcessing {
  public:
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
     *     set from RsMetadataServerAPIHelper::sqlCatalogsServerAPI
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
     *     result set from RsMetadataServerAPIHelper::sqlSchemasServerAPI
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
     *   retEmpty (bool): a boolean to determine return empty result set
     *     directly without calling any Server API SHOW command
     *   intermediateRS (const std::vector<SHOWTABLESResult> &): intermediate
     *     result set from RsMetadataServerAPIHelper::sqlTablesServerAPI
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN sqlTablesPostProcessing(
        SQLHSTMT phstmt, std::string pTableType, bool retEmpty,
        const std::vector<SHOWTABLESResult> &intermediateRS);

    /* ----------------
     * sqlColumnsPostProcessing
     *
     * helper function to apply post-processing on intermediate result set for
     * SQLColumns
     *
     * Parameters:
     *   phstmt (SQLHSTMT): statement handler
     *   retEmpty (bool): a boolean to determine return empty result set
     *     directly without calling any Server API SHOW command
     *   intermediateRS (const std::vector<SHOWCOLUMNSResult> &): intermediate
     *     result set from RsMetadataServerAPIHelper::sqlColumnsServerAPI
     *
     * Return:
     *   SQLRETURN
     * ----------------
     */
    static SQLRETURN sqlColumnsPostProcessing(
        SQLHSTMT phstmt, bool retEmpty,
        const std::vector<SHOWCOLUMNSResult> &intermediateRS);
};

#endif // __RS_METADATAAPIPOSTPROCESSING_H__