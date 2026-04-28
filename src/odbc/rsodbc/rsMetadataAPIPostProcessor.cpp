/*-------------------------------------------------------------------------
 *
 * Copyright(c) 2024, Amazon.com, Inc. or Its Affiliates. All rights reserved.
 *
 * Author: stinghsu
 *-------------------------------------------------------------------------
 */

#include "rsMetadataAPIPostProcessor.h"
#include "rsexecute.h"
#include "rsutil.h"

RsMetadataAPIPostProcessor::RsMetadataAPIPostProcessor(RS_STMT_INFO* stmt) {
    // Initialize column names based on ODBC version
    RsMetadataAPIHelper::initializeColumnNames(stmt);
}

//
// Helper function to apply post-processing on intermediate result set for
// SQLGetTypeInfo
//
SQLRETURN RsMetadataAPIPostProcessor::sqlGetTypeInfoPostProcessing(
    SQLHSTMT phstmt, const std::vector<RS_TYPE_INFO> &typeInfo) {

    RS_LOG_TRACE("sqlGetTypeInfoPostProcessing", "%s", PostProcessorLoggings::START_TRACE_MSG);
    if (!VALID_HSTMT(phstmt)) {
        RS_LOG_ERROR("sqlGetTypeInfoPostProcessing", "%s", PostProcessorLoggings::INVALID_HANDLER);
        return SQL_INVALID_HANDLE;
    }

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    // Initialize column names based on ODBC version
    RsMetadataAPIHelper::initializeColumnNames(pStmt);

    // Initialize column fields for getTypeInfo
    SQLRETURN rc = metadataAPIHelper.initializeColumnField(
        pStmt,
        (char **)metadataAPIHelper.kTypeInfoCol,
        kTypeInfoColNum,
        (int *)metadataAPIHelper.kTypeInfoColDatatype);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlGetTypeInfoPostProcessing", "%s",
            PostProcessorLoggings::COLUMN_FIELD_INITIALIZE_FAILED);
        return rc;
    }

    // Apply post-processing
    rc = libpqCreateSQLGetTypeInfoCustomizedResultSet(pStmt, kTypeInfoColNum,
                                                   typeInfo);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlGetTypeInfoPostProcessing", "%s",
                     PostProcessorLoggings::RESULTSET_CREATION_FAILED);
        return rc;
    }

    RS_LOG_TRACE("sqlGetTypeInfoPostProcessing", "%s", PostProcessorLoggings::END_TRACE_MSG);
    return rc;
}

//
// Helper function to apply post-processing on intermediate result set for
// SQLTables special call to retrieve catalog list
//
SQLRETURN RsMetadataAPIPostProcessor::sqlCatalogsPostProcessing(
    SQLHSTMT phstmt, const std::vector<std::string> &intermediateRS) {

    RS_LOG_TRACE("sqlCatalogsPostProcessing", "%s", PostProcessorLoggings::START_TRACE_MSG);
    if (!VALID_HSTMT(phstmt)) {
        RS_LOG_ERROR("sqlCatalogsPostProcessing", "%s", PostProcessorLoggings::INVALID_HANDLER);
        return SQL_INVALID_HANDLE;
    }

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    // Initialize column names based on ODBC version
    RsMetadataAPIHelper::initializeColumnNames(pStmt);

    // Initialize column fields for table privileges
    SQLRETURN rc = metadataAPIHelper.initializeColumnField(
        pStmt,
        (char **)metadataAPIHelper.kTablesCol,
        kTablesColNum,
        (int *)metadataAPIHelper.kTablesColDatatype);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlCatalogsPostProcessing", "%s",
            PostProcessorLoggings::COLUMN_FIELD_INITIALIZE_FAILED);
        return rc;
    }

    // Apply post-processing
    rc = libpqCreateSQLCatalogsCustomizedResultSet(pStmt, kTablesColNum,
                                                   intermediateRS);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlCatalogsPostProcessing", "%s",
                     PostProcessorLoggings::RESULTSET_CREATION_FAILED);
        return rc;
    }

    RS_LOG_TRACE("sqlCatalogsPostProcessing", "%s", PostProcessorLoggings::END_TRACE_MSG);
    return rc;
}

