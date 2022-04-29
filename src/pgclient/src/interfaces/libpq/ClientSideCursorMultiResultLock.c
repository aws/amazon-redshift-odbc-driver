/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "ClientSideCursorMultiResultLock.h"
    
/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Initialize the csc lock
//
ClientSideCursorMultiResultLock *createCscMultiResultLock(void)
{
    ClientSideCursorMultiResultLock *pCscMultiResultLock = rs_calloc(1,sizeof(ClientSideCursorMultiResultLock));

    if(pCscMultiResultLock)
    {
        pCscMultiResultLock->m_lock = rsCreateMutex();
        
        pCscMultiResultLock->m_fullResultReadFromServerDoneCondition  = rsCreateBinarySem(0);
        pCscMultiResultLock->m_fullResultReadFromServerDoneConditionStatus = 0;
        pCscMultiResultLock->m_firstBatchInMemResultReadFromServerDoneCondition = rsCreateBinarySem(0);
        pCscMultiResultLock->m_firstBatchInMemResultReadFromServerDoneConditionStatus = 0;
        
        pCscMultiResultLock->m_curExecutionNumber = 0;
        pCscMultiResultLock->m_firstBatchWaitExecutionNumber = 0;
        pCscMultiResultLock->m_firstBatchSignalExecutionNumber = 0;
        pCscMultiResultLock->m_queryDone = FALSE;
    }

    return pCscMultiResultLock;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release resources of the csc lock.
//
ClientSideCursorMultiResultLock *destroyCscMultiResultLock(ClientSideCursorMultiResultLock *pCscMultiResultLock)
{
    if(pCscMultiResultLock)
    {
        rsDestroyMutex(pCscMultiResultLock->m_lock);
        pCscMultiResultLock->m_lock = NULL;
        
        rsDestroySem(pCscMultiResultLock->m_fullResultReadFromServerDoneCondition);
        pCscMultiResultLock->m_fullResultReadFromServerDoneCondition = NULL;
        pCscMultiResultLock->m_fullResultReadFromServerDoneConditionStatus = -1;

        rsDestroySem(pCscMultiResultLock->m_firstBatchInMemResultReadFromServerDoneCondition);
        pCscMultiResultLock->m_firstBatchInMemResultReadFromServerDoneCondition = NULL;
        pCscMultiResultLock->m_firstBatchInMemResultReadFromServerDoneConditionStatus = -1;

        pCscMultiResultLock->m_curExecutionNumber = 0;
        pCscMultiResultLock->m_firstBatchWaitExecutionNumber = 0;
        pCscMultiResultLock->m_queryDone = FALSE;

        pCscMultiResultLock = rs_free(pCscMultiResultLock);
    }

    return NULL;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Wait for the reading all results from socket.
// Result may have been put in csc file or just skip.
//
void waitForFullResultReadFromServerCscMultiResultLock(ClientSideCursorMultiResultLock *pCscMultiResultLock, int resultExecutionNumber) 
{
    // Lock the monitor
    rsLockMutex(pCscMultiResultLock->m_lock);
    
    if(IS_TRACE_ON_CSC())
    {
        traceInfoCsc("waitForFullResultReadFromServer() called. resultExecutionNumber = %d", resultExecutionNumber);
    }

    if(!(pCscMultiResultLock->m_queryDone))
    {
        if(resultExecutionNumber != 0 && resultExecutionNumber == pCscMultiResultLock->m_curExecutionNumber)
        {
            // Unlock the monitor
            rsUnlockMutex(pCscMultiResultLock->m_lock);

            // wait for the condition to be signal.
            rsLockSem(pCscMultiResultLock->m_fullResultReadFromServerDoneCondition);
            pCscMultiResultLock->m_fullResultReadFromServerDoneConditionStatus = 0;
        }
        else
        {
            // Unlock the monitor
            rsUnlockMutex(pCscMultiResultLock->m_lock);
        }
    }
    else
    {
        // Unlock the monitor
        rsUnlockMutex(pCscMultiResultLock->m_lock);
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Signal for the reading all results done from socket.
//
void signalForFullResultReadFromServerCscMultiResultLock(ClientSideCursorMultiResultLock *pCscMultiResultLock, int queryDone) 
{
    // Lock the monitor
    rsLockMutex(pCscMultiResultLock->m_lock);
    
    if(IS_TRACE_ON_CSC())
    {
        traceInfoCsc("signalForFullResultReadFromServerCscMultiResultLock() called. queryDone = %d", queryDone);
    }

    if(queryDone || (pCscMultiResultLock->m_firstBatchWaitExecutionNumber != 0 
                        && (pCscMultiResultLock->m_firstBatchWaitExecutionNumber == pCscMultiResultLock->m_curExecutionNumber)))
    {
        // signal for any wait thread for first result.
        rsUnlockSem(pCscMultiResultLock->m_firstBatchInMemResultReadFromServerDoneCondition);
        pCscMultiResultLock->m_firstBatchWaitExecutionNumber = 0;
        pCscMultiResultLock->m_firstBatchInMemResultReadFromServerDoneConditionStatus = 1;
    }
    
    if(queryDone)
    {
        pCscMultiResultLock->m_curExecutionNumber = 0;
        pCscMultiResultLock->m_queryDone = TRUE;
    }
    else
    {
        (pCscMultiResultLock->m_curExecutionNumber)++;
    }
    
    // signal for any wait thread for full result.
    rsUnlockSem(pCscMultiResultLock->m_fullResultReadFromServerDoneCondition);
    pCscMultiResultLock->m_fullResultReadFromServerDoneConditionStatus = 1;
    
    // Unlock the monitor
    rsUnlockMutex(pCscMultiResultLock->m_lock);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Wait for the reading first batch results from socket.
//
void waitForFirstBatchInMemResultReadFromServerCscMultiResultLock(ClientSideCursorMultiResultLock *pCscMultiResultLock, int resultExecutionNumber) 
{
    // Lock the monitor
    rsLockMutex(pCscMultiResultLock->m_lock);

    if(IS_TRACE_ON_CSC())
    {
        traceInfoCsc("waitForFirstBatchInMemResultReadFromServer() called. resultExecutionNumber = %d", resultExecutionNumber);
    }

    if(!(pCscMultiResultLock->m_queryDone))
    {
        if(resultExecutionNumber != 0 && resultExecutionNumber >= pCscMultiResultLock->m_curExecutionNumber)
        {
            pCscMultiResultLock->m_firstBatchWaitExecutionNumber = resultExecutionNumber;

            if (pCscMultiResultLock->m_firstBatchSignalExecutionNumber != 0 
                    && pCscMultiResultLock->m_firstBatchSignalExecutionNumber < resultExecutionNumber)
            {
                // Reset sems count. i.e. If it's signal then make it non-signal.
                // Reset first batch signal for any previous results. If it's already release then count won't increase than max count of 1.
                if(pCscMultiResultLock->m_firstBatchInMemResultReadFromServerDoneConditionStatus == 1)
                {
                    rsLockSem(pCscMultiResultLock->m_firstBatchInMemResultReadFromServerDoneCondition);
                    pCscMultiResultLock->m_firstBatchInMemResultReadFromServerDoneConditionStatus = 0;
                }
            }

            // Unlock the monitor
            rsUnlockMutex(pCscMultiResultLock->m_lock);

            // wait for the condition to be signal.
            rsLockSem(pCscMultiResultLock->m_firstBatchInMemResultReadFromServerDoneCondition);

            rsLockMutex(pCscMultiResultLock->m_lock);
            pCscMultiResultLock->m_firstBatchInMemResultReadFromServerDoneConditionStatus = 0;
            rsUnlockMutex(pCscMultiResultLock->m_lock);
        }
        else
        {
            // Unlock the monitor
            rsUnlockMutex(pCscMultiResultLock->m_lock);
        }
    }
    else
    {
        // Unlock the monitor
        rsUnlockMutex(pCscMultiResultLock->m_lock);
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Signal for the reading first batch results done from socket.
//
void signalForFirstBatchInMemResultReadFromServerCscMultiResultLock(ClientSideCursorMultiResultLock *pCscMultiResultLock, int resultExecutionNumber)  
{
    // Lock the monitor
    rsLockMutex(pCscMultiResultLock->m_lock);
    
    if(IS_TRACE_ON_CSC())
    {
        traceInfoCsc("signalForFirstBatchInMemResultReadFromServerCscMultiResultLock() called");
    }

    pCscMultiResultLock->m_firstBatchSignalExecutionNumber = resultExecutionNumber;
    
    // signal for any wait thread for full result.
    rsUnlockSem(pCscMultiResultLock->m_firstBatchInMemResultReadFromServerDoneCondition);
    pCscMultiResultLock->m_firstBatchInMemResultReadFromServerDoneConditionStatus = 1;

    // Unlock the monitor
    rsUnlockMutex(pCscMultiResultLock->m_lock);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set the current execution number.
//
void setResultExecutionNumberCscMultiResultLock(ClientSideCursorMultiResultLock *pCscMultiResultLock, int curExecutionNumber)
{
    // Lock the monitor
    rsLockMutex(pCscMultiResultLock->m_lock);
    
    pCscMultiResultLock->m_curExecutionNumber = curExecutionNumber;

    // Unlock the monitor
    rsUnlockMutex(pCscMultiResultLock->m_lock);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get the current execution number.
//
int getResultExecutionNumberCscMultiResultLock(ClientSideCursorMultiResultLock *pCscMultiResultLock)
{
    int curExecutionNumber;

    // Lock the monitor
    rsLockMutex(pCscMultiResultLock->m_lock);
    
    curExecutionNumber = pCscMultiResultLock->m_curExecutionNumber;

    // Unlock the monitor
    rsUnlockMutex(pCscMultiResultLock->m_lock);

    return curExecutionNumber;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Reset vars at start of execution.
//
void resetCscMultiResultLock(ClientSideCursorMultiResultLock *pCscMultiResultLock)
{
    // Lock the monitor
    rsLockMutex(pCscMultiResultLock->m_lock);
    
    pCscMultiResultLock->m_curExecutionNumber = 0;
    pCscMultiResultLock->m_firstBatchWaitExecutionNumber = 0;
    pCscMultiResultLock->m_firstBatchSignalExecutionNumber = 0;
    pCscMultiResultLock->m_queryDone = FALSE;

    // Reset sem count. i.e. If it's signal then make it non-signal.
    if(pCscMultiResultLock->m_firstBatchInMemResultReadFromServerDoneConditionStatus == 1)
    {
        rsLockSem(pCscMultiResultLock->m_firstBatchInMemResultReadFromServerDoneCondition);
        pCscMultiResultLock->m_firstBatchInMemResultReadFromServerDoneConditionStatus = 0;
    }

    // Reset sem count. i.e. If it's signal then make it non-signal.
    if(pCscMultiResultLock->m_fullResultReadFromServerDoneConditionStatus == 1)
    {
        rsLockSem(pCscMultiResultLock->m_fullResultReadFromServerDoneCondition);
        pCscMultiResultLock->m_fullResultReadFromServerDoneConditionStatus = 0;
    }

    // Unlock the monitor
    rsUnlockMutex(pCscMultiResultLock->m_lock);
}
