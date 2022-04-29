/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "ClientSideCursorExecutor.h"
#include "ClientSideCursorThread.h"

void _pqGetResultLoopOnThread(void *_pCscStatementContext);
void setCalledFromCscThread(void *_pCscStatementContext,int flag);
PGconn *getPgConnCsc(void *_pCscStatementContext);
MessageLoopState *getPgMsgLoopCsc(void *_pCscStatementContext);


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Initialize the csc executor
//
ClientSideCursorExecutor *createCscExecutor(PGconn *pConn)
{
    ClientSideCursorExecutor *pCscExecutor = rs_calloc(1,sizeof(ClientSideCursorExecutor));

    if(pCscExecutor)
    {
        if (pConn->iCscEnable)
        {
            pCscExecutor->m_cscOptions = createCscOptions(pConn->iCscEnable, pConn->llCscThreshold, pConn->llCscMaxFileSize, pConn->szCscPath);
        }

        pCscExecutor->m_conn = pConn;
    }

    return pCscExecutor;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release the csc executor
//
ClientSideCursorExecutor *releaseCscExecutor(ClientSideCursorExecutor *pCscExecutor)
{
    if(pCscExecutor)
    {
        pCscExecutor->m_cscOptions = releaseCscOptions(pCscExecutor->m_cscOptions);

        pCscExecutor = rs_free(pCscExecutor);
    }

    return NULL;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get connection object associated with CSC
//
PGconn *getCscConnection(ClientSideCursorExecutor *pCscExecutor)
{
    return pCscExecutor->m_conn;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Create csc result object. One per result.
//
ClientSideCursorResult *createCSCResultObj(void *_pCscExecutor, int maxRows, int resultsettype, int executeIndex, int resultsetconcurrency)
{
    ClientSideCursorExecutor *pCscExecutor = (ClientSideCursorExecutor *)_pCscExecutor;
    ClientSideCursorResult *pCscResult;

    pCscResult = (((pCscExecutor->m_cscOptions != NULL)
                        && (pCscExecutor->m_cscOptions->m_enable)
                        && (resultsetconcurrency == SQL_CONCUR_READ_ONLY))
                    ? createCscResult(pCscExecutor->m_cscOptions, maxRows, pCscExecutor->m_conn->pghost,
                        pCscExecutor->m_conn->be_pid, NULL, resultsettype, executeIndex) 
                    : NULL);

    return pCscResult;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Wait for csc thread to finish.
//
void pgWaitForCscThreadToFinish(PGconn *conn, int calledFromConnectionClose)
{
    if(conn && conn->m_pCscExecutor)
    {
        waitForCscThreadToFinish(conn->m_pCscExecutor, calledFromConnectionClose);
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Wait for csc thread to finish.
//
void waitForCscThreadToFinish(ClientSideCursorExecutor *pCscExecutor, int calledFromConnectionClose)
{
    // Wait for full read of any executing command
    if(pCscExecutor->m_cscThread != NULL)
    {
        if (calledFromConnectionClose)
        {
            pCscExecutor->m_cscStopThread = TRUE;
        }

        rsJoinThread(pCscExecutor->m_cscThread->m_hThread);

        if (calledFromConnectionClose)
        {
            pCscExecutor->m_cscStopThread = FALSE;
        }
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Wait for next result.
//
void pgWaitForCscNextResult(void *_pCscStatementContext, PGresult * res, int resultExecutionNumber)
{
    if(_pCscStatementContext)
    {
        PGconn *conn = getPgConnCsc(_pCscStatementContext);
        ClientSideCursorExecutor *pCscExecutor = conn->m_pCscExecutor;
        MessageLoopState *pMessageLoopState = getPgMsgLoopCsc(_pCscStatementContext);

        if(pCscExecutor)
            waitForCscNextResult(pCscExecutor, resultExecutionNumber, pMessageLoopState);
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Wait for next result.
//
void waitForCscNextResult(ClientSideCursorExecutor *pCscExecutor, int resultExecutionNumber, MessageLoopState *pMessageLoopState)
{
    // Is thread running?
    if (!pMessageLoopState->endQuery && pCscExecutor->m_cscThread != NULL)
    {
        if (resultExecutionNumber > 0)
        {
            // Check current execution number
            if (pMessageLoopState->m_cscMultiResultLock != NULL)
            {
                int curExecutionNumber = getResultExecutionNumberCscMultiResultLock(pMessageLoopState->m_cscMultiResultLock);

                // Still result read from socket going on for the same result?
                if (resultExecutionNumber == curExecutionNumber)
                {
                    // Drain current result
                    setSkipResultNumberCsc(pMessageLoopState, resultExecutionNumber);

                    // wait for current result to be done
                    waitForFullResultReadFromServerCscMultiResultLock(pMessageLoopState->m_cscMultiResultLock, resultExecutionNumber);
                }

                if (curExecutionNumber <= (resultExecutionNumber + 1))
                {
                    // wait for in memory result to be ready for next result
                    waitForFirstBatchInMemResultReadFromServerCscMultiResultLock(pMessageLoopState->m_cscMultiResultLock, resultExecutionNumber + 1);
                }

                setSkipResultNumberCsc(pMessageLoopState,0);
            }
            else
            {
                waitForCscThreadToFinish(pCscExecutor, FALSE);
            }
        }
        // result number
        else
        {
            waitForCscThreadToFinish(pCscExecutor, FALSE);
        }
    } // Thread check
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Create multi result lock to navigate multiple results.
//
ClientSideCursorMultiResultLock *createOrResetCscMultiResultLock(void *_pCscExecutor, int multiThreads, 
                                                                 ClientSideCursorMultiResultLock *pCscMultiResultLock)
{
    ClientSideCursorExecutor *pCscExecutor = (ClientSideCursorExecutor *)_pCscExecutor;

    if (pCscExecutor->m_cscOptions != NULL && pCscExecutor->m_cscOptions->m_enable && multiThreads)
    {
        if (pCscMultiResultLock == NULL)
        {
            pCscMultiResultLock = createCscMultiResultLock();
        }
        else
        {
            resetCscMultiResultLock(pCscMultiResultLock);
        }
    }
    else
    {
        pCscMultiResultLock = NULL;
    }

    return pCscMultiResultLock;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Process rows on a thread.
//
// 
void readResultsOnThreadCsc(void *_pCscStatementContext)
{
    setCalledFromCscThread(_pCscStatementContext, TRUE);
    _pqGetResultLoopOnThread(_pCscStatementContext);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Create processing thread to fetch rows from the socket.
//
int createProcessingThreadCsc(ClientSideCursorExecutor *pCscExecutor, MessageLoopState *pMessageLoopState, 
                                ClientSideCursorResult *pCscResult, PGresult *_pgResult, CscStatementContext *pCscStatementContext)
{
    int rc = FALSE;

    if (pCscResult != NULL && pCscResult->m_fileCreated)
    {
        // create lock
        createLockCsc(pCscResult);
    }

    // Return result
    pMessageLoopState->pgResult = _pgResult;
    if(pMessageLoopState->pgResult)
        pMessageLoopState->pgResult->m_cscResult = pCscResult;

//  First result we return using normal execution path
    if(pCscStatementContext->m_pResultHandlerCallbackFunc
        && pCscStatementContext->m_calledFromCscThread)
    {
        // Return the rows
        (*pCscStatementContext->m_pResultHandlerCallbackFunc)(pCscStatementContext->m_pCallerContext,
                                                                pMessageLoopState->pgResult); 
    } 

    pMessageLoopState->rowsInMemReturned = TRUE;

    // Don't increase the index.
    if(pMessageLoopState->m_cscMultiResultLock != NULL)
    {
        // Signal first batch is ready for multi result.
        signalForFirstBatchInMemResultReadFromServerCscMultiResultLock(pMessageLoopState->m_cscMultiResultLock,pMessageLoopState->executeIndex + 1);
    }

    // Only create one thread
    if(!pMessageLoopState->threadCreated)
    {
        pMessageLoopState->threadCreated = TRUE;

        // Create new thread and start it to read rest of rows from socket
        pCscExecutor->m_cscThread = createCscThread(pMessageLoopState, pCscExecutor, pCscStatementContext);

        //                        Simulate thread error.                        
        //                        pCscExecutor->m_cscThread = NULL;

        if(pCscExecutor->m_cscThread == NULL)
        {
            pMessageLoopState->threadCreated = FALSE;

            // Reset the lock, otherwise close() will hang for ever.
            resetLockCsc(pCscResult);

            if(IS_TRACE_ON_CSC())
            {
                traceInfoCsc("Couldn't create more thread object for CSC.");
            }

            return rc;
        }


        rc = TRUE;
    }

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// 'Z' ReadyForQuery received
//
void endOfQueryResponseCsc(ClientSideCursorExecutor *pCscExecutor, MessageLoopState *pMessageLoopState)
{
    ClientSideCursorResult *pCscResult = pMessageLoopState->m_cscResult;

    pMessageLoopState->endQuery = TRUE;

    pMessageLoopState->pgResult = NULL;
    pMessageLoopState->executeIndex = 0;

    // For Cancel sometime directly "Z" (i.e. EOQ) will come and it won't send "C" (i.e. EOC).
    if(pCscResult != NULL)
    {
        closeOutputFileCsc(pCscResult);

        if(pMessageLoopState->threadCreated)
        {
            // Signal end of result
            signalForFullResultReadFromServerCsc(pCscResult);
        }
    }

    // Release last created csc result in resetAfter and not used.
    pMessageLoopState->m_cscResult = releaseCsc(pMessageLoopState->m_cscResult);
    pCscResult = NULL;
    pMessageLoopState->m_skipAllResults = FALSE;
    pMessageLoopState->m_skipResultNumber = 0;
    if (pMessageLoopState->m_cscMultiResultLock != NULL)
    {
        signalForFullResultReadFromServerCscMultiResultLock(pMessageLoopState->m_cscMultiResultLock,TRUE);
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// 'Z' ReadyForQuery received
//
void resetFromCscThread(void *_pCscExecutor)
{
    ClientSideCursorExecutor *pCscExecutor = (ClientSideCursorExecutor *)_pCscExecutor;

    pCscExecutor->m_cscThread = NULL;
    pCscExecutor->m_cscStopThread = FALSE;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get the thread stop flag
//
int getCscStopThreadFlag(void *_pCscExecutor)
{
    ClientSideCursorExecutor *pCscExecutor = (ClientSideCursorExecutor *)_pCscExecutor;

    return pCscExecutor->m_cscStopThread;
}
