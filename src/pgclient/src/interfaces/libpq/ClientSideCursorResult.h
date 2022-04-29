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

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef WIN32
#include <io.h>
#include "csc_win_port.h"
#endif // WIN32

#ifdef LINUX
#include "csc_linux_port.h"
#endif

#include "rsfile.h"
#include "rslock.h"
#include "rsmem.h"

#include "ClientSideCursorOptions.h"
#include "ClientSideCursorOutputStream.h"
#include "ClientSideCursorInputStream.h"
#include "ClientSideCursorLock.h"
#include "ClientSideCursorTrace.h"

#include "libpq-fe.h"
#include "libpq-int.h"


// Constants

// file name prefix
#define FILE_NAME_PREFIX  "rsdbodbc"

// data file name extension
#define DATA_FILE_NAME_EXTENSION "cursor"

// offset file name extension
#define OFFSET_FILE_NAME_EXTENSION  "offset"

// Batch size to signal. 
#define BATCH_SIZE_TO_PROCESS  1000


/**
 * This class store results on disk and keep track of size of in memory cursor.
 * We will allocate it per result as part of statement execution, so we can maintain it for each result.
 * 
 * Following functions called from ODBC/libpq resultset object:
 *  - readNextBatchOfRows
 *  - readPreviousBatchOfRows
 *  - readLastBatchOfRows
 *  - readFirstBatchOfRows
 *  - readAbsoluteBatchOfRows
 *  - isFileCreated
 *  - isLastBatch
 *  - isFirstBatch
 *  - getFirstRowIndex
 *   
 * @author igarish
 *
 */
typedef struct _ClientSideCursorResult 
{
    // Is memory reach to threshold limit?
    int m_alreadyReachedThreshold;
    
    // last row length read from server
    long long m_rawRowLength;  
    
    // Total rows read from server
    long long m_rowCount;
    
    // Max rows defined for this query
    int m_maxRows;
    
    // Indicate max rows reached.
    int m_maxRowsReached;
    
    // File name to store the data
    char m_fileName[MAX_PATH + 1];

    // File name to store the offset
    char m_offsetFileName[MAX_PATH + 1];
    
    // csc options
    ClientSideCursorOptions *m_cscOptions;
    
    // Output data stream
    ClientSideCursorOutputStream *m_cscOutputStream;
    
    // total memory occupied in memory when read from server
    long  long m_inMemoryResultSize;
    
    // File created flag
    int m_fileCreated;
    
    // last row length read from file
    long long m_rawRowLengthFromFile;  

    // Input data stream
    ClientSideCursorInputStream *m_cscInputStream;
    
    // Indicate end of input stream reached.
    int m_endOfInputStream;

    // total memory occupied in memory when read from file
    long long    m_inMemoryResultSizeFromFile;
    
    // Resultset type
    int m_resultsettype;
    
    // File offset from where first read will happen.
    long long m_firstReadFromFileOffset;
    
    // offset output data stream for scrollable cursor
    ClientSideCursorOutputStream *m_cscOffsetOutputStream;
    
    // Scrollable cursor info.
    
    // 1 based record number. First row number in the memory. Row numbers runs from 1 to m_totalRows.    
    int m_firstRowNumberInMem; 
    
    // 1 based record number. Last row number in the memory. So total records in memory = m_lastRowNumberInMem - m_firstRowNumberInMem + 1.      
    int m_lastRowNumberInMem;
    
    // Total rows in the file.
    int m_totalRows;

    // Total commited rows in the file.
    int m_totalCommitedRows;
    
    // Input offset stream
    ClientSideCursorInputStream *m_cscOffsetInputStream;
    
    // Number of cols
    int m_cols;
    
    // Connection identifier
    int  m_connectionPid;
    
    // Lock to wait for reading more data from socket
    ClientSideCursorLock *m_cscLock;
    
    // Query executor who created the csc
//    private QueryExecutorImpl m_queryExecutor;
    
    // Execution index
    int m_executeIndex;

    // Skip result
    int m_skipResult;

    // IOException error code
    int m_ioe;
}ClientSideCursorResult;

// Function declarations

