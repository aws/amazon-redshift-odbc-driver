#pragma once

#ifndef __RS_ERROR_H__

#define __RS_ERROR_H__


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "rsodbc.h"
#include "rsutil.h"
#include "rsunicode.h"
#include "rstrace.h"


class RsError {
  public:
    static SQLRETURN  SQL_API RS_SQLError(SQLHENV phenv,
                                          SQLHDBC phdbc,
                                          SQLHSTMT phstmt,
                                          SQLHDESC phdesc,
                                          SQLCHAR *pSqlstate,
                                          SQLINTEGER *piNativeError,
                                          SQLCHAR *pMessageText,
                                          SQLSMALLINT cbLen,
                                          SQLSMALLINT *pcbLen,
                                          SQLSMALLINT recNumber,
                                          int remove);

    static SQLRETURN  SQL_API RS_SQLGetDiagRec(SQLSMALLINT hHandleType,
                                       SQLHANDLE pHandle,
                                       SQLSMALLINT hRecNumber,
                                       SQLCHAR *pSqlstate,
                                       SQLINTEGER *piNativeError,
                                       SQLCHAR *pMessageText,
                                       SQLSMALLINT cbLen,
                                       SQLSMALLINT *pcbLen);

    static SQLRETURN  SQL_API RS_SQLGetDiagField(SQLSMALLINT HandleType,
                                            SQLHANDLE Handle,
                                            SQLSMALLINT hRecNumber,
                                            SQLSMALLINT hDiagIdentifier,
                                            SQLPOINTER  pDiagInfo,
                                            SQLSMALLINT cbLen,
                                            SQLSMALLINT *pcbLen,
                                            int *piRetType);


};

#endif // __RS_ERROR_H__


