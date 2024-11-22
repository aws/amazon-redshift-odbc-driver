/*-------------------------------------------------------------------------
 *
 * Copyright(c) 2024, Amazon.com, Inc. or Its Affiliates. All rights reserved.
 *
 * Author: stinghsu
 *-------------------------------------------------------------------------
 */

#include "rsMetadataAPIPostProcessing.h"
#include "rsexecute.h"
#include "rsutil.h"

//
// Helper function to apply post-processing on intermediate result set for
// SQLTables special call to retrieve catalog list
//
SQLRETURN RsMetadataAPIPostProcessing::sqlCatalogsPostProcessing(
    SQLHSTMT phstmt, const std::vector<std::string> &intermediateRS) {
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    RS_LOG_TRACE("rsMetadataAPIPostProcessing",
                 "Calling sqlCatalogsPostProcessing");

    // Column number and column name are same as SQLTables
    rc = metadataAPIHelper.initializeColumnField(
        pStmt, (char **)metadataAPIHelper.kTablesCol, kTablesColNum,
        (int *)metadataAPIHelper.kTablesColDatatype);
    if (rc != SQL_SUCCESS) {
        addError(
            &pStmt->pErrorList, "HY000",
            "sqlCatalogsPostProcessing: Fail to initialize column field for "
            "SQLTables ... ",
            0, NULL);
        return rc;
    }

    // Apply post-processing
    rc = libpqCreateSQLCatalogsCustomizedResultSet(pStmt, kTablesColNum,
                                                   intermediateRS);
    if (rc != SQL_SUCCESS) {
        addError(
            &pStmt->pErrorList, "HY000",
            "sqlCatalogsPostProcessing: Error when create customized result "
            "set ... ",
            0, NULL);
        return rc;
    }

    RS_LOG_TRACE("rsMetadataAPIPostProcessing",
                 "sqlCatalogsPostProcessing done");
    return rc;
}

//
// Helper function to apply post-processing on intermediate result set for
// SQLTables special call to retrieve schema list
//
SQLRETURN RsMetadataAPIPostProcessing::sqlSchemasPostProcessing(
    SQLHSTMT phstmt, const std::vector<SHOWSCHEMASResult> &intermediateRS) {
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    RS_LOG_TRACE("rsMetadataAPIPostProcessing",
                 "Calling sqlSchemasPostProcessing");

    // Column number and column name are same as SQLTables
    rc = metadataAPIHelper.initializeColumnField(
        pStmt, (char **)metadataAPIHelper.kTablesCol, kTablesColNum,
        (int *)metadataAPIHelper.kTablesColDatatype);
    if (rc != SQL_SUCCESS) {
        addError(
            &pStmt->pErrorList, "HY000",
            "sqlSchemasPostProcessing: Fail to initialize column field for "
            "SQLTables ... ",
            0, NULL);
        return rc;
    }

    // Apply post-processing
    rc = libpqCreateSQLSchemasCustomizedResultSet(pStmt, kTablesColNum,
                                                  intermediateRS);
    if (rc != SQL_SUCCESS) {
        addError(
            &pStmt->pErrorList, "HY000",
            "sqlSchemasPostProcessing: Error when create customized result "
            "set ... ",
            0, NULL);
        return rc;
    }

    RS_LOG_TRACE("rsMetadataAPIPostProcessing",
                 "sqlSchemasPostProcessing done");
    return rc;
}

//
// Helper function to apply post-processing on pre-defined table type list
//
SQLRETURN
RsMetadataAPIPostProcessing::sqlTableTypesPostProcessing(SQLHSTMT phstmt) {
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    RS_LOG_TRACE("rsMetadataAPIPostProcessing",
                 "Calling sqlTableTypesPostProcessing");

    // Column number and column name are same as SQLTables
    rc = metadataAPIHelper.initializeColumnField(
        pStmt, (char **)metadataAPIHelper.kTablesCol, kTablesColNum,
        (int *)metadataAPIHelper.kTablesColDatatype);
    if (rc != SQL_SUCCESS) {
        addError(
            &pStmt->pErrorList, "HY000",
            "sqlTableTypesPostProcessing: Fail to initialize column field for "
            "SQLTables ... ",
            0, NULL);
        return rc;
    }

    // Apply post-processing
    rc = libpqCreateSQLTableTypesCustomizedResultSet(
        pStmt, kTablesColNum, RsMetadataAPIHelper::tableTypeList);
    if (rc != SQL_SUCCESS) {
        addError(
            &pStmt->pErrorList, "HY000",
            "sqlTableTypesPostProcessing: Error when create customized result "
            "set ... ",
            0, NULL);
        return rc;
    }

    RS_LOG_TRACE("rsMetadataAPIPostProcessing",
                 "sqlTableTypesPostProcessing done");
    return rc;
}

