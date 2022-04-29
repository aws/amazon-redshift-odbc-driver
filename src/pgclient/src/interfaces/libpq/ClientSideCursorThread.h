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
#include <sys/types.h>
#include <windows.h>
#endif // WIN32
#if defined LINUX 
#include <unistd.h>
#endif

#include <stdio.h>

// FWD declarations.
struct _MessageLoopState;

#include "rslock.h"
#include "MessageLoopState.h"

/**
 * Client side cursor thread to call message loop.
 * It's an inner class because it can access outer class member vars.
 * 
 *
 */
typedef struct _ClientSideCursorThread
{
    // Outer class reference
    void *m_cscExecutor;

    // Statement context for message loop
    void *m_cscStatementContext;

    THREAD_HANDLE m_hThread;
}ClientSideCursorThread;

#ifdef __cplusplus
extern "C" 
{
#endif /* C++ */

ClientSideCursorThread *createCscThread(MessageLoopState *pMessageLoopState, void *pCscExecutor, void *pCscStatementContext);
ClientSideCursorThread *releaseCscThread(ClientSideCursorThread *pCscThread);

#ifdef WIN32
void
#endif
#if defined LINUX 
void *
#endif
runCscThread(void *pArg);

#ifdef __cplusplus
}
#endif /* C++ */

