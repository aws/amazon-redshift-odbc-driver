/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "ClientSideCursorThread.h"

void readResultsOnThreadCsc(void *_pCscStatementContext);
void resetFromCscThread(void *_pCscExecutor);
void resetCscStatementConext(void *_pCscStatementContext);

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Initialize the ClientSideCursorThread. void * to avoid recursive includes.
//
ClientSideCursorThread *createCscThread(MessageLoopState *pMessageLoopState, void *pCscExecutor, void *pCscStatementContext)
{
    ClientSideCursorThread *pCscThread = rs_calloc(1,sizeof(ClientSideCursorThread));

    if(pCscThread)
    {
        pCscThread->m_cscExecutor = pCscExecutor;
        pCscThread->m_cscStatementContext = pCscStatementContext;
        pCscThread->m_hThread = rsCreateThread(runCscThread,pCscThread);
    }

    return pCscThread;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release ClientSideCursorThread. 
//
ClientSideCursorThread *releaseCscThread(ClientSideCursorThread *pCscThread)
{
    if(pCscThread)
    {
        // Couldn't close the handle because thread terminate also closes it.
        pCscThread->m_hThread = (THREAD_HANDLE)(long) NULL; // paCloseThreadHandle(pCscThread->m_hThread);
        pCscThread = rs_free(pCscThread);
    }

    return NULL;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Run the thread
//
#ifdef WIN32
void
#endif
#if defined LINUX 
void *
#endif
runCscThread(void *pArg)
{
    ClientSideCursorThread *pCscThread = (ClientSideCursorThread *)pArg;

    // Process result
    readResultsOnThreadCsc(pCscThread->m_cscStatementContext);
    checkForIOExceptionCsc(pCscThread->m_cscStatementContext);

    resetFromCscThread(pCscThread->m_cscExecutor);
    resetCscStatementConext(pCscThread->m_cscStatementContext);
    pCscThread->m_cscExecutor = NULL;
    pCscThread->m_cscStatementContext = NULL;
    pCscThread = releaseCscThread(pCscThread);

#ifdef WIN32
    return;
#endif
#if defined LINUX 
    return NULL;
#endif
}
