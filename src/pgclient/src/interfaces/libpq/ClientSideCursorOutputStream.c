/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "ClientSideCursorOutputStream.h"

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Initialize the csc output stream.
//
ClientSideCursorOutputStream *createCscOutputStream(char *fileName, int resultsettype,int *pError) 
{
    ClientSideCursorOutputStream *pCscOutputStream = rs_calloc(1, sizeof(ClientSideCursorOutputStream));

    if(pCscOutputStream)
    {
        char *mode =
#ifdef WIN32
           "w+bc"; // Sync mode for commit-to-disk when flush called.
#endif
#if defined LINUX 
            "w+b";
#endif
            
        pCscOutputStream->m_resultsettype = resultsettype;
        
        pCscOutputStream->m_dataOutputStream = rs_fopen(fileName,mode);

        if(pCscOutputStream->m_dataOutputStream)
            setvbuf(pCscOutputStream->m_dataOutputStream, NULL, _IOFBF, 8 * 1024);

        pCscOutputStream->m_fd = rs_fileno(pCscOutputStream->m_dataOutputStream);
        
        pCscOutputStream->m_written = 0;

        // Set error, if we couldn't open the file.
        *pError = (pCscOutputStream->m_dataOutputStream == NULL);
    }
    else
        *pError = TRUE;

    return pCscOutputStream;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release the csc output stream.
//
ClientSideCursorOutputStream *releaseCscOutputStream(ClientSideCursorOutputStream *pCscOutputStream) 
{
    if(pCscOutputStream)
    {
        pCscOutputStream->m_resultsettype = 0;
        pCscOutputStream->m_dataOutputStream = NULL;
        pCscOutputStream->m_fd = 0;
        pCscOutputStream->m_written = 0;
        pCscOutputStream = rs_free(pCscOutputStream);
    }

    return NULL;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Close the stream. 0 means successful.
//
int closeCscOutputStream(ClientSideCursorOutputStream *pCscOutputStream) 
{
    int rc;

    if(pCscOutputStream->m_dataOutputStream != NULL)
    {
        rc = fclose(pCscOutputStream->m_dataOutputStream);

        pCscOutputStream->m_dataOutputStream = NULL;
        pCscOutputStream->m_fd = 0;
        pCscOutputStream->m_written = 0;
    }
    else
        rc = 0;

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Write short. 0 means successful.
//
int writeShortCscOutputStream(ClientSideCursorOutputStream *pCscOutputStream,short v) 
{
    size_t rc = fwrite(&v,sizeof(short),1,pCscOutputStream->m_dataOutputStream);

    incCountCscOutputStream(pCscOutputStream,2);

    return (rc == 0);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Write integer. 0 means successful.
//
int writeIntCscOutputStream(ClientSideCursorOutputStream *pCscOutputStream,int v) 
{
    size_t rc = fwrite(&v,sizeof(int),1,pCscOutputStream->m_dataOutputStream);

    incCountCscOutputStream(pCscOutputStream, 4);

    return (rc == 0);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Write long long. 0 means successful.
//
int writeLongLongCscOutputStream(ClientSideCursorOutputStream *pCscOutputStream, long long v) 
{
    size_t rc = fwrite(&v,sizeof(long long),1,pCscOutputStream->m_dataOutputStream);

    incCountCscOutputStream(pCscOutputStream, 8);

    return (rc == 0);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Write bytes. 0 means successful.
//
int writeCscOutputStream(ClientSideCursorOutputStream *pCscOutputStream, char b[], int off, int len) 
{
    size_t rc = fwrite(b+off,sizeof(char),len,pCscOutputStream->m_dataOutputStream);

    incCountCscOutputStream(pCscOutputStream,len);

    return (rc == 0);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Size of the stream.
//
long long sizeCscOutputStream(ClientSideCursorOutputStream *pCscOutputStream) 
{
    return pCscOutputStream->m_written; 
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get file pointer position.
//
long long getPositionCscOutputStream(ClientSideCursorOutputStream *pCscOutputStream) 
{
    if(doesItForwardOnlyCursor(pCscOutputStream->m_resultsettype))
    {
        return -1;
    }
    else
    {
        fflush(pCscOutputStream->m_dataOutputStream);
        
        return rs_ftello(pCscOutputStream->m_dataOutputStream);
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Flush the output. 0 means successful.
//
int flushCscOutputStream(ClientSideCursorOutputStream *pCscOutputStream) 
{
    int rc;

    if(doesItForwardOnlyCursor(pCscOutputStream->m_resultsettype))
    {
        fflush(pCscOutputStream->m_dataOutputStream);

        rc = rs_fsync(pCscOutputStream->m_fd);
    }
    else
        rc = 0;

    return rc;
} 

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Check for ODBC cursor type.
//
int doesItForwardOnlyCursor(int resultsettype)
{
    return (resultsettype == SQL_CURSOR_FORWARD_ONLY);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Increases the written counter by the specified value
// until it reaches Integer.MAX_VALUE.
//
void incCountCscOutputStream(ClientSideCursorOutputStream *pCscOutputStream, int value) 
{
    long long temp = pCscOutputStream->m_written + value;

    if (temp < 0) 
    {
        temp = LLONG_MAX;
    }
    pCscOutputStream->m_written = temp;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set IO error
//
void setIOErrorCsc(int *pError, int value)
{
    if(pError && *pError == 0)
        *pError = value;

}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get IO error
//
int getIOErrorCsc(int *pError)
{
    int error = *pError;

    resetIOErrorCsc(pError);

    return error;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Reset IO error
//
void resetIOErrorCsc(int *pError)
{
    if(pError)
        *pError = 0;
}
