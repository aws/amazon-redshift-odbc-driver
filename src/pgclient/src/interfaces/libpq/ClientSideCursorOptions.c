/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "ClientSideCursorOptions.h"

// Default is temp directory
static char DFLT_CSC_PATH[MAX_PATH + 1] = "";

#if defined LINUX 
int GetTempPath(int size, char *pBuf);
#endif

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Initialize the csc options with defaults
//
ClientSideCursorOptions *createCscOptionsWithDflts(void)
{
    setDfltCscPath();

    return createCscOptions(DFLT_CSC_ENABLE, DFLT_CSC_THRESHOLD, DFLT_CSC_MAX_FILE_SIZE, DFLT_CSC_PATH);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Store csc options.
//
ClientSideCursorOptions *createCscOptions(int enable, long long threshold, long long maxfilesize, char *path)
{
    ClientSideCursorOptions *pCscOptions = rs_calloc(1,sizeof(ClientSideCursorOptions));

    if(pCscOptions)
    {
        setEnableCscOption(pCscOptions, enable);
        setThresholdCscOption(pCscOptions, threshold);
        setMaxFileSizeCscOption(pCscOptions, maxfilesize);
        setPathCscOption(pCscOptions, path);

        if(IS_TRACE_ON_CSC())
        {
            traceInfoCsc("csc options: cscenable=%d, cscthreshold=%lld, cscmaxfilesize=%lld, cscpath = %s",
                            pCscOptions->m_enable, pCscOptions->m_threshold, pCscOptions->m_maxfilesize, pCscOptions->m_path);
        }
    }

    return pCscOptions;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release resources of the csc options.
//
ClientSideCursorOptions *releaseCscOptions(ClientSideCursorOptions *pCscOptions)
{
    if(pCscOptions)
    {
        pCscOptions = rs_free(pCscOptions);
    }

    return NULL;
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set enable
//
void setEnableCscOption(ClientSideCursorOptions *pCscOptions,int enable)
{
    pCscOptions->m_enable = enable;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set threshold. Threshold parameter is in MB.
//
void setThresholdCscOption(ClientSideCursorOptions *pCscOptions, long long threshold)
{
    pCscOptions->m_threshold = (threshold <= 0) ? 0 : (threshold * 1024L * 1024);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set max file size. maxfilesize parameter is in MB.
//
void setMaxFileSizeCscOption(ClientSideCursorOptions *pCscOptions, long long maxfilesize)
{
    pCscOptions->m_maxfilesize = (maxfilesize <= 0) ? 0 : (maxfilesize * 1024L * 1024);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set path.
//
void setPathCscOption(ClientSideCursorOptions *pCscOptions, char *path)
{
    int addPathSeparator = FALSE;

    // Set first the default path, if it's empty.
    setDfltCscPath();

    if(path != NULL && strlen(path) > 0)
    {
        if(path[strlen(path) - 1] != PATH_SEPARATOR_CHAR)
        {
            // If last character is not path separator add it.
            addPathSeparator = TRUE;
        }

#ifdef WIN32
        if(_access(path,0) == 0)
#endif
#if defined LINUX 
        if(access(path,0) == 0)
#endif
        {
            strncpy(pCscOptions->m_path, path, MAX_PATH - 1);
        }
        else
        {
            strncpy(pCscOptions->m_path, DFLT_CSC_PATH, MAX_PATH - 1);
        }
    }
    else
    {
        strncpy(pCscOptions->m_path, DFLT_CSC_PATH, MAX_PATH - 1);
    }

    if(!addPathSeparator)
    {
        if(pCscOptions->m_path != NULL
                && pCscOptions->m_path[strlen(pCscOptions->m_path) - 1] != PATH_SEPARATOR_CHAR)
        {
            // If last character is not path separator add it.
            addPathSeparator = TRUE;
        }
    }

    if(addPathSeparator)
        strncat(pCscOptions->m_path,PATH_SEPARATOR,sizeof(pCscOptions->m_path)-strlen(pCscOptions->m_path));
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get enable.
//
int getEnableCscOption(ClientSideCursorOptions *pCscOptions)
{
    return pCscOptions->m_enable;
}

//---------------------------------------------------------------------------------------------------------igarish
// Get threshold.
//
long long getThresholdCscOption(ClientSideCursorOptions *pCscOptions)
{
    return pCscOptions->m_threshold;
}

//---------------------------------------------------------------------------------------------------------igarish
// Get threshold.
//
long long  getMaxFileSizeCscOption(ClientSideCursorOptions *pCscOptions)
{
    return pCscOptions->m_maxfilesize;
}

//---------------------------------------------------------------------------------------------------------igarish
// Get path.
//
char *getPathCscOption(ClientSideCursorOptions *pCscOptions)
{
    return pCscOptions->m_path;
}

//---------------------------------------------------------------------------------------------------------igarish
// Set default csc path.
//
void setDfltCscPath(void)
{
    if(DFLT_CSC_PATH[0] == '\0')
    {
        int dwRetVal = 0;

        dwRetVal = GetTempPath(MAX_PATH, DFLT_CSC_PATH);
        if (dwRetVal > MAX_PATH || (dwRetVal == 0))
            DFLT_CSC_PATH[0] = '\0';
    }
}


/*====================================================================================================================================================*/

#if defined LINUX 

//---------------------------------------------------------------------------------------------------------igarish
// Get the temp path.
//
int GetTempPath(int size, char *pBuf)
{
    char *pPath = getenv("TMPDIR");

    if (!pPath || *pPath == '\0')
        pPath = "/tmp";

    if(pBuf)
        strncpy(pBuf,pPath,size);

    return (pBuf) ? strlen(pPath) : 0;
}

#endif