//
// Helper function to apply post-processing on intermediate result set for
// SQLTables special call to retrieve schema list
//
SQLRETURN RsMetadataAPIPostProcessor::sqlSchemasPostProcessing(
    SQLHSTMT phstmt, const std::vector<SHOWSCHEMASResult> &intermediateRS) {

    RS_LOG_TRACE("sqlSchemasPostProcessing", "%s", PostProcessorLoggings::START_TRACE_MSG);
    if (!VALID_HSTMT(phstmt)) {
        RS_LOG_ERROR("sqlSchemasPostProcessing", "%s", PostProcessorLoggings::INVALID_HANDLER);
        return SQL_INVALID_HANDLE;
    }

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    // Initialize column names based on ODBC version
    RsMetadataAPIHelper::initializeColumnNames(pStmt);

    // Initialize column fields for table privileges
    SQLRETURN rc = metadataAPIHelper.initializeColumnField(
        pStmt,
        (char **)metadataAPIHelper.kTablesCol,
        kTablesColNum,
        (int *)metadataAPIHelper.kTablesColDatatype);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlSchemasPostProcessing", "%s",
            PostProcessorLoggings::COLUMN_FIELD_INITIALIZE_FAILED);
        return rc;
    }

    // Apply post-processing
    rc = libpqCreateSQLSchemasCustomizedResultSet(pStmt, kTablesColNum,
                                                  intermediateRS);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlSchemasPostProcessing", "%s",
                     PostProcessorLoggings::RESULTSET_CREATION_FAILED);
        return rc;
    }

    RS_LOG_TRACE("sqlSchemasPostProcessing", "%s", PostProcessorLoggings::END_TRACE_MSG);
    return rc;
}

//
// Helper function to apply post-processing on pre-defined table type list
//
SQLRETURN
RsMetadataAPIPostProcessor::sqlTableTypesPostProcessing(SQLHSTMT phstmt) {

    RS_LOG_TRACE("sqlTableTypesPostProcessing", "%s", PostProcessorLoggings::START_TRACE_MSG);
    if (!VALID_HSTMT(phstmt)) {
        RS_LOG_ERROR("sqlTableTypesPostProcessing", "%s", PostProcessorLoggings::INVALID_HANDLER);
        return SQL_INVALID_HANDLE;
    }

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    // Initialize column names based on ODBC version
    RsMetadataAPIHelper::initializeColumnNames(pStmt);

    // Initialize column fields for table privileges
    SQLRETURN rc = metadataAPIHelper.initializeColumnField(
        pStmt,
        (char **)metadataAPIHelper.kTablesCol,
        kTablesColNum,
        (int *)metadataAPIHelper.kTablesColDatatype);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlTableTypesPostProcessing", "%s",
            PostProcessorLoggings::COLUMN_FIELD_INITIALIZE_FAILED);
        return rc;
    }

    // Apply post-processing
    rc = libpqCreateSQLTableTypesCustomizedResultSet(
        pStmt, kTablesColNum, RsMetadataAPIHelper::tableTypeList);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlTableTypesPostProcessing", "%s",
                     PostProcessorLoggings::RESULTSET_CREATION_FAILED);
        return rc;
    }


    RS_LOG_TRACE("sqlTableTypesPostProcessing", "%s", PostProcessorLoggings::END_TRACE_MSG);
    return rc;
}

