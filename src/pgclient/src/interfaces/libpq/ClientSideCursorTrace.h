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
#include <stdarg.h>

// Golbal var
extern FILE    *g_CscFpTrace; // Trace file handle, set if trace level is >= TRACE_INFO

#define IS_TRACE_ON_CSC() (g_CscFpTrace != NULL)

#define WRITE_INTO_TRACE_FILE_CSC() \
        if(g_CscFpTrace) \
        {                          \
            va_list args;         \
            va_start(args, fmt);  \
            vfprintf(g_CscFpTrace, fmt, args); \
            fprintf(g_CscFpTrace,"\n"); \
            fflush(g_CscFpTrace); \
            va_end(args); \
        }

void setTraceInfoCsc(FILE    *fpTrace);
void traceInfoCsc(char *fmt,...);


