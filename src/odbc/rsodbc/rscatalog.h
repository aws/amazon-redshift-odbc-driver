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