//
// Helper function to apply post-processing on intermediate result set for
// SQLTables
//
SQLRETURN RsMetadataAPIPostProcessor::sqlTablesPostProcessing(
    SQLHSTMT phstmt, std::string pTableType,
    const std::vector<SHOWTABLESResult> &intermediateRS) {

    RS_LOG_TRACE("sqlTablesPostProcessing", "%s", PostProcessorLoggings::START_TRACE_MSG);
    if (!VALID_HSTMT(phstmt)) {
        RS_LOG_ERROR("sqlTablesPostProcessing", "%s", PostProcessorLoggings::INVALID_HANDLER);
        return SQL_INVALID_HANDLE;
    }

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    // Initialize column names based on ODBC version
    RsMetadataAPIHelper::initializeColumnNames(pStmt);

    // Initialize column fields for table privileges
    SQLRETURN rc = metadataAPIHelper.initializeColumnField(
        pStmt,
        (char **)metadataAPIHelper.kTablesCol,
        kTablesColNum,
        (int *)metadataAPIHelper.kTablesColDatatype);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlTablesPostProcessing", "%s",
            PostProcessorLoggings::COLUMN_FIELD_INITIALIZE_FAILED);
        return rc;
    }

    // Apply post-processing
    rc = libpqCreateSQLTablesCustomizedResultSet(pStmt, kTablesColNum,
                                                 pTableType, intermediateRS);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlTablesPostProcessing", "%s",
                     PostProcessorLoggings::RESULTSET_CREATION_FAILED);
        return rc;
    }

    RS_LOG_TRACE("sqlTablesPostProcessing", "%s", PostProcessorLoggings::END_TRACE_MSG);
    return rc;
}

//
// Helper function to apply post-processing on intermediate result set for
// SQLColumns
//
SQLRETURN RsMetadataAPIPostProcessor::sqlColumnsPostProcessing(
    SQLHSTMT phstmt, const std::vector<SHOWCOLUMNSResult> &intermediateRS) {

    RS_LOG_TRACE("sqlColumnsPostProcessing", "%s", PostProcessorLoggings::START_TRACE_MSG);
    if (!VALID_HSTMT(phstmt)) {
        RS_LOG_ERROR("sqlColumnsPostProcessing", "%s", PostProcessorLoggings::INVALID_HANDLER);
        return SQL_INVALID_HANDLE;
    }

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    // Initialize column names based on ODBC version
    RsMetadataAPIHelper::initializeColumnNames(pStmt);

    // Initialize column fields for table privileges
    SQLRETURN rc = metadataAPIHelper.initializeColumnField(
        pStmt,
        (char **)metadataAPIHelper.kColumnsCol,
        kColumnsColNum,
        (int *)metadataAPIHelper.kColumnsColDatatype);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlColumnsPostProcessing", "%s",
            PostProcessorLoggings::COLUMN_FIELD_INITIALIZE_FAILED);
        return rc;
    }

    // Apply post-processing
    rc = libpqCreateSQLColumnsCustomizedResultSet(pStmt, kColumnsColNum,
                                                  intermediateRS);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlColumnsPostProcessing", "%s",
                     PostProcessorLoggings::RESULTSET_CREATION_FAILED);
        return rc;
    }

    RS_LOG_TRACE("sqlColumnsPostProcessing", "%s", PostProcessorLoggings::END_TRACE_MSG);

    return rc;
}

//
// Helper function to apply post-processing on intermediate result set for
// SQLPrimaryKeys
//
SQLRETURN RsMetadataAPIPostProcessor::sqlPrimaryKeysPostProcessing(
    SQLHSTMT phstmt, const std::vector<SHOWCONSTRAINTSPRIMARYKEYSResult> &intermediateRS) {
    SQLRETURN rc = SQL_SUCCESS;

    RS_LOG_TRACE("sqlPrimaryKeysPostProcessing", "%s", PostProcessorLoggings::START_TRACE_MSG);
    if (!VALID_HSTMT(phstmt)) {
        RS_LOG_ERROR("sqlPrimaryKeysPostProcessing", "%s", PostProcessorLoggings::INVALID_HANDLER);
        return SQL_INVALID_HANDLE;
    }

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    // Initialize column names based on ODBC version
    RsMetadataAPIHelper::initializeColumnNames(pStmt);

    rc = metadataAPIHelper.initializeColumnField(
        pStmt,
        (char **)metadataAPIHelper.kPrimaryKeysCol,
        kPrimaryKeysColNum,
        (int *)metadataAPIHelper.kPrimaryKeysColDatatype);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlPrimaryKeysPostProcessing", "%s",
            PostProcessorLoggings::COLUMN_FIELD_INITIALIZE_FAILED);
        return rc;
    }

    // Apply post-processing
    rc = libpqCreateSQLPrimaryKeysCustomizedResultSet(pStmt, kPrimaryKeysColNum,
                                                 intermediateRS);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlPrimaryKeysPostProcessing", "%s",
                     PostProcessorLoggings::RESULTSET_CREATION_FAILED);
        return rc;
    }

    RS_LOG_TRACE("sqlPrimaryKeysPostProcessing", "%s", PostProcessorLoggings::END_TRACE_MSG);
    return rc;
}

