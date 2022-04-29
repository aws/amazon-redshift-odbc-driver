/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "ClientSideCursorResult.h"
#include "ClientSideCursorShutdownHook.h"

// Monitor for synchronize methods
static MUTEX_HANDLE g_CscMethodLock = NULL;

void _pgFreeTuplePointers(PGresult * res);

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Initialize the csc lib
//
void initCscLib(FILE    *fpTrace)
{
    g_CscMethodLock = rsCreateMutex();

    setTraceInfoCsc(fpTrace);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Uninitialize the csc lib
//
void uninitCscLib(void)
{
    setTraceInfoCsc(NULL);

    rsDestroyMutex(g_CscMethodLock);
    g_CscMethodLock = NULL;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Initialize the csc result with defaults
//
ClientSideCursorResult *createCscResult(ClientSideCursorOptions *pCscOptions, int maxRows, char *hostName, 
                                        int connectionPid, char *portalName, int resultsettype, int executeIndex)
{
    return createCscResultWithFileName(pCscOptions, maxRows, hostName, connectionPid, portalName, resultsettype, executeIndex, 
                                        NULL, FALSE, 0);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Initialize the csc result.
// fileName, openForReading, totalRows is for unit testing.
//
ClientSideCursorResult *createCscResultWithFileName(ClientSideCursorOptions *pCscOptions, int maxRows, char *hostName, 
                                int connectionPid, char *portalName, int resultsettype, int executeIndex, 
                                char *fileName, int openForReading, int totalRows)
{
    ClientSideCursorResult *pCsc = rs_calloc(1,sizeof(ClientSideCursorResult));
    char tempFileName[MAX_PATH + 1];

    if(pCsc)
    {
        if(fileName == NULL || strlen(fileName) == 0)
        {
            fileName = tempFileName;
            snprintf(fileName, sizeof(tempFileName), "%s%s%s%s%s%d%s%s%s%lld", getPathCscOption(pCscOptions), FILE_NAME_PREFIX, "_" , hostName , "_" , connectionPid,
                                                    ((portalName != NULL) ? ("_")  : "") , ((portalName != NULL) ? portalName : ""), 
                                                    "_", getCurrentTimeInMilli());
            
            openForReading = FALSE;
            totalRows = 0;
        }
        
        pCsc->m_connectionPid = connectionPid;
        pCsc->m_alreadyReachedThreshold = FALSE;
        pCsc->m_rawRowLength     = 0;
        pCsc->m_rowCount         = 0;
        pCsc->m_maxRows         = maxRows;
        pCsc->m_maxRowsReached = FALSE;
        pCsc->m_cscOptions     = pCscOptions;
        snprintf(pCsc->m_fileName, sizeof(pCsc->m_fileName), "%s%s%s", fileName, "." ,  DATA_FILE_NAME_EXTENSION);
        snprintf(pCsc->m_offsetFileName, sizeof(pCsc->m_offsetFileName), "%s%s%s", fileName, ".", OFFSET_FILE_NAME_EXTENSION);
        
        pCsc->m_cscOutputStream            = NULL;
        pCsc->m_firstReadFromFileOffset     = 0;
        pCsc->m_cscOffsetOutputStream        = NULL;
        
        pCsc->m_inMemoryResultSize = 0;
        pCsc->m_fileCreated = openForReading;
        pCsc->m_cscInputStream = NULL;
        pCsc->m_endOfInputStream = FALSE;
        pCsc->m_rawRowLengthFromFile=0;
        
        pCsc->m_resultsettype = resultsettype;
        
        pCsc->m_firstRowNumberInMem=0;
        pCsc->m_lastRowNumberInMem=0;
        pCsc->m_totalRows=(openForReading) ? totalRows : 0;
        
        pCsc->m_inMemoryResultSizeFromFile = 0;
        
        pCsc->m_cscOffsetInputStream = NULL;
        pCsc->m_cols = 0;
        
        pCsc->m_cscLock = NULL;
        pCsc->m_ioe = 0;
        pCsc->m_totalCommitedRows=pCsc->m_totalRows;
        
        pCsc->m_executeIndex  = executeIndex;
        pCsc->m_skipResult = FALSE;
    }

    return pCsc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release CSC
//
ClientSideCursorResult *releaseCsc(ClientSideCursorResult *pCsc)
{
    if(pCsc)
    {
        pCsc->m_cscLock = destroyCscLock(pCsc->m_cscLock);
        pCsc = rs_free(pCsc);
    }

    return pCsc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set whether threshold reach or not.
//
void setThresholdReachCsc(ClientSideCursorResult *pCsc, int thresholdReach)
{
    pCsc->m_alreadyReachedThreshold = thresholdReach;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set raw length of a row.
//
void setRawRowLengthCsc(ClientSideCursorResult *pCsc, long long rawRowLength)
{
    pCsc->m_rawRowLength = rawRowLength;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Check status of threshold.
//
int doesThresholdReachCsc(ClientSideCursorResult *pCsc)
{
    return pCsc->m_alreadyReachedThreshold;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get the raw length of a row.
//
long long  getRawRowLengthCsc(ClientSideCursorResult *pCsc)
{
    return pCsc->m_rawRowLength;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Check for threshold reach or not.
//
int checkForThresholdLimitReachCsc(ClientSideCursorResult *pCsc, int cols, long long rawRowLength, int creatingCsc)
{
    int reachThresholdLimit;
    long long thresholdCSCLimit = getThresholdCscOption(pCsc->m_cscOptions);

    if(thresholdCSCLimit > 0)
    {
        long long inMemoryResultSize = (creatingCsc) ? pCsc->m_inMemoryResultSize : pCsc->m_inMemoryResultSizeFromFile;
        long long    rowSize = 0;
        
        if(inMemoryResultSize == 0)
        {
            // First row then add size of tupes vector class
            
            // 40 calculation:
            //        8 (for the Vector class) +
            //        4 (for the pointer to elemendata) +
            //        4 (for element count) +
            //          16 (for the elemendata class & length) +
            //        8  (for modCount)
            //
            inMemoryResultSize += 40;
        }
        
        // 4 
        inMemoryResultSize +=  4; // Accumulate for each row for tuples[] vector pointing to row
        
        
        // 12 : 8 for byte[] (first dimension) class + 4 for the pointer to elemendata
        // 4 * no_of_cols: for byte[] pointing to column data array
        // rawRowLength is total column data length in the row.
        // 12 : 8 for byte[] (second dimension) class + 4 for the pointer to elemendata
        // 4 * no_of_cols: for byte[] pointing to actual column data
        rowSize = (12 + (4 * cols) + rawRowLength + (12 + (4 * cols)));
        
        inMemoryResultSize += rowSize;
        
        reachThresholdLimit = (inMemoryResultSize >= thresholdCSCLimit);
        
        if(creatingCsc)
        {
            pCsc->m_inMemoryResultSize = inMemoryResultSize;
        }
        else
        {
            pCsc->m_inMemoryResultSizeFromFile = inMemoryResultSize;
        }
    }
    else
    {
        reachThresholdLimit = FALSE;
    }

    if(reachThresholdLimit)
    {
        if(IS_TRACE_ON_CSC())
        {
            traceInfoCsc("checkForThresholdLimitReach: Threshold limit reached. Limit defined = %lld", thresholdCSCLimit); 
        }
    }
    
    return reachThresholdLimit;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Check for max rows limit reached or not.
//
void incRowCountAndCheckForMaxRowsCsc(ClientSideCursorResult *pCsc)
{
    (pCsc->m_rowCount)++;
    
    if((pCsc->m_maxRows != 0) && (pCsc->m_rowCount == pCsc->m_maxRows))
    {
        if(IS_TRACE_ON_CSC())
        {
            traceInfoCsc("incRowCountAndCheckForMaxRows: Max rows is reached. max rows = %d", pCsc->m_maxRows); 
        }

        pCsc->m_maxRowsReached = TRUE;
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Check status of max rows limit.
//
int doesMaxRowsReachCsc(ClientSideCursorResult *pCsc)
{
    return pCsc->m_maxRowsReached;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get row count
//
long long getRowCountCsc(ClientSideCursorResult *pCsc)
{
    return pCsc->m_rowCount;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Create the file for csc. 0 means successful.
//
int createFileCsc(ClientSideCursorResult *pCsc, PGresult *pgResult) 
{
    PGresAttValue **tuples = pqGetTuples(pgResult);

    if(IS_TRACE_ON_CSC())
    {
        traceInfoCsc("createFile: Start creating data file: %s records count in mem:%d", pCsc->m_fileName, ((pgResult != NULL) ? PQntuples(pgResult) : 0)); 
    }

    pCsc->m_cscOutputStream = createCscOutputStream(pCsc->m_fileName, pCsc->m_resultsettype, &(pCsc->m_ioe));

    if(pCsc->m_ioe)
        return pCsc->m_ioe;
    
    pCsc->m_fileCreated = TRUE;

    // Add the hook, if it's not added already
    addShutdownHookForFileCleanupCsc(pCsc);
    
    // Add this object as unclosed object in the list
    if(getCscShutdownHook() != NULL)
    {
        addCscForCleanupOnExit(pCsc);
    }
    
    pCsc->m_firstRowNumberInMem = 1;
    
    // We are not storing first batch in file, if it's forward only cursor.
    if(!doesItForwardOnlyCursor(pCsc->m_resultsettype))
    {
        int noOfRows;
        int row;
        int noOfCols;

        if(IS_TRACE_ON_CSC())
        {
            traceInfoCsc("createFile: Start pushing first batch of data in file for scrollable cursor..."); 
        }

        pCsc->m_cscOffsetOutputStream = createCscOutputStream(pCsc->m_offsetFileName, pCsc->m_resultsettype, &(pCsc->m_ioe));
        if(pCsc->m_ioe)
            return pCsc->m_ioe;
        
        noOfRows = (pgResult != NULL) ? PQntuples(pgResult) : 0;
        noOfCols = PQnfields(pgResult);
        
        for(row = 0; row < noOfRows;row++)
        {
            PGresAttValue *tuple = tuples[row];
            
            pCsc->m_ioe = writeRowCsc(pCsc,tuple, noOfCols);
            if(pCsc->m_ioe)
                return pCsc->m_ioe;
            
            // Store number of cols for row size estimate.
            if(pCsc->m_cols == 0 && pgResult != NULL)
            {
                pCsc->m_cols = noOfCols;
            }
        } // row loop
        
        // Remember position in file, from where next read will happen from file, because we already have first batch in memory.
        pCsc->m_firstReadFromFileOffset =  getPositionCscOutputStream(pCsc->m_cscOutputStream);
        
        pCsc->m_lastRowNumberInMem = pCsc->m_totalRows;
    }

    return pCsc->m_ioe;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Write one row on the disk. o means successful.
// Format is:
//    2 bytes: column count
//    for each column:
//         4 bytes: column value length. 0 means empty string, -1 means NULL, otherwise actual data length in bytes.
//         N bytes: column value in bytes.
int writeRowCsc(ClientSideCursorResult *pCsc, PGresAttValue *tuple, int noOfCols) 
{
    if(pCsc->m_cscOutputStream != NULL && tuple != NULL)
    {
        long long maxFileSize = getMaxFileSizeCscOption(pCsc->m_cscOptions);
        int col;
        
        if((maxFileSize == 0) || (sizeCscOutputStream(pCsc->m_cscOutputStream) < maxFileSize))
        {
            // Write offset first
            if(!doesItForwardOnlyCursor(pCsc->m_resultsettype))
            {
                if(pCsc->m_cscOffsetOutputStream != NULL)
                {
                    // 8 bytes as offset in data file.
                    pCsc->m_ioe = writeLongLongCscOutputStream(pCsc->m_cscOffsetOutputStream, getPositionCscOutputStream(pCsc->m_cscOutputStream));

                    // Flush the offset file for parallel reading.
                    getPositionCscOutputStream(pCsc->m_cscOffsetOutputStream);
                }
            }
            
            // 2 bytes number of cols
            pCsc->m_ioe = writeShortCscOutputStream(pCsc->m_cscOutputStream, noOfCols);
            
            for(col = 0; col < noOfCols; col++)
            {
                int colValLen = (tuple[col].value != NULL) ? tuple[col].len : -1;
                
                // 4 bytes val len
                pCsc->m_ioe = writeIntCscOutputStream(pCsc->m_cscOutputStream,colValLen);
                
                if(colValLen > 0)
                {
                    // colValLen data val
                    pCsc->m_ioe = writeCscOutputStream(pCsc->m_cscOutputStream,tuple[col].value, 0, colValLen);
                }
            } // col loop
            
            (pCsc->m_totalRows)++;
            
            if((pCsc->m_totalRows  % BATCH_SIZE_TO_PROCESS) == 0)
            {
                pCsc->m_totalCommitedRows = pCsc->m_totalRows;
                
                // Flush the output
                pCsc->m_ioe = flushCscOutputStream(pCsc->m_cscOutputStream);
                
                // Signal batch has been written
                signalForBatchResultReadFromServerCsc(pCsc);
            } 

/*            if(!doesItForwardOnlyCursor(pCsc->m_resultsettype))
            {
                // To simulate write error for testing
                if(pCsc->m_totalRows % 30000 == 0)
                {
                    printf("writeRow(): Press any key to continue...\n");
                    getchar();
                    pCsc->m_ioe = TRUE;
                } 
            } */
        }
        else
        {
            // File max size reached.
            if(IS_TRACE_ON_CSC())
            {
                traceInfoCsc("writeRow: File max size reached. file max size = %lld", maxFileSize); 
            }

           pCsc->m_ioe = TRUE;
           // Drain current result
           pCsc->m_skipResult = TRUE;
        }
    } 

    return pCsc->m_ioe;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Close the csc file.
//
int closeOutputFileCsc(ClientSideCursorResult *pCsc) 
{
    int rc;

    if(pCsc->m_cscOutputStream != NULL)
    {
        int rc1;

        if(IS_TRACE_ON_CSC())
        {
            traceInfoCsc("closeOutputFile: End pushing data in file...%s", pCsc->m_fileName); 
        }

        pCsc->m_totalCommitedRows = pCsc->m_totalRows;

        rc = closeCscOutputStream(pCsc->m_cscOutputStream);
        
        pCsc->m_cscOutputStream = releaseCscOutputStream(pCsc->m_cscOutputStream);
        
        rc1 = closeOffsetOutputFileCsc(pCsc);
        if(!rc)
            rc = rc1;
    }
    else
        rc = 0;

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Close the csc offset file. 0 means successful.
//
int closeOffsetOutputFileCsc(ClientSideCursorResult *pCsc) 
{
    int rc;

    if(pCsc->m_cscOffsetOutputStream != NULL)
    {
        rc = closeCscOutputStream(pCsc->m_cscOffsetOutputStream);
        
        pCsc->m_cscOffsetOutputStream = releaseCscOutputStream(pCsc->m_cscOffsetOutputStream);
    }
    else
        rc = 0;

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Delete data and offset file(s). 0 means successful.
//
int deleteDataFileCsc(ClientSideCursorResult *pCsc) 
{
    int rc = 0;
    
    // To simulate delete error for testing
/*    
    {
        printf("deleteDataFile(): Press any key to continue...\n");
        getchar();
        pCsc->m_ioe = TRUE;
    }  */
    
    // Delete the file
    if(fileExists(pCsc->m_fileName))
    {
/*            // Testing with USB drive for delete error.
        printf("deleteDataFile(): Remove USB drive and press any key to continue...\n");
        getchar(); 
*/
        
        // Attempt to delete it
        int success =  (remove(pCsc->m_fileName) == 0);

        if(!success)
        {
            // Error
            if(IS_TRACE_ON_CSC())
            {
                traceInfoCsc("deleteDataFile: Error during data file delete. File name = %s", pCsc->m_fileName); 
            }

            rc = 1; 

            return rc;
        }
    }
    
    rc = deleteOffsetFileCsc(pCsc);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Delete offset file. 0 means successful.
//
int deleteOffsetFileCsc(ClientSideCursorResult *pCsc) 
{
    int rc = 0;

    if(!doesItForwardOnlyCursor(pCsc->m_resultsettype))
    {
        // Delete the file
        if(fileExists(pCsc->m_offsetFileName))
        {
            // Attempt to delete it
            int success = (remove(pCsc->m_offsetFileName) == 0);
    
            if(!success)
            {
                // Error
                if(IS_TRACE_ON_CSC())
                {
                    traceInfoCsc("deleteOffsetFile: Error during offset file delete. File name = %s", pCsc->m_offsetFileName); 
                }

                rc = 1; 

                return rc;
            }
        }
    }

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Close the csc in the pgResult. 0 means successful.
//
int pgCloseCsc(PGresult * pgResult) 
{
    int rc;

    if((pgResult) && (pgResult->m_cscResult != NULL))
    {
        rc = closeCsc(pgResult->m_cscResult, FALSE);
        pgResult->m_cscResult = releaseCsc(pgResult->m_cscResult);
    }
    else
        rc = 0;

    return rc;
}


//---------------------------------------------------------------------------------------------------------igarish
// Close the csc. i.e. delete the file associated with it. 0 means successful.
//
int closeCsc(ClientSideCursorResult *pCsc,int iCalledFromShutdownHook) 
{
    int rc = 0;

    if(pCsc->m_fileCreated)
    {
        int savIoe = pCsc->m_ioe;

        if(IS_TRACE_ON_CSC())
        {
            traceInfoCsc("close: Closing client side cursor"); 
        }

        if(!isFullResultReadFromServerCsc(pCsc))
        {
            // Drain current result
            pCsc->m_skipResult = TRUE;
            
            // If thread is running when close() called, wait until we read/skip all data from socket.
            waitForFullResultReadFromServerCsc(pCsc);
        }
        
        pCsc->m_fileCreated = FALSE;
        
        // Close output file
        pCsc->m_ioe = closeOutputFileCsc(pCsc);
        
        // Close input file
        pCsc->m_ioe = closeInputFileCsc(pCsc);   
        
        // Close offset input file
        pCsc->m_ioe = closeOffsetInputFileCsc(pCsc);
        
        // Delete data and offset file(s)
        pCsc->m_ioe = deleteDataFileCsc(pCsc);
        
        // Reset the lock var.
        pCsc->m_cscLock = destroyCscLock(pCsc->m_cscLock);
        
        // Remove CSC object from unclosed list.
        if(getCscShutdownHook() != NULL)
        {
            removeCscFromCleanupOnExit(pCsc, iCalledFromShutdownHook);
        }

        rc = pCsc->m_ioe;

        pCsc->m_ioe = (pCsc->m_ioe || savIoe);
    }
    
    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read next set of rows from the file in memory. 1 means read rows from file. 
//
int pgReadNextBatchOfRowsCsc(PGresult * res,int *piError) 
{
    int rc = FALSE;

    if(res)
    {
        PGresAttValue **tuples = NULL;
        int ntups = 0;

        tuples = readNextBatchOfRowsCsc(res->m_cscResult, res, &ntups);

        if(piError)
        {
            if(res->m_cscResult)
            {
                *piError = res->m_cscResult->m_ioe;
                res->m_cscResult->m_ioe = 0;
            }
            else
                *piError = 0;
        }

        if(tuples != NULL && ntups > 0)
        {
            // Put rows in pgResult
            res->ntups = ntups;
            res->tuples = tuples;
            res->m_tuplesAllocatedByCscRead = TRUE;
            rc = TRUE;
        }
    }

    return rc;
}


//---------------------------------------------------------------------------------------------------------igarish
// Read next set of rows from the file in memory.
//
PGresAttValue **readNextBatchOfRowsCsc(ClientSideCursorResult *pCsc, PGresult * res, int *pntups) 
{
    PGresAttValue **tuples = NULL;

    if(pCsc->m_fileCreated)
    {
        if(!(pCsc->m_endOfInputStream))
        {
            pCsc->m_ioe = checkAndOpenInputStreamCsc(pCsc, TRUE);

            if(pCsc->m_ioe == 0)
            {
                if(pCsc->m_cscInputStream != NULL)
                {
                    int lastRowNumberInMem = pCsc->m_lastRowNumberInMem;
                    
                    // Set the first row number in next batch
                    if((pCsc->m_lastRowNumberInMem < pCsc->m_totalRows)) // !doesItForwardOnlyCursor()
                    {
                        pCsc->m_firstRowNumberInMem = pCsc->m_lastRowNumberInMem + 1;
                        lastRowNumberInMem = 0;
                    } 
                    
                    // Read rows until threshold or end reach
                    tuples = readNextBatchOfRowsUntilThresholdCsc(pCsc,res, TRUE, pntups); 
                    
                    // Set the first row number in next batch
                    if(lastRowNumberInMem != 0 && tuples != NULL && pntups != NULL && *pntups != 0)
                    {
                        pCsc->m_firstRowNumberInMem = lastRowNumberInMem + 1;
                    }
                }
                else
                {
                    pCsc->m_endOfInputStream = TRUE;

                    if(IS_TRACE_ON_CSC())
                    {
                        traceInfoCsc("readNextBatchOfRows: input stream is null."); 
                    }
                }
            }
            else
            {
                if(IS_TRACE_ON_CSC())
                {
                    traceInfoCsc("readNextBatchOfRows: File not found. File name = %s", pCsc->m_fileName); 
                }
            }
        }
        else
        {
            if(IS_TRACE_ON_CSC())
            {
                traceInfoCsc("readNextBatchOfRows: end of the input stream already reached."); 
            }
        }
    }
    else
    {
        if(IS_TRACE_ON_CSC())
        {
            traceInfoCsc("readNextBatchOfRows: File is not created."); 
        }
    }
    
    return tuples;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read a single row from the file.
//
PGresAttValue *readRowCsc(ClientSideCursorResult *pCsc, int *piError,int *piEof) 
{
    PGresAttValue *tuple = NULL;
    
    pCsc->m_rawRowLengthFromFile = 0;
    *piError = 0;
    *piEof = 0;
    
    if(pCsc->m_cscInputStream != NULL)
    {
        *piEof = feofCscInputStream(pCsc->m_cscInputStream);

        if(*piEof == 0)
        {
            int noOfCols;
            int col;

            // 2 bytes number of cols
            noOfCols = readShortCscInputStreamForNumberOfCols(pCsc->m_cscInputStream);
            
            if(noOfCols > 0)
            {
                tuple = (PGresAttValue *)rs_calloc(sizeof(PGresAttValue),noOfCols);
                for(col = 0; col < noOfCols; col++)
                {
                    int colValLen;
                    
                    // 4 bytes val len
                    colValLen = readIntCscInputStream(pCsc->m_cscInputStream);

                    tuple[col].len  = colValLen;
                    
                    if(colValLen != -1)
                    {
                        int actualRead;

                        tuple[col].value = rs_calloc(sizeof(char),colValLen + 1);
                        
                        pCsc->m_rawRowLengthFromFile += colValLen;

						if(colValLen > 0)
						{
							// colValLen data val
							actualRead = readCscInputStream(pCsc->m_cscInputStream,tuple[col].value, 0, colValLen);
	                        
							if(actualRead != colValLen)
							{
								if(IS_TRACE_ON_CSC())
								{
									traceInfoCsc("readRow: File read error. File name = %s. Trying to read: %d bytes. But actual read: %d bytes.",
													pCsc->m_fileName, colValLen, actualRead); 
								}

								*piError = 1;

								// Free row memory

								return NULL;
							}
						} // Check for empty string length
                    }
                } // col loop
            }
            else
                *piEof = feofCscInputStream(pCsc->m_cscInputStream);

            // Check for any read error
            if(*piError == 0)
            {
                *piError = getIOErrorCsc(&(pCsc->m_cscInputStream->m_error));
            }

            if(*piError == 0 && *piEof == 0)
                (pCsc->m_lastRowNumberInMem)++;
            
    /*        if(!doesItForwardOnlyCursor(pCsc->m_resultsettype))
            {
                // To simulate read error for testing
                if(pCsc->m_lastRowNumberInMem % 30000 == 0)
                {
                    printf("readRow(): Press any key to continue...\n");
                    getchar();
                    *piError = 1;
                }
            } */
        } // EOF
    }
    else
    {
        if(IS_TRACE_ON_CSC())
        {
            traceInfoCsc("readRow: input stream is null.");
        }
    }
    
    return tuple;
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Close the csc file which open for read. 0 means successful.
//
int closeInputFileCsc(ClientSideCursorResult *pCsc) 
{
    int rc;

    if(pCsc->m_cscInputStream != NULL)
    {
        rc = closeCscInputStream(pCsc->m_cscInputStream);
        
        pCsc->m_cscInputStream = releaseCscInputStream(pCsc->m_cscInputStream);
    }
    else
        rc = 0;

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Close the offset file which open for read. 0 means successful.
//
int closeOffsetInputFileCsc(ClientSideCursorResult *pCsc) 
{
    int rc;

    if(pCsc->m_cscOffsetInputStream != NULL)
    {
        rc = closeCscInputStream(pCsc->m_cscOffsetInputStream);
        
        pCsc->m_cscOffsetInputStream = releaseCscInputStream(pCsc->m_cscOffsetInputStream);
    }
    else
        rc = 0;

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read previous set of rows from the file in memory.
//
int pgReadPreviousBatchOfRowsCsc(PGresult * res, int *piError) 
{
    int rc = FALSE;

    if(res)
    {
        PGresAttValue **tuples = NULL;
        int ntups = 0;

        tuples = readPreviousBatchOfRowsCsc(res->m_cscResult, res, &ntups);

        if(tuples != NULL && ntups > 0)
        {
            // Put rows in pgResult
            res->ntups = ntups;
            res->tuples = tuples;
            res->m_tuplesAllocatedByCscRead = TRUE;
            rc = TRUE;
        }

        if(piError)
        {
            if(res->m_cscResult)
            {
                *piError = res->m_cscResult->m_ioe;
                res->m_cscResult->m_ioe = 0;
            }
            else
                *piError = 0;
        }
    }

    return rc;
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read previous set of rows from the file in memory.
//
PGresAttValue **readPreviousBatchOfRowsCsc(ClientSideCursorResult *pCsc, PGresult * res, int *pntups) 
{
    PGresAttValue **tuples = NULL;
    
    if(pCsc->m_fileCreated)
    {
        if(!doesItForwardOnlyCursor(pCsc->m_resultsettype))
        {
            if(pCsc->m_firstRowNumberInMem > 1)
            {
                int rowNumber = pCsc->m_firstRowNumberInMem - 1;
                
                tuples = readPreviousBatchOfRowsFromRowNumberCsc(pCsc, rowNumber, res, pntups);
            }
            else
            {
                if(IS_TRACE_ON_CSC())
                {
                    traceInfoCsc("readPreviousBatchOfRows: Cursor is on first batch.");
                }
            }
        }
        else
        {
            if(IS_TRACE_ON_CSC())
            {
                traceInfoCsc("readPreviousBatchOfRows: Cursor is not scrollable.");
            }
        }
    }
    else
    {
        if(IS_TRACE_ON_CSC())
        {
            traceInfoCsc("readPreviousBatchOfRows: input file is not created.");
        }
    }
    
    return tuples;
} 

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read set of rows from the file in memory using start row offset, start row number and row count.
//
PGresAttValue **readNextBatchOfRowsFromOffsetCsc(ClientSideCursorResult *pCsc, long long startRowOffset,int startRowNumber, int rows, 
                                                 PGresult * res, int *pntups) 
{
    PGresAttValue **tuples = NULL;
    
    if(!doesItForwardOnlyCursor(pCsc->m_resultsettype))
    {
        if(pCsc->m_fileCreated)
        {
            if(!(pCsc->m_endOfInputStream))
            {
                if(pCsc->m_cscInputStream != NULL)
                {
                    int row = 0;
                    PGresAttValue *tuple;
            
                    // Jump to the start row offset
                    pCsc->m_ioe = seekCscInputStream(pCsc->m_cscInputStream,startRowOffset);
                    
                    tuples = rs_calloc(rows, sizeof(PGresAttValue *));

                    pCsc->m_firstRowNumberInMem = startRowNumber;
                    pCsc->m_lastRowNumberInMem  =  pCsc->m_firstRowNumberInMem - 1;
                    
                    // Read rows from file
                    while(row < rows)
                    {
                        if(pCsc->m_lastRowNumberInMem >= pCsc->m_totalRows)
                        {
                            // Reach last row.
                            break;
                        } 
                        
                        // is there any write error?
                        if(!(pCsc->m_ioe))
                        {
                            // Read a row from file
                            tuple = readRowCsc(pCsc,&(pCsc->m_ioe),&(pCsc->m_endOfInputStream));
                        }
                        else
                            tuple = NULL;

                        // If EOF or IO error break the loop
                        if(pCsc->m_endOfInputStream || pCsc->m_ioe || tuple == NULL)
                        {
                            if(pCsc->m_endOfInputStream)
                            {
                                // EOF file reached
                                if(IS_TRACE_ON_CSC())
                                {
                                    traceInfoCsc("readNextBatchOfRowsFromOffset: End reading data from file: %s", pCsc->m_fileName);
                                }
                            }

                            break;
                        }
                        
                        // Add row in memory
                        tuples[row] = tuple;
                        row++;
                        *pntups=row;
                        
                        // Now we have at least one row in new batch, clear the old rows from mem.
                        if(res->tuples != NULL && res->ntups > 0)
                        {
                            // Truncate it to zero to release the memory
                            _pgFreeTuplePointers(res);
                        }
                    } // rows loop
                }
                else
                {
                    pCsc->m_endOfInputStream = TRUE;

                    if(IS_TRACE_ON_CSC())
                    {
                        traceInfoCsc("readPreviousBatchOfRows: input stream is null.");
                    }
                }
            }
            else
            {
                if(IS_TRACE_ON_CSC())
                {
                    traceInfoCsc("readPreviousBatchOfRows: end of input stream already reached.");
                }
            }
        } // File created?
        else
        {
            if(IS_TRACE_ON_CSC())
            {
                traceInfoCsc("readPreviousBatchOfRows: File is not created.");
            }
        }
    } // Scrollable
    else
    {
        if(IS_TRACE_ON_CSC())
        {
            traceInfoCsc("readPreviousBatchOfRows: Cursor is not scrollable.");
        }
    }
    
    return tuples;
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read data file offset using row number. Row number start from 1.
//
long long readDataFileOffsetCsc(ClientSideCursorResult *pCsc, int rowNumber)
{
    long long dataOffset = -1;
    
    if(!doesItForwardOnlyCursor(pCsc->m_resultsettype))
    {
        if(rowNumber > 0)
        {
            // Open the offset file, if it's not open
            if(pCsc->m_cscOffsetInputStream == NULL)
            {
                // create it on first call
                pCsc->m_cscOffsetInputStream =  createCscInputStream(pCsc->m_offsetFileName, pCsc->m_resultsettype, &(pCsc->m_ioe));

                if(pCsc->m_cscOffsetInputStream && pCsc->m_ioe)
                {
                    if(IS_TRACE_ON_CSC())
                    {
                        traceInfoCsc("readDataFileOffset: File not found. File name = %s", pCsc->m_offsetFileName); 
                    }

                    return dataOffset;
                }
            }
            
            if(pCsc->m_cscOffsetInputStream != NULL)
            {
                // Jump to rowNumber * 8 to get the data file offset of that row
                long long offset = (rowNumber - 1) * 8;
                
                pCsc->m_ioe = seekCscInputStream(pCsc->m_cscOffsetInputStream,offset);

                // Read the row offset
                dataOffset = readLongLongCscInputStream(pCsc->m_cscOffsetInputStream);
                pCsc->m_ioe = getIOErrorCsc(&(pCsc->m_cscOffsetInputStream->m_error));

                if(pCsc->m_ioe)
                {
                    dataOffset = -1;

                    if(IS_TRACE_ON_CSC())
                    {
                        traceInfoCsc("readDataFileOffset IO Error"); 
                    }
                }
            }
            else
            {
                if(IS_TRACE_ON_CSC())
                {
                    traceInfoCsc("readDataFileOffset: Offset input stream is null."); 
                }
            }
        }
        else
        {
            if(IS_TRACE_ON_CSC())
            {
                traceInfoCsc("readDataFileOffset: rowNumber is <= 0. rowNumber=%d", rowNumber); 
            }
        }
    }
    else
    {
        if(IS_TRACE_ON_CSC())
        {
            traceInfoCsc("readDataFileOffset: Cursor is not scrollable."); 
        }
    }
    
    return dataOffset;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get the connection pid.
//
int getConnectionPidCsc(ClientSideCursorResult *pCsc)
{
    return pCsc->m_connectionPid;
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Add shutdown hook for csc file cleaning.
//
void addShutdownHookForFileCleanupCsc(ClientSideCursorResult *pCsc)
{
    // Lock
    rsLockMutex(g_CscMethodLock);

    if(getCscShutdownHook() == NULL)
    {
        // Add the hook.
        createCscShutdownHook();
    }

    // Unlock
    rsUnlockMutex(g_CscMethodLock);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get the row index of zeroth row in the current batch.
//
int pgGetFirstRowIndexCsc(PGresult * res)
{
    int rc;

    if(res)
    {
        rc = getFirstRowIndexCsc(res->m_cscResult);
    }
    else
        rc = 0;

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get the row index of zeroth row in the current batch.
//
int getFirstRowIndexCsc(ClientSideCursorResult *pCsc)
{
    return (pCsc->m_firstRowNumberInMem > 0) ? pCsc->m_firstRowNumberInMem - 1 : 0;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read last batch of rows.
//
int pgReadLastBatchOfRowsCsc(PGresult * res, int *piError) 
{
    int rc = FALSE;

    if(res)
    {
        PGresAttValue **tuples = NULL;
        int ntups = 0;

        tuples = readLastBatchOfRowsCsc(res->m_cscResult, res, &ntups);

        if(tuples != NULL && ntups > 0)
        {
            // Put rows in pgResult
            res->ntups = ntups;
            res->tuples = tuples;
            res->m_tuplesAllocatedByCscRead = TRUE;
            rc = TRUE;
        }

        if(piError)
        {
            if(res->m_cscResult)
            {
                *piError = res->m_cscResult->m_ioe;
                res->m_cscResult->m_ioe = 0;
            }
            else
                *piError = 0;

        }
    }

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read last batch of rows.
//
PGresAttValue **readLastBatchOfRowsCsc(ClientSideCursorResult *pCsc, PGresult * res, int *pntups) 
{
    PGresAttValue **tuples = NULL;

    if(pCsc->m_fileCreated)
    {
        // Wait for full read
        waitForFullResultReadFromServerCsc(pCsc);
        
        if(!doesItForwardOnlyCursor(pCsc->m_resultsettype))
        {
            if(pCsc->m_lastRowNumberInMem < pCsc->m_totalRows)
            {
                int rowNumber = pCsc->m_totalRows;
                
                pCsc->m_ioe = checkAndOpenInputStreamCsc(pCsc, FALSE);

                if(pCsc->m_ioe == 0)
                {
                    tuples = readPreviousBatchOfRowsFromRowNumberCsc(pCsc, rowNumber, res,  pntups);
                }
                else
                {
                    if(IS_TRACE_ON_CSC())
                    {
                        traceInfoCsc("readLastBatchOfRows: File not found. File name = %s",pCsc->m_fileName); 
                    }
                }
            } // On last batch?
            else
            {
                if(IS_TRACE_ON_CSC())
                {
                    traceInfoCsc("readLastBatchOfRows: Cursor is already on last batch."); 
                }
            }
        } // Scrollable?
        else
        {
            if(IS_TRACE_ON_CSC())
            {
                traceInfoCsc("readLastBatchOfRows: Cursor is not scrollable."); 
            }
        }
    }
    else
    {
        if(IS_TRACE_ON_CSC())
        {
            traceInfoCsc("readLastBatchOfRows: File is not created."); 
        }
    }
    
    return tuples;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read the rows in backward direction from given row number.
//
PGresAttValue **readPreviousBatchOfRowsFromRowNumberCsc(ClientSideCursorResult *pCsc, int rowNumber, PGresult * res, int *pntups)
{
    PGresAttValue **tuples = NULL;
    
    if(rowNumber > 0)
    {
        long long startRowOffset;
        long long curRowOffset;
        long long nextRowOffset;
        
        int rows = 0;
        long long rawRowLength;
        int reachThresholdLimit = FALSE;
        
        // Read memory size using offset file, start from m_firstRowNumberInMem - 1 and moving backward.
        nextRowOffset = 0;
        curRowOffset = readDataFileOffsetCsc(pCsc, rowNumber);
        if(pCsc->m_ioe)
            return tuples;

        startRowOffset = curRowOffset; 
        
        pCsc->m_inMemoryResultSizeFromFile = 0;
        
        while(curRowOffset >= 0 && !reachThresholdLimit)
        {
            rows++;
            startRowOffset = curRowOffset; 
            
            if((rowNumber - 1) <= 1)
            {
                break;
            }
            
            nextRowOffset = curRowOffset; 
            curRowOffset = readDataFileOffsetCsc(pCsc,--rowNumber);
            
            if(curRowOffset != -1)
            {
                // Calculate size
                rawRowLength = nextRowOffset - curRowOffset;  
                
                // Compare size, to check fit into memory or not
                reachThresholdLimit = checkForThresholdLimitReachCsc(pCsc,pCsc->m_cols, rawRowLength, FALSE);
            }
            
            if(curRowOffset < 0 || reachThresholdLimit)
            {
                rowNumber++;
            }
        } // Rows loop
        
        if(pCsc->m_ioe)
            return tuples;

        // Read rows using start row offset, start row number and number of rows
        tuples = readNextBatchOfRowsFromOffsetCsc(pCsc, startRowOffset, rowNumber, rows, res, pntups);
    } // Total rows exist
    else
    {
        if(IS_TRACE_ON_CSC())
        {
            traceInfoCsc("readPreviousBatchOfRowsFromRowNumber: rowNumber is  <= 0. rowNumber = %d", rowNumber); 
        }
    }

    return tuples;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Open the input stream if not opened. 0 means successful.
//
int checkAndOpenInputStreamCsc(ClientSideCursorResult *pCsc, int seekForNext) 
{
    int rc = 0;

    if(pCsc->m_cscInputStream == NULL)
    {
        if(IS_TRACE_ON_CSC())
        {
            traceInfoCsc("checkAndOpenInputStream: Start reading data from file: %s", pCsc->m_fileName); 
        }

        // create it on first call
        pCsc->m_cscInputStream = createCscInputStream(pCsc->m_fileName, pCsc->m_resultsettype,&rc);
        
        if(!doesItForwardOnlyCursor(pCsc->m_resultsettype))
        {
            if((rc == 0) && (pCsc->m_firstReadFromFileOffset > 0))
            {
                if(seekForNext)
                {
                    // Skip first batch of rows
                    rc = seekCscInputStream(pCsc->m_cscInputStream,pCsc->m_firstReadFromFileOffset);
                }
                
                pCsc->m_firstReadFromFileOffset = 0;
            }
        }
        else
        {
            if(IS_TRACE_ON_CSC())
            {
                traceInfoCsc("checkAndOpenInputStream: Cursor is not scrollable."); 
            }
        }
    }

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Check whether we are at last batch in file.
//
int pgIsLastBatchCsc(PGresult * pgResult)
{
    int rc = TRUE;

    if(pgResult && pgResult->m_cscResult)
        rc = isLastBatchCsc(pgResult->m_cscResult);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Check whether we are at last batch in file.
//
int isLastBatchCsc(ClientSideCursorResult *pCsc)
{
    int rc;
    
    if(pCsc->m_fileCreated)
    {
        // Wait for full read
        waitForFullResultReadFromServerCsc(pCsc);

        if(pCsc->m_ioe)
            return FALSE;
        
        rc = ((pCsc->m_cscInputStream != NULL) && (pCsc->m_lastRowNumberInMem >= pCsc->m_totalRows));
    }
    else
    {
        // Everything was in memory
        rc = TRUE;
    }
    
    return rc;
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read next set of rows until threshold reach.
//
PGresAttValue **readNextBatchOfRowsUntilThresholdCsc(ClientSideCursorResult *pCsc, PGresult * res,  int waitForWriteCommit, int *pntups) 
{
    PGresAttValue **tuples = NULL;
    
    if(pCsc->m_fileCreated)
    {
        if(!(pCsc->m_endOfInputStream))
        {
            if(pCsc->m_cscInputStream != NULL)
            {
                int reachedThreshold = FALSE;
                long long rowCount = 0;
                PGresAttValue *tuple;
                
                tuples = rs_calloc(BATCH_SIZE_TO_PROCESS, sizeof(PGresAttValue *));
                
                // Reset in memory size to zero.
                pCsc->m_inMemoryResultSizeFromFile = 0;
                
                // Read rows from file
                while(!reachedThreshold)
                {
                    if(!doesItForwardOnlyCursor(pCsc->m_resultsettype) 
                            && (pCsc->m_lastRowNumberInMem >= pCsc->m_totalRows) 
                            && isFullResultReadFromServerCsc(pCsc))
                    {
                        // Reach last row.
                        break;
                    } 
                    
                    // Check if next batch is avail
                    if(waitForWriteCommit)
                    {
                        if((rowCount % BATCH_SIZE_TO_PROCESS) == 0)
                        {
                            if((pCsc->m_totalCommitedRows - pCsc->m_lastRowNumberInMem) < BATCH_SIZE_TO_PROCESS)
                            {
                                do
                                {
                                    // Wait for batch read
                                    waitForBatchResultReadFromServerCsc(pCsc);
                                    waitForWriteCommit = !isFullResultReadFromServerCsc(pCsc); 
                                }while(waitForWriteCommit && ((pCsc->m_totalCommitedRows - pCsc->m_lastRowNumberInMem) < BATCH_SIZE_TO_PROCESS));
                            }
                        }
                    }
                    
                    if(!doesItForwardOnlyCursor(pCsc->m_resultsettype) 
                            && (pCsc->m_lastRowNumberInMem >= pCsc->m_totalRows) 
                            && isFullResultReadFromServerCsc(pCsc))
                    {
                        // Reach last row.
                        break;
                    } 
                    
                    // is there any write error?
                    if(!(pCsc->m_ioe))
                    {
                        // Read a row from file
                        tuple = readRowCsc(pCsc,&(pCsc->m_ioe),&(pCsc->m_endOfInputStream));
                    }
                    else
                        tuple = NULL;

                    // If EOF or IO error break the loop
                    if(pCsc->m_endOfInputStream || pCsc->m_ioe)
                    {
                        if(pCsc->m_endOfInputStream)
                        {
                            // EOF file reached
                            if(IS_TRACE_ON_CSC())
                            {
                                traceInfoCsc("readNextBatchOfRowsUntilThreshold: End reading data from file: %s", pCsc->m_fileName);
                            }
                        }
                        else
                        {
                            // IO error
                            if(IS_TRACE_ON_CSC())
                            {
                                traceInfoCsc("readNextBatchOfRowsUntilThreshold IO error.");
                            }
                        }

                        break;
                    }
                    
                    // Add row in memory
                    if(((rowCount + 1) % BATCH_SIZE_TO_PROCESS) == 0)
                    {
                        tuples = rs_realloc(tuples, sizeof(PGresAttValue *) * (rowCount + BATCH_SIZE_TO_PROCESS));
                    }

                    tuples[rowCount] = tuple;
                    rowCount++;
                    *pntups = (int)rowCount;
                    
                    // Now we have at least one row in new batch, clear the old rows from mem.
                    if(res->tuples != NULL && res->ntups > 0)
                    {
                        // Truncate it to zero to release the memory
                        _pgFreeTuplePointers(res);
                    }
                    
                    // Check memory reached to threshold
                    reachedThreshold = checkForThresholdLimitReachCsc(pCsc, (tuple != NULL) ? tuple->len : 0, pCsc->m_rawRowLengthFromFile, FALSE);
                } // rows loop
            } // InputStream != null
            else
            {
                pCsc->m_endOfInputStream = TRUE;

                if(IS_TRACE_ON_CSC())
                {
                    traceInfoCsc("readNextBatchOfRowsUntilThreshold: input stream is null.");
                }
            }
        }
        else
        {
            if(IS_TRACE_ON_CSC())
            {
                traceInfoCsc("readNextBatchOfRowsUntilThreshold: end of input stream already reached.");
            }
        }
    } // File created?
    else
    {
        if(IS_TRACE_ON_CSC())
        {
            traceInfoCsc("readNextBatchOfRowsUntilThreshold: file is not created.");
        }
    }

    return tuples;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read first batch of rows.
//
int pgReadFirstBatchOfRowsCsc(PGresult * res, int *piError) 
{
    int rc = FALSE;

    if(res)
    {
        PGresAttValue **tuples = NULL;
        int ntups = 0;

        tuples = readFirstBatchOfRowsCsc(res->m_cscResult, res, &ntups);

        if(tuples != NULL && ntups > 0)
        {
            // Put rows in pgResult
            res->ntups = ntups;
            res->tuples = tuples;
            res->m_tuplesAllocatedByCscRead = TRUE;
            rc = TRUE;
        }

        if(piError)
        {
            if(res->m_cscResult)
            {
                *piError = res->m_cscResult->m_ioe;
                res->m_cscResult->m_ioe = 0;
            }
            else
                *piError = 0;
        }
    }

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read first batch of rows.
//
PGresAttValue **readFirstBatchOfRowsCsc(ClientSideCursorResult *pCsc, PGresult * res,  int *pntups) 
{
    PGresAttValue **tuples = NULL;

    if(pCsc->m_fileCreated)
    {
        if(!doesItForwardOnlyCursor(pCsc->m_resultsettype))
        {
            if(pCsc->m_firstRowNumberInMem > 1)
            {
                pCsc->m_ioe = checkAndOpenInputStreamCsc(pCsc,FALSE);

                if(pCsc->m_ioe == 0)
                {
                    // Jump to the first row offset
                    pCsc->m_ioe = seekCscInputStream(pCsc->m_cscInputStream,0);
                    
                    pCsc->m_firstRowNumberInMem = 1;
                    pCsc->m_lastRowNumberInMem  =  pCsc->m_firstRowNumberInMem - 1;
                    
                    tuples = readNextBatchOfRowsUntilThresholdCsc(pCsc, res, FALSE, pntups);
                }
                else
                {
                    if(IS_TRACE_ON_CSC())
                    {
                        traceInfoCsc("readFirstBatchOfRows: File not found. File name = %s", pCsc->m_fileName);
                    }
                }
            } // On first batch?
            else
            {
                if(IS_TRACE_ON_CSC())
                {
                    traceInfoCsc("readFirstBatchOfRows: Cursor is already on first batch.");
                }
            }
        } // Scrollable?
        else
        {
            if(IS_TRACE_ON_CSC())
            {
                traceInfoCsc("readFirstBatchOfRows: Cursor is not scrollable.");
            }
        }
    }
    else
    {
        if(IS_TRACE_ON_CSC())
        {
            traceInfoCsc("readFirstBatchOfRows: file is not created.");
        }
    }

    return tuples;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Check whether we are at first batch in file.
//
int pgIsFirstBatchCsc(PGresult * pgResult)
{
    int rc = TRUE;

    if(pgResult && pgResult->m_cscResult)
        rc = isFirstBatchCsc(pgResult->m_cscResult);

    return rc;
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Check whether we are at first batch in file.
//
int isFirstBatchCsc(ClientSideCursorResult *pCsc)
{
    int rc;
    
    if(pCsc->m_fileCreated)
    {
       rc = (pCsc->m_firstRowNumberInMem <= 1);
    }
    else
    {
        // Everything was in memory
        rc = TRUE;
    }
    
    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read batch of rows for absolute position. rowNumber is 1 base. We will make it first row in the batch.
// We will set new row number to adjust in the current batch in memory.
//
int pgReadAbsoluteBatchOfRowsCsc(PGresult * res, int *piError, int *indexBuf) 
{
    int rc = FALSE;

    if(res)
    {
        PGresAttValue **tuples = NULL;
        int ntups = 0;

        tuples = readAbsoluteBatchOfRowsCsc(res->m_cscResult, res, &ntups, indexBuf);

        if(tuples != NULL && ntups > 0)
        {
            // Put rows in pgResult
            res->ntups = ntups;
            res->tuples = tuples;
            res->m_tuplesAllocatedByCscRead = TRUE;
            rc = TRUE;
        }

        if(piError)
        {
            if(res->m_cscResult)
            {
                *piError = res->m_cscResult->m_ioe;
                res->m_cscResult->m_ioe = 0;
            }
            else
                *piError = 0;
        }
    }

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read batch of rows for absolute position. rowNumber is 1 base. We will make it first row in the batch.
// We will set new row number to adjust in the current batch in memory.
//
PGresAttValue **readAbsoluteBatchOfRowsCsc(ClientSideCursorResult *pCsc, PGresult * res, int *pntups, int *indexBuf) 
{
    PGresAttValue **tuples = NULL;
    
    if(indexBuf != NULL)
    {
        if(pCsc->m_fileCreated)
        {
            if(!doesItForwardOnlyCursor(pCsc->m_resultsettype))
            {
                int rowNumber = *indexBuf;
                int    newRowNumber;
                int waitForWriteCommit = TRUE;
                
                if(rowNumber < 0)
                {
                    // Wait for full read
                    waitForFullResultReadFromServerCsc(pCsc);
                    
                    waitForWriteCommit = FALSE;
                    
                    // If the given row number is negative, the cursor moves to an absolute row position with respect to the end of the result set.
                    // For example, calling the method absolute(-1) positions the cursor on the last row;                        
                    rowNumber = pCsc->m_totalRows + rowNumber + 1; 
                }
                else
                {
                    // Check for row number should be less than committed rows in file.
                    if((rowNumber > 0) && (rowNumber > pCsc->m_totalCommitedRows))
                    {
                        // wait for file written up to required position.
                        do
                        {
                            // Wait for batch read
                            waitForBatchResultReadFromServerCsc(pCsc);
                            waitForWriteCommit = !isFullResultReadFromServerCsc(pCsc); 
                        }while(waitForWriteCommit && (rowNumber > pCsc->m_totalCommitedRows));
                    }
                }
                
                // Check it should between 1..Total
                if((rowNumber > 0) && (rowNumber <= pCsc->m_totalRows))
                {
                    // Check it should be out of current batch window in memory
                    if((rowNumber < pCsc->m_firstRowNumberInMem)
                        || (rowNumber > pCsc->m_lastRowNumberInMem))
                    {
                        long long rowOffset;

                        pCsc->m_ioe = checkAndOpenInputStreamCsc(pCsc,FALSE);

                        if(pCsc->m_ioe == 0)
                        {
                            // Jump to the given row 
                            rowOffset = readDataFileOffsetCsc(pCsc, rowNumber);
                            
                            if(rowOffset != -1)
                            {
                                pCsc->m_ioe = seekCscInputStream(pCsc->m_cscInputStream, rowOffset);

                                pCsc->m_firstRowNumberInMem = rowNumber;
                                pCsc->m_lastRowNumberInMem  =  pCsc->m_firstRowNumberInMem - 1;

                                if(pCsc->m_ioe)
                                    return tuples;

                                tuples = readNextBatchOfRowsUntilThresholdCsc(pCsc, res, waitForWriteCommit, pntups);
                            }
                        }
                        else
                        {
                            if(IS_TRACE_ON_CSC())
                            {
                                traceInfoCsc("readAbsoluteBatchOfRows: File not found. File name = %s", pCsc->m_fileName);
                            }
                        }
                    }
                    
                    // Adjust the absolute row number in current batch in memory.
                    newRowNumber = rowNumber - pCsc->m_firstRowNumberInMem + 1;
                    *indexBuf=newRowNumber;
                } // In the range?
                else
                {
                    if(IS_TRACE_ON_CSC())
                    {
                        traceInfoCsc("readAbsoluteBatchOfRows: rowNumber is not in the cursor range. rowNumber = %d", rowNumber);
                    }
                }
            } // Scrollable?
            else
            {
                if(IS_TRACE_ON_CSC())
                {
                    traceInfoCsc("readAbsoluteBatchOfRows: Cursor is not scrollable.");
                }
            }
        } // File
        else
        {
            if(IS_TRACE_ON_CSC())
            {
                traceInfoCsc("readAbsoluteBatchOfRows: file is not created.");
            }
        }
    } // indexBuf
    else
    {
        if(IS_TRACE_ON_CSC())
        {
            traceInfoCsc("readAbsoluteBatchOfRows: indexBuf is empty or null.");
        }
    }

    return tuples;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Even if csc enable, we need to see file created or not.
//
int pgIsFileCreatedCsc(PGresult * pgResult)
{
    int rc = FALSE;

    if(pgResult && pgResult->m_cscResult)
        rc = isFileCreatedCsc(pgResult->m_cscResult);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Even if csc enable, we need to see file created or not.
//
int isFileCreatedCsc(ClientSideCursorResult *pCsc)
{
    return pCsc->m_fileCreated;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Create lock for read/write in parallel
//
void createLockCsc(ClientSideCursorResult *pCsc) 
{
    pCsc->m_cscLock = createCscLock();

    // Simulate lock error
//            pCsc->m_cscLock = null;
    
    pCsc->m_ioe = (pCsc->m_cscLock == NULL);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Reset the lock variable in case of error condition.
//
void resetLockCsc(ClientSideCursorResult *pCsc)
{
    pCsc->m_cscLock = NULL;
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Wait full read from socket done for this cursor.
//
void waitForFullResultReadFromServerCsc(ClientSideCursorResult *pCsc) 
{
    // Wait for full read
    if(pCsc->m_cscLock != NULL)
    {
        if(!isFullResultReadFromServerCscLock(pCsc->m_cscLock))
        {
            waitForFullResultReadFromServerCscLock(pCsc->m_cscLock);
        }
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Signal full read from socket done for this cursor.
//
void signalForFullResultReadFromServerCsc(ClientSideCursorResult *pCsc) 
{
    // signal full read
    if(pCsc->m_cscLock != NULL)
    {
        signalForFullResultReadFromServerCscLock(pCsc->m_cscLock);
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Wait batch read from socket done for this cursor.
//
void waitForBatchResultReadFromServerCsc(ClientSideCursorResult *pCsc) 
{
    // Wait for batch read
    if(pCsc->m_cscLock != NULL)
    {
        if(!isFullResultReadFromServerCscLock(pCsc->m_cscLock))
        {
            waitForBatchResultReadFromServerCscLock(pCsc->m_cscLock);
        }
    }
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Signal batch read from socket done for this cursor.
//
void signalForBatchResultReadFromServerCsc(ClientSideCursorResult *pCsc) 
{
    // signal batch read
    if(pCsc->m_cscLock != NULL)
    {
        signalForBatchResultReadFromServerCscLock(pCsc->m_cscLock);
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set IO exception from thread.
//
void setIOExceptionCsc(ClientSideCursorResult *pCsc, int ioe)
{
    pCsc->m_ioe = ioe;
    signalForFullResultReadFromServerCsc(pCsc);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read IO exception occurred in csc thread.
//
int getIOExceptionCsc(ClientSideCursorResult *pCsc)
{
    int ioe = pCsc->m_ioe;
    
    pCsc->m_ioe = 0;
    
    return ioe;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Is full read from socket done for this cursor.
//
int isFullResultReadFromServerCsc(ClientSideCursorResult *pCsc) 
{
    int rc;
    
    if(pCsc->m_cscLock != NULL)
    {
       rc = isFullResultReadFromServerCscLock(pCsc->m_cscLock);
    }
    else
    {
        rc = TRUE;
    }
    
    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get execution number associated with csc. (i.e. 1..N is valid, 0 means no CSC) 
//
int pgGetExecuteNumberCsc(PGresult * pgResult)
{
    int rc = 0;

    if(pgResult && pgResult->m_cscResult)
    {
        rc = getExecuteIndexCsc(pgResult->m_cscResult);
        rc += 1; // Number = Index + 1;
    }

    return rc; 
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get execution index associated with csc.
//
int getExecuteIndexCsc(ClientSideCursorResult *pCsc)
{
    return pCsc->m_executeIndex;        
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Do we want to skip result?
//
int isSkipResultCsc(ClientSideCursorResult *pCsc)
{
    return pCsc->m_skipResult;        
}


