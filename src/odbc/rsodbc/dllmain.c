/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

// dllmain.cpp : Defines the entry point for the DLL application.
#ifdef WIN32

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "rsodbc.h"
#include "rsutil.h"
#include "rsunicode.h"
#include <rslog.h>

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// DLL entry point method. 
//
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        {
//			MessageBox(NULL, "in DLLMain", NULL, MB_OK);
            // For muliple threads it improve performance.
            DisableThreadLibraryCalls(hModule);

            initODBC(hModule);

            if(IS_TRACE_LEVEL_DEBUG())
                RS_LOG_DEBUG("ODBCDLL", "DLL_PROCESS_ATTACH");

            break;
        }

        case DLL_THREAD_ATTACH:
        {
            if(IS_TRACE_LEVEL_DEBUG())
                RS_LOG_DEBUG("ODBCDLL", "DLL_THREAD_ATTACH");

            break;
        }

        case DLL_THREAD_DETACH:
        {
            if(IS_TRACE_LEVEL_DEBUG())
                RS_LOG_DEBUG("ODBCDLL", "DLL_THREAD_DETACH");

            break;
        }

        case DLL_PROCESS_DETACH:
        {
            if(IS_TRACE_LEVEL_DEBUG())
                RS_LOG_DEBUG("ODBCDLL", "DLL_PROCESS_DETACH");

            uninitODBC();

            break;
        }
    }

    return TRUE;
}

#endif // WIN32
