/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#pragma once

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#endif // WIN32

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef WIN32
#include "csc_win_port.h"
#endif // WIN32

#ifdef LINUX
#include "csc_linux_port.h"
#endif

#include "rsfile.h"
#include "rslock.h"
#include "rsmem.h"
#include "ClientSideCursorTrace.h"


/**
 * Class to wait and signal the read events.
 * 
 *
 */
typedef struct _ClientSideCursorLock 
{
    // Monitor for condition check
    MUTEX_HANDLE m_lock;
    
    // Full result read condition
    SEM_HANDLE m_fullResultReadFromServerDoneCondition;
    
    // Full result read indicator
    int m_fullResultReadFromServerDone;
    
    // Batch result read condition
    SEM_HANDLE  m_batchResultReadFromServerDoneCondition;

    // 0 means locked, 1 means unlocked and -1 means destroyed. 
    int  m_batchResultReadFromServerDoneConditionStatus;
}ClientSideCursorLock;

// Function declarations

#ifdef __cplusplus
extern "C" 
{
#endif /* C++ */

ClientSideCursorLock *createCscLock(void);
ClientSideCursorLock *destroyCscLock(ClientSideCursorLock *pCscLock);
void waitForFullResultReadFromServerCscLock(ClientSideCursorLock *pCscLock);
void signalForFullResultReadFromServerCscLock(ClientSideCursorLock *pCscLock);
int isFullResultReadFromServerCscLock(ClientSideCursorLock *pCscLock);
void waitForBatchResultReadFromServerCscLock(ClientSideCursorLock *pCscLock);
void signalForBatchResultReadFromServerCscLock(ClientSideCursorLock *pCscLock);

#ifdef __cplusplus
}
#endif /* C++ */

