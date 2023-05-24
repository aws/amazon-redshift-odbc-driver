/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "MessageLoopState.h"

ClientSideCursorResult *createCSCResultObj(void *_pCscExecutor, int maxRows, int resultsettype, int executeIndex, int resultsetconcurrency);
ClientSideCursorMultiResultLock *createOrResetCscMultiResultLock(void *_pCscExecutor, int multiThreads, 
                                                                 ClientSideCursorMultiResultLock *pCscMultiResultLock);



/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Initialize the MessageLoopState with defaults
//
MessageLoopState *createDfltMessageLoopStateCsc(void)
{
    MessageLoopState *pMessageLoopState = rs_calloc(1,sizeof(MessageLoopState));

    if(pMessageLoopState)
    {
        initMessageLoopStateCsc(pMessageLoopState, NULL,  0, SQL_CURSOR_FORWARD_ONLY, FALSE, NULL, SQL_CONCUR_READ_ONLY);
    }

    return pMessageLoopState;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Initialize the MessageLoopState with given arguments
//
MessageLoopState *createMessageLoopStateCsc(void *pCscExecutor, int _maxRows, int _resultsettype, int _multiThreads, PGresult *pgResult,
                                            int _resultsetconcurrency)
{
    MessageLoopState *pMessageLoopState = rs_calloc(1,sizeof(MessageLoopState));

    if(pMessageLoopState)
    {
        initMessageLoopStateCsc(pMessageLoopState, pCscExecutor, _maxRows, _resultsettype, _multiThreads, pgResult, _resultsetconcurrency);
    }

    return pMessageLoopState;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release  MessageLoopState 
//
MessageLoopState *releaseMessageLoopStateCsc(MessageLoopState *pMessageLoopState)
{
    if(pMessageLoopState)
    {
        pMessageLoopState->m_cscMultiResultLock = destroyCscMultiResultLock(pMessageLoopState->m_cscMultiResultLock);

        pMessageLoopState = rs_free(pMessageLoopState);
    }

    return pMessageLoopState;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Initialize the object before starting the run.
//
void initMessageLoopStateCsc(MessageLoopState *pMessageLoopState, void *pCscExecutor, int _maxRows, int _resultsettype,
                            int _multiThreads, PGresult *_pgResult, int _resultsetconcurrency)
{
    pMessageLoopState->maxRows = _maxRows;
    pMessageLoopState->resultsettype = _resultsettype;
    pMessageLoopState->resultsetconcurrency = _resultsetconcurrency;
    pMessageLoopState->pgResult = _pgResult;
    pMessageLoopState->endQuery = FALSE;

    pMessageLoopState->executeIndex = 0;
    if(pCscExecutor != NULL)
    {
        // Create CSC result if it's enable.
        pMessageLoopState->m_cscResult = createCSCResultObj(pCscExecutor,pMessageLoopState->maxRows, pMessageLoopState->resultsettype, pMessageLoopState->executeIndex, pMessageLoopState->resultsetconcurrency);
    }
    else
    {
        pMessageLoopState->m_cscResult = NULL;
    }

    pMessageLoopState->threadCreated = FALSE;

    pMessageLoopState->multiThreads = _multiThreads;
    pMessageLoopState->rowsInMemReturned = FALSE;

    // Create global lock for multi result.
    if(pCscExecutor != NULL)
    {
        pMessageLoopState->m_cscMultiResultLock = createOrResetCscMultiResultLock(pCscExecutor,_multiThreads, pMessageLoopState->m_cscMultiResultLock);
        if (pMessageLoopState->m_cscMultiResultLock != NULL)
        {
            // Set processing first result.
            setResultExecutionNumberCscMultiResultLock(pMessageLoopState->m_cscMultiResultLock, 1);
        }
    }
    else
    {
        pMessageLoopState->m_cscMultiResultLock = NULL;
    }

    pMessageLoopState->m_skipResultNumber = 0;
    pMessageLoopState->m_skipAllResults = FALSE;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set the global flag for skipping all pending results.
//
void pgSetSkipAllResultsCsc(void *_pCscStatementContext, int val)
{
    struct _CscStatementContext *pCscStatementContext = (struct _CscStatementContext *)_pCscStatementContext;
    MessageLoopState *pMessageLoopState = pCscStatementContext->m_pMessageLoopState;

    if(pMessageLoopState)
        setSkipAllResultsCsc(pMessageLoopState, val);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set the global flag for skipping all pending results.
//
void setSkipAllResultsCsc(MessageLoopState *pMessageLoopState, int val)
{
    pMessageLoopState->m_skipAllResults = val;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Skip the given result number.
//
void setSkipResultNumberCsc(MessageLoopState *pMessageLoopState, int skipResultNumber)
{
    pMessageLoopState->m_skipResultNumber = skipResultNumber;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get csc result object.
//
ClientSideCursorResult *getCscResult(MessageLoopState *pMessageLoopState)
{
    return pMessageLoopState->m_cscResult;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set csc result object.
//
void setCscResult(MessageLoopState *pMessageLoopState, ClientSideCursorResult *_cscResult)
{
    pMessageLoopState->m_cscResult = _cscResult;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get memory cache vector.
//
PGresult *getResultHandleCsc(MessageLoopState *pMessageLoopState)
{
    return pMessageLoopState->pgResult;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set memory cache vector.
//
void setResultHandleCsc(MessageLoopState *pMessageLoopState, PGresult *_pgResult)
{
    pMessageLoopState->pgResult = _pgResult;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get multi thread flag.
//
int getMultiThreadsCsc(MessageLoopState *pMessageLoopState)
{
    return pMessageLoopState->multiThreads;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get rows in mem flag.
//
int getRowsInMemReturnedCsc(MessageLoopState *pMessageLoopState)
{
    return pMessageLoopState->rowsInMemReturned;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set rows in mem flag.
//
void setRowsInMemReturnedCsc(MessageLoopState *pMessageLoopState, int _rowsInMemReturned)
{
    pMessageLoopState->rowsInMemReturned = _rowsInMemReturned;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get thread created flag.
//
int getThreadCreatedCsc(MessageLoopState *pMessageLoopState)
{
    return pMessageLoopState->threadCreated;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set thread created flag.
//
void setThreadCreatedCsc(MessageLoopState *pMessageLoopState, int _threadCreated)
{
    pMessageLoopState->threadCreated = _threadCreated;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Reset variables after result read finish.
//
ClientSideCursorResult *resetAfterOneResultReadFromServerFinishCsc(MessageLoopState *pMessageLoopState, void *pCscExecutor, int index, 
                                                                    ClientSideCursorResult *pNextCscResult)
{
    pMessageLoopState->pgResult = NULL;
    pMessageLoopState->rowsInMemReturned = FALSE;
    
    if(pMessageLoopState->m_cscResult != NULL)
    {
        closeOutputFileCsc(pMessageLoopState->m_cscResult);

        if(pMessageLoopState->threadCreated)
        {
            // Signal end of result
            signalForFullResultReadFromServerCsc(pMessageLoopState->m_cscResult);

            if(pMessageLoopState->m_cscMultiResultLock != NULL)
            {
                // Send signal for multi result
                signalForFullResultReadFromServerCscMultiResultLock(pMessageLoopState->m_cscMultiResultLock, FALSE);
            }
        }
        else
        {
            if(pMessageLoopState->m_cscMultiResultLock != NULL)
            {
                // Set processing for next result.
                setResultExecutionNumberCscMultiResultLock(pMessageLoopState->m_cscMultiResultLock,
                                                            getResultExecutionNumberCscMultiResultLock(pMessageLoopState->m_cscMultiResultLock) + 1);
            }
        }
        
        pMessageLoopState->m_cscResult = NULL;
    }

    if(pNextCscResult == NULL)
    {
        if (pCscExecutor != NULL)
        {
            pMessageLoopState->m_cscResult = createCSCResultObj(pCscExecutor,pMessageLoopState->maxRows, pMessageLoopState->resultsettype, index, pMessageLoopState->resultsetconcurrency);
        }
        else
        {
            pMessageLoopState->m_cscResult = NULL;
        }
    }
    else
        pMessageLoopState->m_cscResult = pNextCscResult;
    
    setSkipResultNumberCsc(pMessageLoopState,0);
    
    return pMessageLoopState->m_cscResult;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Initialize the statement context
//
struct _CscStatementContext *createCscStatementContext(void *pCallerContext, CSC_RESULT_HANDLER_CALLBACK *pResultHandlerCallbackFunc, 
                                                        PGconn *conn, int iUseMsgLoop)
{
    struct _CscStatementContext *pCscStatementContext = (struct _CscStatementContext *) rs_calloc(1,sizeof(struct _CscStatementContext));

    if(pCscStatementContext)
    {
        pCscStatementContext->m_pCallerContext = pCallerContext;
        pCscStatementContext->m_pResultHandlerCallbackFunc = pResultHandlerCallbackFunc;
        pCscStatementContext->m_pgConn = conn;

        if(iUseMsgLoop)
            pCscStatementContext->m_pMessageLoopState = createDfltMessageLoopStateCsc();
    }

    return pCscStatementContext;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release  MessageLoopState 
//
struct _CscStatementContext *releaseCscStatementContext(struct _CscStatementContext *pCscStatementContext)
{
    if(pCscStatementContext)
    {
        pCscStatementContext->m_pMessageLoopState = releaseMessageLoopStateCsc(pCscStatementContext->m_pMessageLoopState);
        pCscStatementContext = rs_free(pCscStatementContext);
    }

    return pCscStatementContext;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set PG connection in context
//
void setPgConnCsc(struct _CscStatementContext *pCscStatementContext, PGconn *conn)
{
    pCscStatementContext->m_pgConn = conn;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get PG connection in context
//
PGconn *getPgConnCsc(void *_pCscStatementContext)
{
    struct _CscStatementContext *pCscStatementContext = (struct _CscStatementContext *)_pCscStatementContext;

    return pCscStatementContext->m_pgConn;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get message loop context
//
MessageLoopState *getPgMsgLoopCsc(void *_pCscStatementContext)
{
    struct _CscStatementContext *pCscStatementContext = (struct _CscStatementContext *)_pCscStatementContext;

    return pCscStatementContext->m_pMessageLoopState;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set thread called flag in context object.
//
void setCalledFromCscThread(void *_pCscStatementContext,int flag)
{
    struct _CscStatementContext *pCscStatementContext = (struct _CscStatementContext *)_pCscStatementContext;

    pCscStatementContext->m_calledFromCscThread = flag;
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get thread created flag in context object.
//
int getCscThreadCreatedFlag(void *_pCscStatementContext)
{
    struct _CscStatementContext *pCscStatementContext = (struct _CscStatementContext *)_pCscStatementContext;

    return pCscStatementContext->m_cscThreadCreated;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set thread created flag in context object.
//
void setCscThreadCreatedFlag(void *_pCscStatementContext, int flag)
{
    struct _CscStatementContext *pCscStatementContext = (struct _CscStatementContext *)_pCscStatementContext;

    pCscStatementContext->m_cscThreadCreated = flag;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set initialized flag
//
void setCscInitMsgLoopStateFlag(void *_pCscStatementContext, int flag)
{
    struct _CscStatementContext *pCscStatementContext = (struct _CscStatementContext *)_pCscStatementContext;

    pCscStatementContext->m_alreadyInitMsgLoopState = flag;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set initialized flag
//
void setStreamingCursorRows(void *_pCscStatementContext, int iStreamingCursorRows)
{
    struct _CscStatementContext *pCscStatementContext = (struct _CscStatementContext *)_pCscStatementContext;

    pCscStatementContext->m_StreamingCursorInfo.m_streamingCursorRows = iStreamingCursorRows;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Reset end of streaming cursor
//
void setEndOfStreamingCursor(void *_pCscStatementContext, int flag)
{
    struct _CscStatementContext *pCscStatementContext = (struct _CscStatementContext *)_pCscStatementContext;

    pCscStatementContext->m_StreamingCursorInfo.m_endOfStreamingCursor = flag;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get streaming cursor end indicator.
//
int isEndOfStreamingCursor(void *_pCscStatementContext)
{
    struct _CscStatementContext *pCscStatementContext = (struct _CscStatementContext *)_pCscStatementContext;

    return pCscStatementContext->m_StreamingCursorInfo.m_endOfStreamingCursor;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Reset end of streaming cursor query
//
void setEndOfStreamingCursorQuery(void *_pCscStatementContext, int flag)
{
    struct _CscStatementContext *pCscStatementContext = (struct _CscStatementContext *)_pCscStatementContext;

    pCscStatementContext->m_StreamingCursorInfo.m_endOfStreamingCursorQuery = flag;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Reset m_streamResultBatchNumber (C)
//
void resetStreamingCursorBatchNumber(void *_pCscStatementContext)
{
    struct _CscStatementContext *pCscStatementContext = (struct _CscStatementContext *)_pCscStatementContext;

    pCscStatementContext->m_StreamingCursorInfo.m_streamResultBatchNumber = 0;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get streaming cursor query end indicator. (Z)
//
int isEndOfStreamingCursorQuery(void *_pCscStatementContext)
{
    struct _CscStatementContext *pCscStatementContext = (struct _CscStatementContext *)_pCscStatementContext;

    return pCscStatementContext->m_StreamingCursorInfo.m_endOfStreamingCursorQuery;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set skip of streaming cursor
//
void setSkipResultOfStreamingCursor(void *_pCscStatementContext, int flag)
{
    struct _CscStatementContext *pCscStatementContext = (struct _CscStatementContext *)_pCscStatementContext;

    pCscStatementContext->m_StreamingCursorInfo.m_skipStreamingCursor = flag;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Reset m_streamResultBatchNumber (C)
//
int getStreamingCursorBatchNumber(void *_pCscStatementContext)
{
    struct _CscStatementContext *pCscStatementContext = (struct _CscStatementContext *)_pCscStatementContext;

    return pCscStatementContext->m_StreamingCursorInfo.m_streamResultBatchNumber;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Reset statement context for new query execution
//
void resetCscStatementConext(void *_pCscStatementContext)
{
    // Reset thread called flag
    setCalledFromCscThread(_pCscStatementContext,FALSE);

    // Reset init msg loop flag
    setCscInitMsgLoopStateFlag(_pCscStatementContext,FALSE);

	// Reset Streaming cursor
	resetStreamingCursorStatementConext(_pCscStatementContext);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Reset statement context for new query execution for streaming cursor
//
void resetStreamingCursorStatementConext(void *_pCscStatementContext)
{
	// Reset End Of Streaming Cursor
	setEndOfStreamingCursor(_pCscStatementContext,FALSE);

	// Reset stream batch number
	resetStreamingCursorBatchNumber(_pCscStatementContext);

	// Reset End Of Streaming Cursor query
	setEndOfStreamingCursorQuery(_pCscStatementContext,FALSE);

	// Reset skip flag
	setSkipResultOfStreamingCursor(_pCscStatementContext,FALSE);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Check for IO exception after csc thread about to terminate.
//
void  checkForIOExceptionCsc(void *_pCscStatementContext)
{
    struct _CscStatementContext *pCscStatementContext = (struct _CscStatementContext *)_pCscStatementContext;

    if(pCscStatementContext)  
    {
        MessageLoopState *pMessageLoopState = pCscStatementContext->m_pMessageLoopState;

        if(pMessageLoopState && pMessageLoopState->m_cscResult)
        {
            if(pCscStatementContext->iSocketError)
            {
                setIOExceptionCsc(pMessageLoopState->m_cscResult, pCscStatementContext->iSocketError);
            }
            else
            if(!(pMessageLoopState->endQuery))
            {
                // Signal  full result read. This must be either cancel condition or error condition.
                signalForFullResultReadFromServerCsc(pMessageLoopState->m_cscResult);
            }
        }
    }
}

void resetAfterOneResultReadFromServerFinishForStreamingCursor(struct _CscStatementContext *pCscStatementContext, PGconn *conn)
{
	// Indicate end of the rows
    pCscStatementContext->m_StreamingCursorInfo.m_endOfStreamingCursor = TRUE;

    // We need to free it and make it null because, we already pass the result back to caller.
    if(pCscStatementContext->m_StreamingCursorInfo.m_streamResultBatchNumber > 0)
    {
        if(conn->result)
        {
            PQclear(conn->result);
        }
        conn->result = NULL;
    }
}

