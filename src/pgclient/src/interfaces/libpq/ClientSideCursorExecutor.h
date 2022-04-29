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

#include "ClientSideCursorResult.h"
#include "MessageLoopState.h"
#include "ClientSideCursorThread.h"


/** Act as a CSC factory. We have the one object per connection.
 *
 */
typedef struct _ClientSideCursorExecutor
{
    // If csc is enable we will create the object to save all options related to csc.
    ClientSideCursorOptions *m_cscOptions;

    // Last running csc thread.
    ClientSideCursorThread *m_cscThread;
    int m_cscStopThread;

    // Parent object who created CSC
    PGconn *m_conn;
}ClientSideCursorExecutor;

#ifdef __cplusplus
extern "C" 
{
#endif /* C++ */

ClientSideCursorExecutor *createCscExecutor(PGconn *pConn);
ClientSideCursorExecutor *releaseCscExecutor(ClientSideCursorExecutor *pCscExecutor);
PGconn *getCscConnection(ClientSideCursorExecutor *pCscExecutor);
int getCscStopThreadFlag(void *_pCscExecutor);
ClientSideCursorResult *createCSCResultObj(void *_pCscExecutor, int maxRows, int resultsettype, int executeIndex,int resultsetconcurrency);

void waitForCscThreadToFinish(ClientSideCursorExecutor *pCscExecutor, int calledFromConnectionClose);
void waitForCscNextResult(ClientSideCursorExecutor *pCscExecutor, int resultExecutionNumber, MessageLoopState *pMessageLoopState);
ClientSideCursorMultiResultLock *createOrResetCscMultiResultLock(void *_pCscExecutor, int multiThreads, 
                                                                 ClientSideCursorMultiResultLock *pCscMultiResultLock);

void readResultsOnThreadCsc(void *_pCscStatementContext);
int createProcessingThreadCsc(ClientSideCursorExecutor *pCscExecutor, MessageLoopState *pMessageLoopState, 
                                ClientSideCursorResult *pCscResult, PGresult *_pgResult, CscStatementContext *pCscStatementContext);
void endOfQueryResponseCsc(ClientSideCursorExecutor *pCscExecutor, MessageLoopState *pMessageLoopState);
void resetFromCscThread(void *_pCscExecutor);

void pgWaitForCscNextResult(void *_pCscStatementContext, PGresult * res, int resultExecutionNumber);
void pgWaitForCscThreadToFinish(PGconn *conn, int calledFromConnectionClose);


#ifdef __cplusplus
}
#endif /* C++ */

