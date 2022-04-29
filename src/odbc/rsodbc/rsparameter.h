#pragma once

#ifndef __RS_PARAMETER_H__

#define __RS_PARAMETER_H__


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "rsodbc.h"
#include "rsutil.h"
#include "rsunicode.h"
#include "rstrace.h"

class RsParameter {
  public:
    static SQLRETURN SQL_API RS_SQLBindParameter(SQLHSTMT     phstmt,
                                      SQLUSMALLINT       hParam,
                                      SQLSMALLINT        hInOutType,
                                      SQLSMALLINT        hType,
                                      SQLSMALLINT        hSQLType,
                                      SQLULEN            iColSize,
                                      SQLSMALLINT        hScale,
                                      SQLPOINTER         pValue,
                                      SQLLEN             cbLen,
                                      SQLLEN             *pcbLenInd);

    static SQLRETURN SQL_API RS_SQLNumParams(SQLHSTMT  phstmt,
                                      SQLSMALLINT   *pParamCount);

    static SQLRETURN SQL_API RS_SQLDescribeParam(SQLHSTMT            phstmt,
                                              SQLUSMALLINT    hParam,
                                              SQLSMALLINT     *pDataType,
                                              SQLULEN         *pParamSize,
                                              SQLSMALLINT     *pDecimalDigits,
                                              SQLSMALLINT     *pNullable);

    static SQLRETURN SQL_API RS_SQLParamOptions(SQLHSTMT phstmt,
                                              SQLULEN  iCrow,
                                              SQLULEN  *piRow);
};

#endif // __RS_PARAMETER_H__


