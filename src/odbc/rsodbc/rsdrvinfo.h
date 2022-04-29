#pragma once

#ifndef __RS_DRVINFO_H__

#define __RS_DRVINFO_H__


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "rsodbc.h"
#include "rsutil.h"
#include "rsunicode.h"
#include "rstrace.h"
#include "rsversion.h"


class RsDrvInfo {
  public:
    static SQLRETURN  SQL_API RS_SQLGetInfo(SQLHDBC phdbc,
                                            SQLUSMALLINT hInfoType,
                                            SQLPOINTER pInfoValue,
                                            SQLSMALLINT cbLen,
                                            SQLSMALLINT *pcbLen,
                                            int *piRetType);

};

#endif // __RS_DRVINFO_H__