//
// Helper function to apply post-processing on intermediate result set for
// SQLForeignKeys
//
SQLRETURN RsMetadataAPIPostProcessor::sqlForeignKeysPostProcessing(
    SQLHSTMT phstmt,
    const std::vector<SHOWCONSTRAINTSFOREIGNKEYSResult> &intermediateRS) {

    RS_LOG_TRACE("sqlForeignKeysPostProcessing", "%s", PostProcessorLoggings::START_TRACE_MSG);
    if (!VALID_HSTMT(phstmt)) {
        RS_LOG_ERROR("sqlForeignKeysPostProcessing", "%s", PostProcessorLoggings::INVALID_HANDLER);
        return SQL_INVALID_HANDLE;
    }

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    // Initialize column names based on ODBC version
    RsMetadataAPIHelper::initializeColumnNames(pStmt);

    SQLRETURN rc = metadataAPIHelper.initializeColumnField(
        pStmt, (char **)metadataAPIHelper.kForeignKeysCol, kForeignKeysColNum,
        (int *)metadataAPIHelper.kForeignKeysColDatatype);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlForeignKeysPostProcessing", "%s",
            PostProcessorLoggings::COLUMN_FIELD_INITIALIZE_FAILED);
        return rc;
    }

    // Apply post-processing
    rc = libpqCreateSQLForeignKeysCustomizedResultSet(pStmt, kForeignKeysColNum,
                                                      intermediateRS);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlForeignKeysPostProcessing", "%s",
                     PostProcessorLoggings::RESULTSET_CREATION_FAILED);
        return rc;
    }

    RS_LOG_TRACE("sqlForeignKeysPostProcessing", "%s", PostProcessorLoggings::END_TRACE_MSG);
    return rc;
}

//
// Helper function to apply post-processing on intermediate result set for
// SQLSpecialColumns
//
SQLRETURN RsMetadataAPIPostProcessor::sqlSpecialColumnsPostProcessing(
    SQLHSTMT phstmt,
    const std::vector<SHOWCOLUMNSResult> &intermediateRS,
    SQLUSMALLINT identifierType) {
    
    RS_LOG_TRACE("sqlSpecialColumnsPostProcessing", "%s", PostProcessorLoggings::START_TRACE_MSG);
    if (!VALID_HSTMT(phstmt)) {
        RS_LOG_ERROR("sqlSpecialColumnsPostProcessing", "%s", PostProcessorLoggings::INVALID_HANDLER);
        return SQL_INVALID_HANDLE;
    }

    if (identifierType != SQL_BEST_ROWID && identifierType != SQL_ROWVER) {
        RS_LOG_ERROR("sqlSpecialColumnsPostProcessing", "Invalid identifierType parameter");
        return SQL_ERROR;
    }

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    // Initialize column names based on ODBC version
    RsMetadataAPIHelper::initializeColumnNames(pStmt);

    // Initialize column fields for special columns
    SQLRETURN rc = metadataAPIHelper.initializeColumnField(
        pStmt,
        (char **)metadataAPIHelper.kSpecialColumnsCol,
        kSpecialColumnsColNum,
        (int *)metadataAPIHelper.kSpecialColumnsColDatatype);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlSpecialColumnsPostProcessing", "%s",
            PostProcessorLoggings::COLUMN_FIELD_INITIALIZE_FAILED);
        return rc;
    }

    // Apply post-processing
    rc = libpqCreateSQLSpecialColumnsCustomizedResultSet(
        pStmt,
        kSpecialColumnsColNum,
        intermediateRS,
        identifierType);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlSpecialColumnsPostProcessing", "%s",
                     PostProcessorLoggings::RESULTSET_CREATION_FAILED);
        return rc;
    }

    RS_LOG_TRACE("sqlSpecialColumnsPostProcessing", "%s", PostProcessorLoggings::END_TRACE_MSG);
    return rc;
}

