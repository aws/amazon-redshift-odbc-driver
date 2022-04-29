#pragma once

#ifndef __RS_DESC_H__

#define __RS_DESC_H__


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "rsodbc.h"
#include "rsutil.h"
#include "rsunicode.h"
#include "rstrace.h"


class RsDesc {
  public:
    static SQLRETURN  SQL_API RS_SQLCopyDesc(SQLHDESC phdescSrc,
                                              SQLHDESC phdescDest,
                                              int iInternal);

    static SQLRETURN  SQL_API RS_SQLGetDescField(SQLHDESC phdesc,
                                           SQLSMALLINT hRecNumber,
                                           SQLSMALLINT hFieldIdentifier,
                                           SQLPOINTER pValue,
                                           SQLINTEGER cbLen,
                                           SQLINTEGER *pcbLen,
                                           int iInternal);

    static SQLRETURN  SQL_API RS_SQLGetDescRec(SQLHDESC phdesc,
                                        SQLSMALLINT hRecNumber,
                                        SQLCHAR *pName,
                                        SQLSMALLINT cbName,
                                        SQLSMALLINT *pcbName,
                                        SQLSMALLINT *phType,
                                        SQLSMALLINT *phSubType,
                                        SQLLEN     *plOctetLength,
                                        SQLSMALLINT *phPrecision,
                                        SQLSMALLINT *phScale,
                                        SQLSMALLINT *phNullable);

    static SQLRETURN  SQL_API RS_SQLSetDescField(SQLHDESC phdesc,
                                        SQLSMALLINT hRecNumber,
                                        SQLSMALLINT hFieldIdentifier,
                                        SQLPOINTER pValue,
                                        SQLINTEGER cbLen,
                                        int iInternal);




};

#endif // __RS_DESC_H__


