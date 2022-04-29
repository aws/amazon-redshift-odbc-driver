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
 * Class to wait and signal the read events for multi results.
 * 
 *
 */
typedef struct _ClientSideCursorMultiResultLock 
{
    // Monitor for condition check
    MUTEX_HANDLE m_lock;
    
    // Full result read condition
    SEM_HANDLE m_fullResultReadFromServerDoneCondition;

    // 0 means locked, 1 means unlocked and -1 means destroyed. This we use for reset the sem.
    int m_fullResultReadFromServerDoneConditionStatus; 

    // In memory result read condition
    SEM_HANDLE m_firstBatchInMemResultReadFromServerDoneCondition;

    // 0 means locked, 1 means unlocked and -1 means destroyed. This we use for reset the sem.
    int m_firstBatchInMemResultReadFromServerDoneConditionStatus; 
    
    // waiting for first batch for this execution number
    int m_firstBatchWaitExecutionNumber;
    
    // Current query execution number. 1..N
    int m_curExecutionNumber;
    
    // First batch signal for query execution number. 1..N
    int m_firstBatchSignalExecutionNumber;

    // Query done indicator
    int m_queryDone;

}ClientSideCursorMultiResultLock;

// Function declarations

#ifdef __cplusplus
extern "C" 
{
#endif /* C++ */

ClientSideCursorMultiResultLock *createCscMultiResultLock(void);
ClientSideCursorMultiResultLock *destroyCscMultiResultLock(ClientSideCursorMultiResultLock *pCscMultiResultLock);
void waitForFullResultReadFromServerCscMultiResultLock(ClientSideCursorMultiResultLock *pCscMultiResultLock, int resultExecutionNumber);
void signalForFullResultReadFromServerCscMultiResultLock(ClientSideCursorMultiResultLock *pCscMultiResultLock, int queryDone);
void waitForFirstBatchInMemResultReadFromServerCscMultiResultLock(ClientSideCursorMultiResultLock *pCscMultiResultLock, int resultExecutionNumber);
void signalForFirstBatchInMemResultReadFromServerCscMultiResultLock(ClientSideCursorMultiResultLock *pCscMultiResultLock, int resultExecutionNumber);
void setResultExecutionNumberCscMultiResultLock(ClientSideCursorMultiResultLock *pCscMultiResultLock, int curExecutionNumber);
int getResultExecutionNumberCscMultiResultLock(ClientSideCursorMultiResultLock *pCscMultiResultLock);
void resetCscMultiResultLock(ClientSideCursorMultiResultLock *pCscMultiResultLock);

#ifdef __cplusplus
}
#endif /* C++ */
