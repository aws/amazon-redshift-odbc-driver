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
#include "ClientSideCursorResult.h"
#include "ClientSideCursorTrace.h"

// Hashmap data as list
typedef struct _ClientSideCursorHashMapData
{
    void *pData; // In this case csc
    struct _ClientSideCursorHashMapData *pNext;
}ClientSideCursorHashMapData;

// Hashmap key and data head. Implemented as list.
typedef struct _ClientSideCursorHashMap
{
    int m_key;
    ClientSideCursorHashMapData *pDataHead;
    struct _ClientSideCursorHashMap *pNext;
}ClientSideCursorHashMap;

/**
 * Store csc for cleanup during exit.
 * 
 */
typedef struct _ClientSideCursorShutdownHook
{
    // List of connections having unclosed CSC objects 
    ClientSideCursorHashMap *m_unclosedCscConnectionMap;
    SEM_HANDLE m_hHashSem;
}ClientSideCursorShutdownHook;

// Function declarations

#ifdef __cplusplus
extern "C" 
{
#endif /* C++ */

ClientSideCursorShutdownHook *createCscShutdownHook(void);
ClientSideCursorShutdownHook *releaseCscShutdownHook(ClientSideCursorShutdownHook *pCscShutdownHook);
void runCscShutdownHook(void);
void addCscForCleanupOnExit(ClientSideCursorResult *pCsc);
void removeCscFromCleanupOnExit(ClientSideCursorResult *pCsc, int iCalledFromShutdownHook);
void closeAllCsc(int connectionPid);
void addCscHashMapData(ClientSideCursorHashMap *pCscHashMap, void *pData);
void releaseCscHashMapData(ClientSideCursorHashMap *pCscHashMap, void *pData);
void addCscHashMapKeyAndData(ClientSideCursorShutdownHook *pCscShutdownHook, int key, void *pData);
void releaseCscHashMapKeyAndData(ClientSideCursorShutdownHook *pCscShutdownHook, int key, void *pData, int iCalledFromShutdownHook);
ClientSideCursorHashMap *findCscHashMapUsingKey(ClientSideCursorShutdownHook *pCscShutdownHook, int key,int removeOpertaion);

ClientSideCursorShutdownHook *getCscShutdownHook(void);

#ifdef __cplusplus
}
#endif /* C++ */


