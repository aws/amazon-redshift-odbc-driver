/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
*-------------------------------------------------------------------------
*/

#ifdef WIN32
#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <sql.h>
#include <sqlext.h>

#include "csc_win_port.h"

typedef SQLLEN* SQLLEN_POINTER;

void move_dialog_to_center(HWND hdlg);
void openHelpFile(char *anchor);

#endif // WIN32