//
// Helper function to apply post-processing on intermediate result set for
// SQLTables
//
SQLRETURN RsMetadataAPIPostProcessing::sqlTablesPostProcessing(
    SQLHSTMT phstmt, std::string pTableType, bool retEmpty,
    const std::vector<SHOWTABLESResult> &intermediateRS) {
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    RS_LOG_TRACE("rsMetadataAPIPostProcessing",
                 "Calling sqlTablesPostProcessing");

    rc = metadataAPIHelper.initializeColumnField(
        pStmt, (char **)metadataAPIHelper.kTablesCol, kTablesColNum,
        (int *)metadataAPIHelper.kTablesColDatatype);
    if (rc != SQL_SUCCESS) {
        addError(&pStmt->pErrorList, "HY000",
                 "sqlTablesPostProcessing: Fail to initialize column field for "
                 "SQLTables ... ",
                 0, NULL);
        return rc;
    }
    // Directly return empty Result
    if (retEmpty) {
        RS_LOG_DEBUG("sqlTablesPostProcessing",
                     "Directly return empty result set");
        rc = libpqCreateEmptyResultSet(pStmt, kTablesColNum,
                                       metadataAPIHelper.kTablesColDatatype);
        if (rc != SQL_SUCCESS) {
            addError(&pStmt->pErrorList, "HY000",
                     "sqlTablesPostProcessing: Error when create empty result "
                     "set ... ",
                     0, NULL);
            return rc;
        }
        return rc;
    }

    // Apply post-processing
    rc = libpqCreateSQLTablesCustomizedResultSet(pStmt, kTablesColNum,
                                                 pTableType, intermediateRS);
    if (rc != SQL_SUCCESS) {
        addError(&pStmt->pErrorList, "HY000",
                 "sqlTablesPostProcessing: Error when create customized result "
                 "set ... ",
                 0, NULL);
        return rc;
    }

    RS_LOG_TRACE("rsMetadataAPIPostProcessing", "sqlTablesPostProcessing done");
    return rc;
}

//
// Helper function to apply post-processing on intermediate result set for
// SQLColumns
//
SQLRETURN RsMetadataAPIPostProcessing::sqlColumnsPostProcessing(
    SQLHSTMT phstmt, bool retEmpty,
    const std::vector<SHOWCOLUMNSResult> &intermediateRS) {
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    RS_LOG_TRACE("rsMetadataAPIPostProcessing",
                 "Calling sqlColumnsPostProcessing");

    rc = metadataAPIHelper.initializeColumnField(
        pStmt, (char **)metadataAPIHelper.kColumnsCol, kColumnsColNum,
        (int *)metadataAPIHelper.kColumnsColDatatype);
    if (rc != SQL_SUCCESS) {
        addError(&pStmt->pErrorList, "HY000",
                 "sqlColumnsPostProcessing: Fail to initialize column field "
                 "for SQLColumns ... ",
                 0, NULL);
        return rc;
    }

    // Directly return empty Result
    if (retEmpty) {
        RS_LOG_DEBUG("sqlColumnsPostProcessing",
                     "Directly return empty result set");
        rc = libpqCreateEmptyResultSet(pStmt, kColumnsColNum,
                                       metadataAPIHelper.kColumnsColDatatype);
        return rc;
    }

    // Apply post-processing
    rc = libpqCreateSQLColumnsCustomizedResultSet(pStmt, kColumnsColNum,
                                                  intermediateRS);
    if (rc != SQL_SUCCESS) {
        addError(&pStmt->pErrorList, "HY000",
                 "sqlColumnsPostProcessing: Error when create customized "
                 "result set ... ",
                 0, NULL);
        return rc;
    }

    RS_LOG_TRACE("rsMetadataAPIPostProcessing",
                 "sqlColumnsPostProcessing Done");

    return rc;
}