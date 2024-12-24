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

#define SQL_DRIVER_SQL_TYPE_MAX 0x7fff
#define SQL_NO_TYPE 120

static const SQLSMALLINT SQL_TYPE_MIN = SQL_GUID-1;
static const SQLSMALLINT SQL_TYPE_MAX = SQL_INTERVAL_MINUTE_TO_SECOND;
static const SQLSMALLINT SQL_MAP_SIZE = SQL_TYPE_MAX - SQL_TYPE_MIN+1;

typedef SQLSMALLINT SQLMAP[SQL_MAP_SIZE];

class RsParameter {
  public:
    static SQLMAP sqlTypeMap;
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