//
// Helper function to apply post-processing on intermediate result set for
// SQLTablePrivileges
//
SQLRETURN RsMetadataAPIPostProcessor::sqlTablePrivilegesPostProcessing(
    SQLHSTMT phstmt,
    const std::vector<SHOWGRANTSTABLEResult> &intermediateRS) {
    
    RS_LOG_TRACE("sqlTablePrivilegesPostProcessing", "%s", PostProcessorLoggings::START_TRACE_MSG);
    if (!VALID_HSTMT(phstmt)) {
        RS_LOG_ERROR("sqlTablePrivilegesPostProcessing", "%s", PostProcessorLoggings::INVALID_HANDLER);
        return SQL_INVALID_HANDLE;
    }

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    // Initialize column names based on ODBC version
    RsMetadataAPIHelper::initializeColumnNames(pStmt);

    // Initialize column fields for table privileges
    SQLRETURN rc = metadataAPIHelper.initializeColumnField(
        pStmt, 
        (char **)metadataAPIHelper.kTablePrivilegesCol, 
        kTablePrivilegesColNum,
        (int *)metadataAPIHelper.kTablePrivilegesColDatatype);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlTablePrivilegesPostProcessing", "%s",
            PostProcessorLoggings::COLUMN_FIELD_INITIALIZE_FAILED);
        return rc;
    }

    // Apply post-processing
    rc = libpqCreateSQLTablePrivilegesCustomizedResultSet(
        pStmt, 
        kTablePrivilegesColNum,
        intermediateRS);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlTablePrivilegesPostProcessing", "%s",
                     PostProcessorLoggings::RESULTSET_CREATION_FAILED);
        return rc;
    }

    RS_LOG_TRACE("sqlTablePrivilegesPostProcessing", "%s", PostProcessorLoggings::END_TRACE_MSG);
    return rc;
}

//
// Helper function to apply post-processing on intermediate result set for
// SQLColumnPrivileges
//
SQLRETURN RsMetadataAPIPostProcessor::sqlColumnPrivilegesPostProcessing(
    SQLHSTMT phstmt,
    const std::vector<SHOWGRANTSCOLUMNResult> &intermediateRS) {
    
    RS_LOG_TRACE("sqlColumnPrivilegesPostProcessing", "%s", PostProcessorLoggings::START_TRACE_MSG);
    if (!VALID_HSTMT(phstmt)) {
        RS_LOG_ERROR("sqlColumnPrivilegesPostProcessing", "%s", PostProcessorLoggings::INVALID_HANDLER);
        return SQL_INVALID_HANDLE;
    }

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    // Initialize column names based on ODBC version
    RsMetadataAPIHelper::initializeColumnNames(pStmt);

    // Initialize column fields for column privileges
    SQLRETURN rc = metadataAPIHelper.initializeColumnField(
        pStmt, 
        (char **)metadataAPIHelper.kColumnPrivilegesCol, 
        kColumnPrivilegesColNum,
        (int *)metadataAPIHelper.kColumnPrivilegesColDatatype);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlColumnPrivilegesPostProcessing", "%s",
            PostProcessorLoggings::COLUMN_FIELD_INITIALIZE_FAILED);
        return rc;
    }

    // Apply post-processing
    rc = libpqCreateSQLColumnPrivilegesCustomizedResultSet(
        pStmt, 
        kColumnPrivilegesColNum,
        intermediateRS);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlColumnPrivilegesPostProcessing", "%s",
                     PostProcessorLoggings::RESULTSET_CREATION_FAILED);
        return rc;
    }

    RS_LOG_TRACE("sqlColumnPrivilegesPostProcessing", "%s", PostProcessorLoggings::END_TRACE_MSG);
    return rc;
}

