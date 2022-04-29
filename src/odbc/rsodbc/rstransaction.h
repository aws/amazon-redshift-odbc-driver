#pragma once

#ifndef __RS_TRANSACTION_H__

#define __RS_TRANSACTION_H__


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "rsodbc.h"
#include "rsutil.h"
#include "rsunicode.h"
#include "rstrace.h"

class RsTransaction {
  public:
    static SQLRETURN  SQL_API RS_SQLTransact(SQLHENV phenv,
                                     SQLHDBC phdbc,
                                     SQLUSMALLINT hCompletionType);

};

#endif // __RS_TRANSACTION_H__


