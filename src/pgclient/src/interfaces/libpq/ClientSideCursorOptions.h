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
#include <io.h>
#include "csc_win_port.h"
#endif // WIN32

#ifdef LINUX
#include "csc_linux_port.h"
#endif

#include "rsfile.h"
#include "rslock.h"
#include "rsmem.h"
#include "ClientSideCursorTrace.h"

    
/**
 * Constants
 */

// Default is disable
#define DFLT_CSC_ENABLE FALSE

// Default is 16MB
#define DFLT_CSC_THRESHOLD  1 // in MB 1, 16

// Default is 4000MB
#define DFLT_CSC_MAX_FILE_SIZE  (4 * 1024L) // in MB


/**
 * This class contains configuration of Client side cursor.
 * We will allocate it per statement execution, so we can maintain it for each statement.
 * 
 *
 */
typedef struct _ClientSideCursorOptions 
{
    // Is Client Side Cursor enabled?
    int m_enable;
    
    // CSC threshold limit in bytes
    long long m_threshold;
    
    // CSC max file size in bytes per result
    long long m_maxfilesize;
    
    // CSC file storage path
    char m_path[MAX_PATH + 1];
}ClientSideCursorOptions ;    

// Function declarations

#ifdef __cplusplus
extern "C" 
{
#endif /* C++ */

ClientSideCursorOptions *createCscOptionsWithDflts(void);
ClientSideCursorOptions *createCscOptions(int enable, long long threshold, long long maxfilesize, char *path);
void setEnableCscOption(ClientSideCursorOptions *pCscOptions,int enable);
void setThresholdCscOption(ClientSideCursorOptions *pCscOptions, long long threshold);
void setMaxFileSizeCscOption(ClientSideCursorOptions *pCscOptions, long long maxfilesize);
void setPathCscOption(ClientSideCursorOptions *pCscOptions, char *path);
int getEnableCscOption(ClientSideCursorOptions *pCscOptions);
long long getThresholdCscOption(ClientSideCursorOptions *pCscOptions);
long long  getMaxFileSizeCscOption(ClientSideCursorOptions *pCscOptions);
char *getPathCscOption(ClientSideCursorOptions *pCscOptions);
ClientSideCursorOptions *releaseCscOptions(ClientSideCursorOptions *pCscOptions);
void setDfltCscPath(void);

#ifdef __cplusplus
}
#endif /* C++ */