#ifdef __cplusplus
extern "C" 
{
#endif /* C++ */

int getConnectionPidCsc(ClientSideCursorResult *pCsc);

ClientSideCursorResult *createCscResult(ClientSideCursorOptions *pCscOptions, int maxRows, char *hostName, 
                                        int connectionPid, char *portalName, int resultsettype, int executeIndex);
ClientSideCursorResult *createCscResultWithFileName(ClientSideCursorOptions *pCscOptions, int maxRows, char *hostName, 
                                        int connectionPid, char *portalName, int resultsettype, int executeIndex, 
                                        char *fileName, int openForReading, int totalRows);

ClientSideCursorResult *releaseCsc(ClientSideCursorResult *pCsc);

void initCscLib(FILE    *fpTrace);
void uninitCscLib();

void setThresholdReachCsc(ClientSideCursorResult *pCsc, int thresholdReach);
void setRawRowLengthCsc(ClientSideCursorResult *pCsc, long long rawRowLength);
int doesThresholdReachCsc(ClientSideCursorResult *pCsc);
long long  getRawRowLengthCsc(ClientSideCursorResult *pCsc);
int checkForThresholdLimitReachCsc(ClientSideCursorResult *pCsc, int cols, long long rawRowLength, int creatingCsc);
void incRowCountAndCheckForMaxRowsCsc(ClientSideCursorResult *pCsc);
int doesMaxRowsReachCsc(ClientSideCursorResult *pCsc);
long long getRowCountCsc(ClientSideCursorResult *pCsc);
int createFileCsc(ClientSideCursorResult *pCsc, PGresult * pgResult);
int writeRowCsc(ClientSideCursorResult *pCsc, PGresAttValue *tuple, int noOfCols);
int closeOutputFileCsc(ClientSideCursorResult *pCsc);
int closeOffsetOutputFileCsc(ClientSideCursorResult *pCsc);
int deleteDataFileCsc(ClientSideCursorResult *pCsc);
int deleteOffsetFileCsc(ClientSideCursorResult *pCsc);
int closeCsc(ClientSideCursorResult *pCsc, int iCalledFromShutdownHook);
PGresAttValue **readNextBatchOfRowsCsc(ClientSideCursorResult *pCsc, PGresult * res, int *pntups);
PGresAttValue *readRowCsc(ClientSideCursorResult *pCsc, int *piError, int *piEof);
int closeInputFileCsc(ClientSideCursorResult *pCsc);
int closeOffsetInputFileCsc(ClientSideCursorResult *pCsc);
PGresAttValue **readPreviousBatchOfRowsCsc(ClientSideCursorResult *pCsc, PGresult * res, int *pntups); 
PGresAttValue **readNextBatchOfRowsFromOffsetCsc(ClientSideCursorResult *pCsc, long long startRowOffset,int startRowNumber, int rows, 
                                                 PGresult * res, int *pntups);
long long readDataFileOffsetCsc(ClientSideCursorResult *pCsc, int rowNumber);
int getConnectionPidCsc(ClientSideCursorResult *pCsc);
void addShutdownHookForFileCleanupCsc(ClientSideCursorResult *pCsc);
int getFirstRowIndexCsc(ClientSideCursorResult *pCsc);
PGresAttValue **readLastBatchOfRowsCsc(ClientSideCursorResult *pCsc, PGresult * res, int *pntups);
PGresAttValue **readPreviousBatchOfRowsFromRowNumberCsc(ClientSideCursorResult *pCsc, int rowNumber, PGresult * res, int *pntups);
int checkAndOpenInputStreamCsc(ClientSideCursorResult *pCsc, int seekForNext);
int isLastBatchCsc(ClientSideCursorResult *pCsc);
PGresAttValue **readNextBatchOfRowsUntilThresholdCsc(ClientSideCursorResult *pCsc, PGresult * res, int waitForWriteCommit, int *pntups);
PGresAttValue **readFirstBatchOfRowsCsc(ClientSideCursorResult *pCsc, PGresult * res, int *pntups);
int isFirstBatchCsc(ClientSideCursorResult *pCsc);
PGresAttValue **readAbsoluteBatchOfRowsCsc(ClientSideCursorResult *pCsc, PGresult * res, int *pntups, int *indexBuf);
int isFileCreatedCsc(ClientSideCursorResult *pCsc);
void createLockCsc(ClientSideCursorResult *pCsc);
void resetLockCsc(ClientSideCursorResult *pCsc);
void waitForFullResultReadFromServerCsc(ClientSideCursorResult *pCsc); 
void signalForFullResultReadFromServerCsc(ClientSideCursorResult *pCsc);
void waitForBatchResultReadFromServerCsc(ClientSideCursorResult *pCsc);
void signalForBatchResultReadFromServerCsc(ClientSideCursorResult *pCsc);
void setIOExceptionCsc(ClientSideCursorResult *pCsc, int ioe);
int getIOExceptionCsc(ClientSideCursorResult *pCsc);
int isFullResultReadFromServerCsc(ClientSideCursorResult *pCsc);
int getExecuteIndexCsc(ClientSideCursorResult *pCsc);
int isSkipResultCsc(ClientSideCursorResult *pCsc);

void setIOErrorCsc(int *pError, int value);
int getIOErrorCsc(int *pError);
void resetIOErrorCsc(int *pError);

int pgCloseCsc(PGresult * pgResult);
int pgGetExecuteNumberCsc(PGresult * pgResult);
int pgGetFirstRowIndexCsc(PGresult * res);
int pgIsFileCreatedCsc(PGresult * pgResult);
int pgIsFirstBatchCsc(PGresult * pgResult);
int pgIsLastBatchCsc(PGresult * pgResult);
int pgReadAbsoluteBatchOfRowsCsc(PGresult * res, int *piError, int *indexBuf);
int pgReadFirstBatchOfRowsCsc(PGresult * res, int *piError);
int pgReadLastBatchOfRowsCsc(PGresult * res, int *piError);
int pgReadNextBatchOfRowsCsc(PGresult * res,int *piError);
int pgReadPreviousBatchOfRowsCsc(PGresult * res, int *piError);

void uninitCscLib(void);

#ifdef __cplusplus
}
#endif /* C++ */
