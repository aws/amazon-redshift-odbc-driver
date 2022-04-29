/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "ClientSideCursorTrace.h"

FILE    *g_CscFpTrace; // Trace file handle, set if trace level is >= TRACE_INFO

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set trace level and trace file info.
//
void setTraceInfoCsc(FILE    *fpTrace)
{
    g_CscFpTrace = fpTrace;
}

/*====================================================================================================================================================*/

void traceInfoCsc(char *fmt,...)
{
    if(IS_TRACE_ON_CSC())
    {
        WRITE_INTO_TRACE_FILE_CSC();
    }
}
