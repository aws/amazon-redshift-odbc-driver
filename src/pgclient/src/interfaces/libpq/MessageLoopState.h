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
#if defined LINUX 
#include <unistd.h>
#endif

#include <stdio.h>

#include "ClientSideCursorResult.h"
#include "ClientSideCursorMultiResultLock.h"

/** Keep the state of the rows loop for csc to work on separate thread.
 * This is use in processResult(). It store all local vars of processResult() methods,
 * so it can process in multiple threads.
 *
 */
typedef struct _MessageLoopState
{
	int maxRows;
    int resultsettype;
    int resultsetconcurrency;
    PGresult *pgResult;
	int endQuery;
	
	int executeIndex;
	
	int multiThreads;
	
	// Create CSC result if it's enable.
	ClientSideCursorResult *m_cscResult;
	int threadCreated;
	int rowsInMemReturned;
	
	ClientSideCursorMultiResultLock *m_cscMultiResultLock;
	int m_skipResultNumber; // Skip the result. 1..N
	int m_skipAllResults; // Skip all results.
}MessageLoopState;

// Call back function. Define in paconnect.c also.
typedef int CSC_RESULT_HANDLER_CALLBACK(void *pCallerContext, PGresult *pgResult);

// Streaming cursor information needed during query processing and result fetching
typedef struct _StreamingCursorInfo
{
	int	  m_streamingCursorRows; // > 0 means streaming cursor with number of rows i.e. SC, 0 means CSC
	int	  m_endOfStreamingCursor; // TRUE means end of current result cursor. FALSE means more rows to read from socket
	int	  m_streamResultBatchNumber; // 0 means we haven't stop the loop bcoz rows exceeds batch count.
	int	  m_endOfStreamingCursorQuery; // End of all result indicator
	int   m_skipStreamingCursor; // 0 means no skip, 1 means skip current result
}StreamingCursorInfo;


// Libpq don't have statement/command context. We need one for ODBC and CSC support.
// It will be one per statement.
typedef struct _CscStatementContext
{
    void *m_pCallerContext; // Caller object context
    CSC_RESULT_HANDLER_CALLBACK *m_pResultHandlerCallbackFunc; // Callback function get called once one result is ready
    MessageLoopState *m_pMessageLoopState; // MessageLoop status object. NULL means not interested in CSC.
    PGconn *m_pgConn; // PG Connection object
    int   iSocketError;
    int   m_cscThreadCreated; // 1: When thread created, so we can break the loop
    int   m_calledFromCscThread; // 1 means loop is called from csc thread.
    int   m_alreadyInitMsgLoopState; // 1 means already initialize the message loop state for this query execution.

	StreamingCursorInfo m_StreamingCursorInfo; // Streaming cursor information

}CscStatementContext;

#ifdef __cplusplus
extern "C" 
{
#endif /* C++ */

MessageLoopState*createDfltMessageLoopStateCsc(void);
MessageLoopState*createMessageLoopStateCsc(void *pCscExecutor, int _maxRows, int _resultsettype, int _multiThreads, PGresult *pgResult, int _resultsetconcurrency);
MessageLoopState*releaseMessageLoopStateCsc(MessageLoopState*pMessageLoopState);
void initMessageLoopStateCsc(MessageLoopState*pMessageLoopState, void *pCscExecutor, int _maxRows, int _resultsettype,
                            int _multiThreads, PGresult *_pgResult, int _resultsetconcurrency);

void setSkipAllResultsCsc(MessageLoopState*pMessageLoopState, int val);
void setSkipResultNumberCsc(MessageLoopState*pMessageLoopState, int skipResultNumber);
ClientSideCursorResult *getCscResult(MessageLoopState*pMessageLoopState);
void setCscResult(MessageLoopState*pMessageLoopState, ClientSideCursorResult *_cscResult);
PGresult *getResultHandleCsc(MessageLoopState*pMessageLoopState);
void setResultHandleCsc(MessageLoopState*pMessageLoopState, PGresult *_pgResult);
int getMultiThreadsCsc(MessageLoopState*pMessageLoopState);
int getRowsInMemReturnedCsc(MessageLoopState*pMessageLoopState);
void setRowsInMemReturnedCsc(MessageLoopState*pMessageLoopState, int _rowsInMemReturned);
int getThreadCreatedCsc(MessageLoopState*pMessageLoopState);
void setThreadCreatedCsc(MessageLoopState*pMessageLoopState, int _threadCreated);
ClientSideCursorResult *resetAfterOneResultReadFromServerFinishCsc(MessageLoopState*pMessageLoopState, void *pCscExecutor, int index, 
                                                                    ClientSideCursorResult *pNextCscResult);

struct _CscStatementContext *createCscStatementContext(void *pCallerContext, CSC_RESULT_HANDLER_CALLBACK *pResultHandlerCallbackFunc,
                                                        PGconn *conn, int iUseMsgLoop);
struct _CscStatementContext *releaseCscStatementContext(struct _CscStatementContext *pCscStatementContext);
void resetCscStatementConext(void *_pCscStatementContext);
void setCalledFromCscThread(void *_pCscStatementContext,int flag);
void setCscThreadCreatedFlag(void *_pCscStatementContext, int flag);
int getCscThreadCreatedFlag(void *_pCscStatementContext);
void setCscInitMsgLoopStateFlag(void *_pCscStatementContext, int flag);
void setPgConnCsc(struct _CscStatementContext *pCscStatementContext, PGconn *conn);
PGconn *getPgConnCsc(void *_pCscStatementContext);
MessageLoopState *getPgMsgLoopCsc(void *_pCscStatementContext);
void pgSetSkipAllResultsCsc(void *_pCscStatementContext, int val);
void  checkForIOExceptionCsc(void *_pCscStatementContext);

void setEndOfStreamingCursor(void *_pCscStatementContext, int flag);
int isEndOfStreamingCursor(void *_pCscStatementContext);
void setStreamingCursorRows(void *_pCscStatementContext, int iStreamingCursorRows);
void resetStreamingCursorBatchNumber(void *_pCscStatementContext);
void setEndOfStreamingCursorQuery(void *_pCscStatementContext, int flag);
int isEndOfStreamingCursorQuery(void *_pCscStatementContext);
int isStreamingCursorMode(void *pCallerContext);
void setSkipResultOfStreamingCursor(void *_pCscStatementContext, int flag);
void resetStreamingCursorStatementConext(void *_pCscStatementContext);
int getStreamingCursorBatchNumber(void *_pCscStatementContext);
void resetAfterOneResultReadFromServerFinishForStreamingCursor(struct _CscStatementContext *pCscStatementContext, PGconn *conn);

#ifdef __cplusplus
}
#endif /* C++ */
