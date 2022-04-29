#pragma once

#ifndef __RS_OPTIONS_H__

#define __RS_OPTIONS_H__


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "rsodbc.h"
#include "rsutil.h"
#include "rsunicode.h"
#include "rstrace.h"


class RsOptions {
  public:

    static SQLRETURN  SQL_API RS_SQLSetConnectOption(SQLHDBC phdbc,
                                                      SQLUSMALLINT hOption,
                                                      SQLULEN Value);

    static SQLRETURN  SQL_API RS_SQLGetConnectOption(SQLHDBC phdbc,
                                                      SQLUSMALLINT hOption,
                                                      SQLPOINTER pValue);

    static SQLRETURN  SQL_API RS_SQLSetStmtOption(SQLHSTMT phstmt,
                                                  SQLUSMALLINT hOption,
                                                  SQLULEN Value);

    static SQLRETURN  SQL_API RS_SQLGetConnectAttr(SQLHDBC phdbc,
                                                   SQLINTEGER    iAttribute,
                                                   SQLPOINTER    pValue,
                                                   SQLINTEGER    cbLen,
                                                   SQLINTEGER  *pcbLen);

    static SQLRETURN  SQL_API RS_SQLGetStmtAttr(SQLHSTMT        phstmt,
                                                SQLINTEGER    iAttribute,
                                                SQLPOINTER    pValue,
                                                SQLINTEGER    cbLen,
                                                SQLINTEGER  *pcbLen);

    static SQLRETURN  SQL_API RS_SQLSetConnectAttr(SQLHDBC phdbc,
                                                   SQLINTEGER    iAttribute,
                                                   SQLPOINTER    pValue,
                                                   SQLINTEGER    cbLen);

    static SQLRETURN  SQL_API RS_SQLSetStmtAttr(SQLHSTMT    phstmt,
                                                 SQLINTEGER    iAttribute,
                                                 SQLPOINTER    pValue,
                                                 SQLINTEGER    cbLen);

};

#endif // __RS_OPTIONS_H__


