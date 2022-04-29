/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#pragma once

#ifdef WIN32
#include <windows.h>
#include <process.h>
#endif // WIN32

#include <stdlib.h>

#ifdef WIN32
#include "csc_win_port.h"
#endif // WIN32

#ifdef LINUX
#include "csc_linux_port.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif /* C++ */

THREAD_HANDLE rsCreateThread(void *pThreadProc, void *pArg);
int rsJoinThread(THREAD_HANDLE hThread);
RS_DWORD rsGetCurrentThreadId(void);
void rsEndThread(void);
THREAD_HANDLE rsCloseThreadHandle(THREAD_HANDLE hThread);
void rsKillThread(THREAD_HANDLE hThread);

MUTEX_HANDLE rsCreateMutex(void);
int rsLockMutex(MUTEX_HANDLE hMutex);
int rsUnlockMutex(MUTEX_HANDLE hMutex);
int rsDestroyMutex(MUTEX_HANDLE hMutex);

SEM_HANDLE rsCreateBinarySem(int initialState);
int rsLockSem(SEM_HANDLE hSem);
int rsUnlockSem(SEM_HANDLE hSem);
int rsDestroySem(SEM_HANDLE hSem);

RS_DWORD rsGetCurrentProcessId(void);


#ifdef __cplusplus
}
#endif /* C++ */
