#pragma once

#ifndef __RS_EXECUTE_H__

#define __RS_EXECUTE_H__


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "rsodbc.h"
#include "rsutil.h"
#include "rsunicode.h"
#include "rstrace.h"


class RsExecute {
  public:
    static SQLRETURN  SQL_API RS_SQLExecDirect(SQLHSTMT phstmt,
                                        SQLCHAR* pCmd,
                                        SQLINTEGER cbLen,
                                        int iInternal,
                                        int executePrepared,
                                        int iSQLExecDirectW,
                                        int iLockRequired);

    static SQLRETURN SQL_API RS_SQLNativeSql(SQLHDBC   phdbc,
                                        SQLCHAR*    szSqlStrIn,
                                        SQLINTEGER    cbSqlStrIn,
                                        SQLCHAR*    szSqlStrOut,
                                        SQLINTEGER  cbSqlStrOut,
                                        SQLINTEGER  *pcbSqlStrOut,
                                        int iInternal);
};

#endif // __RS_EXECUTE_H__
