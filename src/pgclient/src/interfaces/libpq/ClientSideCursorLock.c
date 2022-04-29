/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/ 

#include "ClientSideCursorLock.h"

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Initialize the csc lock
//
ClientSideCursorLock *createCscLock(void)
{
    ClientSideCursorLock *pCscLock = rs_calloc(1,sizeof(ClientSideCursorLock));

    if(pCscLock)
    {
        pCscLock->m_lock = rsCreateMutex();
        
        pCscLock->m_fullResultReadFromServerDoneCondition  = rsCreateBinarySem(0);
        pCscLock->m_fullResultReadFromServerDone = FALSE;
        
        pCscLock->m_batchResultReadFromServerDoneCondition  = rsCreateBinarySem(0);
        pCscLock->m_batchResultReadFromServerDoneConditionStatus = 0;
    }

    return pCscLock;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release resources of the csc lock.
//
ClientSideCursorLock *destroyCscLock(ClientSideCursorLock *pCscLock)
{
    if(pCscLock)
    {
        rsDestroyMutex(pCscLock->m_lock);
        pCscLock->m_lock = NULL;
        
        rsDestroySem(pCscLock->m_fullResultReadFromServerDoneCondition);
        pCscLock->m_fullResultReadFromServerDoneCondition = NULL;

        pCscLock->m_fullResultReadFromServerDone = FALSE;
        
        rsDestroySem(pCscLock->m_batchResultReadFromServerDoneCondition);
        pCscLock->m_batchResultReadFromServerDoneCondition = NULL;
        pCscLock->m_batchResultReadFromServerDoneConditionStatus = -1;

        pCscLock = rs_free(pCscLock);
    }

    return NULL;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Wait for the reading all results from socket.
// Result may have been put in csc file or just skip.
//
void waitForFullResultReadFromServerCscLock(ClientSideCursorLock *pCscLock) 
{
    // Lock the monitor
    rsLockMutex(pCscLock->m_lock);
    
    if(IS_TRACE_ON_CSC())
    {
        traceInfoCsc("waitForFullResultReadFromServer() called");
    }

    if(!pCscLock->m_fullResultReadFromServerDone)
    {
        // Unlock the monitor
        rsUnlockMutex(pCscLock->m_lock);

        // wait for the condition to be signal.
        rsLockSem(pCscLock->m_fullResultReadFromServerDoneCondition);
    }
    else
    {
        // Unlock the monitor
        rsUnlockMutex(pCscLock->m_lock);
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Signal for the reading all results done from socket.
// 
void signalForFullResultReadFromServerCscLock(ClientSideCursorLock *pCscLock) 
{
    // Lock the monitor
    rsLockMutex(pCscLock->m_lock);

    if(IS_TRACE_ON_CSC())
    {
        traceInfoCsc("signalForFullResultReadFromServerCscLock() called");
    }

    // Set the flag
    pCscLock->m_fullResultReadFromServerDone = TRUE;
    
    // signal for any wait thread for full result.
    rsUnlockSem(pCscLock->m_fullResultReadFromServerDoneCondition);
    
    // signal for any wait thread for batch result.
    rsUnlockSem(pCscLock->m_batchResultReadFromServerDoneCondition);
    pCscLock->m_batchResultReadFromServerDoneConditionStatus = 1;

    // Unlock the monitor
    rsUnlockMutex(pCscLock->m_lock);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// true if full result already read from socket, otherwise false.
// 
int isFullResultReadFromServerCscLock(ClientSideCursorLock *pCscLock)
{
    return pCscLock->m_fullResultReadFromServerDone;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Wait for the reading batch results from socket.
// Result may have been put in csc file or just skip.
//
void waitForBatchResultReadFromServerCscLock(ClientSideCursorLock *pCscLock) 
{
    // Lock the monitor
    rsLockMutex(pCscLock->m_lock);
    
    if(IS_TRACE_ON_CSC())
    {
        traceInfoCsc("waitForBatchResultReadFromServer() called");
    }

    if(!pCscLock->m_fullResultReadFromServerDone)
    {
        // Unlock the monitor
        rsUnlockMutex(pCscLock->m_lock);

        // wait for the condition to be signal.
        rsLockSem(pCscLock->m_batchResultReadFromServerDoneCondition);
        pCscLock->m_batchResultReadFromServerDoneConditionStatus = 0;
    }
    else
    {
        // Unlock the monitor
        rsUnlockMutex(pCscLock->m_lock);
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Signal for the reading batch results done from socket.
// 
void signalForBatchResultReadFromServerCscLock(ClientSideCursorLock *pCscLock)
{
    // Lock the monitor
    rsLockMutex(pCscLock->m_lock);
    
    if(IS_TRACE_ON_CSC())
    {
        traceInfoCsc("signalForBatchResultReadFromServer() called");
    }

    // signal for any wait thread for batch result.
    rsUnlockSem(pCscLock->m_batchResultReadFromServerDoneCondition);
    pCscLock->m_batchResultReadFromServerDoneConditionStatus = 1;

    // Unlock the monitor
    rsUnlockMutex(pCscLock->m_lock);
}
