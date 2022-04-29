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

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef WIN32
#include "csc_win_port.h"
#endif // WIN32

#ifdef LINUX
#include <unistd.h>
#include "csc_linux_port.h"
#endif

#include "rsfile.h"
#include "rsmem.h"

/**
 * Multiplex output stream for forward only cursor and scrollable cursor.
 * 
 *
 */
typedef struct _ClientSideCursorOutputStream
{
    // Output stream for cursor.
    FILE *m_dataOutputStream;

    int    m_fd;
    
    // Resultset type
    int m_resultsettype;
    
    // The number of bytes written to the data output stream so far. 
    // If this counter overflows, it will be wrapped to Long.MAX_VALUE.
    long long m_written;
}ClientSideCursorOutputStream;

#ifdef __cplusplus
extern "C" 
{
#endif /* C++ */

ClientSideCursorOutputStream *createCscOutputStream(char *fileName, int resultsettype, int *pError);
ClientSideCursorOutputStream *releaseCscOutputStream(ClientSideCursorOutputStream *pCscOutputStream);
int closeCscOutputStream(ClientSideCursorOutputStream *pCscOutputStream);
int writeShortCscOutputStream(ClientSideCursorOutputStream *pCscOutputStream,short v);
int writeIntCscOutputStream(ClientSideCursorOutputStream *pCscOutputStream,int v);
int writeLongLongCscOutputStream(ClientSideCursorOutputStream *pCscOutputStream, long long v);
int writeCscOutputStream(ClientSideCursorOutputStream *pCscOutputStream, char b[], int off, int len);
long long sizeCscOutputStream(ClientSideCursorOutputStream *pCscOutputStream);
long long getPositionCscOutputStream(ClientSideCursorOutputStream *pCscOutputStream);
int flushCscOutputStream(ClientSideCursorOutputStream *pCscOutputStream);
int doesItForwardOnlyCursor(int resultsettype);
void incCountCscOutputStream(ClientSideCursorOutputStream *pCscOutputStream, int value);
void setIOErrorCsc(int *pError, int value);
int getIOErrorCsc(int *pError);
void resetIOErrorCsc(int *pError);

#ifdef __cplusplus
}
#endif /* C++ */

