#pragma once

#ifndef __RS_CATALOG_H__

#define __RS_CATALOG_H__


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "rsodbc.h"
#include "rsutil.h"
#include "rsunicode.h"
#include "rstrace.h"


// Error message templates
namespace RsMetadataErrors {
    // Generic error templates
    static const char* PROXY_CALL_FAILED = "Failed to retrieve %s information - Server proxy call failed";
    static const char* POST_PROCESSING_FAILED = "Failed to process %s information - Post-processing failed";

    // User-facing error messages
    static const char* ERROR_EXECUTE_QUERY = "Failed to retrieve %s information - Unable to execute query";
    static const char* ERROR_PROCESS_RESULTS = "Failed to retrieve %s information - Error processing result set";

    // Specific operation types
    static const char* TYPE_GET_TYPE_INFO = "get type info";
    static const char* TYPE_CATALOG = "catalog";
    static const char* TYPE_SCHEMA = "schema";
    static const char* TYPE_TABLE = "table";
    static const char* TYPE_COLUMN = "column";
    static const char* TYPE_PRIMARY_KEY = "primary key";
    static const char* TYPE_FOREIGN_KEY = "foreign key";
    static const char* TYPE_COLUMN_PRIVILEGES = "column privileges";
    static const char* TYPE_TABLE_PRIVILEGES = "table privileges";
    static const char* TYPE_PROCEDURE = "procedure";
    static const char* TYPE_TABLE_TYPE_INFO = "table type";

    // SQLSTATE codes
    static const std::string GENERAL_ERROR = "HY000";

    // Helper function to format error messages
    static std::string formatError(const char* template_msg, const char* type) {
      int size_needed = snprintf(nullptr, 0, template_msg, type) + 1; // +1 for null terminator
      if (size_needed <= 0) {
          return "Error formatting message";
      }

      std::vector<char> buffer(size_needed);
      snprintf(buffer.data(), size_needed, template_msg, type);
      return std::string(buffer.data());
    }
}

class RsCatalog {
  public:
    static SQLRETURN  SQL_API RS_SQLTables(SQLHSTMT phstmt,
                                           SQLCHAR *pCatalogName,
                                           SQLSMALLINT cbCatalogName,
                                           SQLCHAR *pSchemaName,
                                           SQLSMALLINT cbSchemaName,
                                           SQLCHAR *pTableName,
                                           SQLSMALLINT cbTableName,
                                           SQLCHAR *pTableType,
                                           SQLSMALLINT cbTableType);

    static SQLRETURN  SQL_API RS_SQLColumns(SQLHSTMT phstmt,
                                              SQLCHAR *pCatalogName,
                                              SQLSMALLINT cbCatalogName,
                                              SQLCHAR *pSchemaName,
                                              SQLSMALLINT cbSchemaName,
                                              SQLCHAR *pTableName,
                                              SQLSMALLINT cbTableName,
                                              SQLCHAR *pColumnName,
                                              SQLSMALLINT cbColumnName);

    static SQLRETURN  SQL_API RS_SQLStatistics(SQLHSTMT phstmt,
                                                SQLCHAR *pCatalogName,
                                                SQLSMALLINT cbCatalogName,
                                                SQLCHAR *pSchemaName,
                                                SQLSMALLINT cbSchemaName,
                                                SQLCHAR *pTableName,
                                                SQLSMALLINT cbTableName,
                                                SQLUSMALLINT hUnique,
                                                SQLUSMALLINT hReserved);

    static SQLRETURN  SQL_API RS_SQLSpecialColumns(SQLHSTMT phstmt,
                                                    SQLUSMALLINT hIdenType,
                                                    SQLCHAR *pCatalogName,
                                                    SQLSMALLINT cbCatalogName,
                                                    SQLCHAR *pSchemaName,
                                                    SQLSMALLINT cbSchemaName,
                                                    SQLCHAR *pTableName,
                                                    SQLSMALLINT cbTableName,
                                                    SQLUSMALLINT hScope,
                                                    SQLUSMALLINT hNullable);

    static SQLRETURN SQL_API RS_SQLProcedureColumns(SQLHSTMT           phstmt,
                                                      SQLCHAR          *pCatalogName,
                                                      SQLSMALLINT      cbCatalogName,
                                                      SQLCHAR          *pSchemaName,
                                                      SQLSMALLINT      cbSchemaName,
                                                      SQLCHAR          *pProcName,
                                                      SQLSMALLINT      cbProcName,
                                                      SQLCHAR          *pColumnName,
                                                      SQLSMALLINT      cbColumnName);

    static SQLRETURN SQL_API RS_SQLProcedures(SQLHSTMT           phstmt,
                                                    SQLCHAR           *pCatalogName,
                                                    SQLSMALLINT        cbCatalogName,
                                                    SQLCHAR           *pSchemaName,
                                                    SQLSMALLINT        cbSchemaName,
                                                    SQLCHAR           *pProcName,
                                                    SQLSMALLINT        cbProcName);

    static SQLRETURN SQL_API RS_SQLForeignKeys(SQLHSTMT               phstmt,
                                                  SQLCHAR           *pPkCatalogName,
                                                  SQLSMALLINT        cbPkCatalogName,
                                                  SQLCHAR           *pPkSchemaName,
                                                  SQLSMALLINT        cbPkSchemaName,
                                                  SQLCHAR           *pPkTableName,
                                                  SQLSMALLINT        cbPkTableName,
                                                  SQLCHAR           *pFkCatalogName,
                                                  SQLSMALLINT        cbFkCatalogName,
                                                  SQLCHAR           *pFkSchemaName,
                                                  SQLSMALLINT        cbFkSchemaName,
                                                  SQLCHAR           *pFkTableName,
                                                  SQLSMALLINT        cbFkTableName);

    static SQLRETURN SQL_API RS_SQLPrimaryKeys(SQLHSTMT           phstmt,
                                                SQLCHAR         *pCatalogName,
                                                SQLSMALLINT     cbCatalogName,
                                                SQLCHAR         *pSchemaName,
                                                SQLSMALLINT     cbSchemaName,
                                                SQLCHAR         *pTableName,
                                                SQLSMALLINT     cbTableName);

   static SQLRETURN  SQL_API RS_SQLGetTypeInfo(SQLHSTMT phstmt,
                                                SQLSMALLINT hType);

   static SQLRETURN SQL_API RS_SQLColumnPrivileges(SQLHSTMT           phstmt,
                                                     SQLCHAR          *pCatalogName,
                                                     SQLSMALLINT      cbCatalogName,
                                                     SQLCHAR          *pSchemaName,
                                                     SQLSMALLINT      cbSchemaName,
                                                     SQLCHAR          *pTableName,
                                                     SQLSMALLINT      cbTableName,
                                                     SQLCHAR          *pColumnName,
                                                     SQLSMALLINT      cbColumnName);

   static SQLRETURN SQL_API RS_SQLTablePrivileges(SQLHSTMT           phstmt,
                                                   SQLCHAR         *pCatalogName,
                                                   SQLSMALLINT     cbCatalogName,
                                                   SQLCHAR         *pSchemaName,
                                                   SQLSMALLINT     cbSchemaName,
                                                   SQLCHAR         *pTableName,
                                                   SQLSMALLINT      cbTableName);

};

#endif // __RS_CATALOG_H__