//
// Helper function to apply post-processing on intermediate result set for
// SQLProcedures
//
SQLRETURN RsMetadataAPIPostProcessor::sqlProceduresPostProcessing(
    SQLHSTMT phstmt,
    const std::vector<SHOWPROCEDURESFUNCTIONSResult> &intermediateRS) {
    
    RS_LOG_TRACE("sqlProceduresPostProcessing", "%s", PostProcessorLoggings::START_TRACE_MSG);
    if (!VALID_HSTMT(phstmt)) {
        RS_LOG_ERROR("sqlProceduresPostProcessing", "%s", PostProcessorLoggings::INVALID_HANDLER);
        return SQL_INVALID_HANDLE;
    }

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    // Initialize column names based on ODBC version
    RsMetadataAPIHelper::initializeColumnNames(pStmt);

    // Initialize column fields for procedures
    SQLRETURN rc = metadataAPIHelper.initializeColumnField(
        pStmt, 
        (char **)metadataAPIHelper.kProceduresCol, 
        kProceduresColNum,
        (int *)metadataAPIHelper.kProceduresColDatatype);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlProceduresPostProcessing", "%s",
            PostProcessorLoggings::COLUMN_FIELD_INITIALIZE_FAILED);
        return rc;
    }

    // Apply post-processing
    rc = libpqCreateSQLProceduresCustomizedResultSet(
        pStmt, 
        kProceduresColNum,
        intermediateRS);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlProceduresPostProcessing", "%s",
                     PostProcessorLoggings::RESULTSET_CREATION_FAILED);
        return rc;
    }

    RS_LOG_TRACE("sqlProceduresPostProcessing", "%s", PostProcessorLoggings::END_TRACE_MSG);
    return rc;
}

//
// Helper function to apply post-processing on intermediate result set for
// SQLProcedureColumns
//
SQLRETURN RsMetadataAPIPostProcessor::sqlProcedureColumnsPostProcessing(
    SQLHSTMT phstmt, const std::vector<SHOWCOLUMNSResult> &intermediateRS) {

    RS_LOG_TRACE("sqlProcedureColumnsPostProcessing", "%s", PostProcessorLoggings::START_TRACE_MSG);
    if (!VALID_HSTMT(phstmt)) {
        RS_LOG_ERROR("sqlProcedureColumnsPostProcessing", "%s", PostProcessorLoggings::INVALID_HANDLER);
        return SQL_INVALID_HANDLE;
    }

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    // Initialize column names
    RsMetadataAPIHelper::initializeColumnNames(pStmt);

    SQLRETURN rc = metadataAPIHelper.initializeColumnField(
        pStmt, (char **)metadataAPIHelper.kProcedureColumnsCol,
        kProcedureColumnsColNum,
        (int *)metadataAPIHelper.kProcedureColumnsColDatatype);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlProcedureColumnsPostProcessing", "%s",
            PostProcessorLoggings::COLUMN_FIELD_INITIALIZE_FAILED);
        return rc;
    }

    // Apply post-processing
    rc = libpqCreateSQLProcedureColumnsCustomizedResultSet(
        pStmt, kProcedureColumnsColNum, intermediateRS);
    if (!SQL_SUCCEEDED(rc)) {
        RS_LOG_ERROR("sqlProcedureColumnsPostProcessing", "%s",
                     PostProcessorLoggings::RESULTSET_CREATION_FAILED);
        return rc;
    }

    RS_LOG_TRACE("sqlProcedureColumnsPostProcessing", "%s", PostProcessorLoggings::END_TRACE_MSG);
    return rc;
}
