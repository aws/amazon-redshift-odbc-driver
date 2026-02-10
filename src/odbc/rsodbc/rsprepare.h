#pragma once

#ifndef __RS_PREPARE_H__

#define __RS_PREPARE_H__


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "rsodbc.h"
#include "rsutil.h"
#include "rsunicode.h"
#include "rstrace.h"

class RsPrepare {
  public:
    static SQLRETURN  SQL_API RS_SQLSetCursorName(SQLHSTMT phstmt,
                                                  SQLCHAR* pCursorName,
                                                  SQLSMALLINT cbLen);

    static  SQLRETURN  SQL_API RS_SQLGetCursorName(SQLHSTMT phstmt,
                                                    SQLCHAR *pCursorName,
                                                    SQLSMALLINT cbLen,
                                                    SQLSMALLINT *pcbLen);

    static SQLRETURN  SQL_API RS_SQLCloseCursor(RS_STMT_INFO *pStmt);

    static SQLRETURN  SQL_API RS_SQLPrepare(SQLHSTMT phstmt,
                                            SQLCHAR* pCmd,
                                            SQLINTEGER cbLen,
                                            int iInternal,
                                            int iSQLPrepareW,
                                            int iReprepareForMultiInsert,
                                            int iLockRequired);
};

#endif // __RS_PREPARE_H__
