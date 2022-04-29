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
 * Multiplex input stream for forward only cursor and scrollable cursor.
 *
 *
 */
typedef struct _ClientSideCursorInputStream
{
    // Input stream for cursor.
    FILE * m_dataInputStream;

    // Resultset type
    int m_resultsettype;

    // IO error indicator
    int m_error;

    // EOF indicator
    int m_eof;

}ClientSideCursorInputStream ;

#ifdef __cplusplus
extern "C"
{
#endif /* C++ */

ClientSideCursorInputStream *createCscInputStream(char *fileName, int resultsettype, int *pError);
ClientSideCursorInputStream *releaseCscInputStream(ClientSideCursorInputStream *pCscInputStream);
int closeCscInputStream(ClientSideCursorInputStream *pCscInputStream);
short readShortCscInputStream(ClientSideCursorInputStream *pCscInputStream);
short readShortCscInputStreamForNumberOfCols(ClientSideCursorInputStream *pCscInputStream);
int readIntCscInputStream(ClientSideCursorInputStream *pCscInputStream);
long long readLongLongCscInputStream(ClientSideCursorInputStream *pCscInputStream);
int readCscInputStream(ClientSideCursorInputStream *pCscInputStream,char b[], int off, int len);
int seekCscInputStream(ClientSideCursorInputStream *pCscInputStream, long long pos);
int doesItForwardOnlyCursor(int resultsettype);
void setIOErrorCsc(int *pError, int value);
int getIOErrorCsc(int *pError);
void resetIOErrorCsc(int *pError);
size_t freadCsc(ClientSideCursorInputStream *pCscInputStream, void * _DstBuf, size_t _ElementSize, size_t _Count, FILE * _File);
int feofCscInputStream(ClientSideCursorInputStream *pCscInputStream);

#ifdef __cplusplus
}
#endif /* C++ */
