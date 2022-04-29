/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/ 

#include "ClientSideCursorInputStream.h"

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Initialize the csc input stream.
//
ClientSideCursorInputStream *createCscInputStream(char *fileName, int resultsettype,int *pError) 
{
    ClientSideCursorInputStream *pCscInputStream = rs_calloc(1,sizeof(ClientSideCursorInputStream));

    if(pCscInputStream)
    {
        pCscInputStream->m_resultsettype = resultsettype;
        
        pCscInputStream->m_dataInputStream = rs_fopen(fileName,"r+b");

        if(pCscInputStream->m_dataInputStream)
            setvbuf(pCscInputStream->m_dataInputStream, NULL, _IOFBF, 8 * 1024);

        // Set error, if we couldn't open the file.
        *pError = (pCscInputStream->m_dataInputStream == NULL);
    }
    else
        *pError = TRUE;

    return pCscInputStream;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release the csc input stream.
//
ClientSideCursorInputStream *releaseCscInputStream(ClientSideCursorInputStream *pCscInputStream) 
{
    if(pCscInputStream)
    {
        pCscInputStream = rs_free(pCscInputStream);
    }

    return NULL;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Close the stream. 0 means successful.
//
int closeCscInputStream(ClientSideCursorInputStream *pCscInputStream) 
{
    int rc;

    if(pCscInputStream->m_dataInputStream != NULL)
    {
        rc = fclose(pCscInputStream->m_dataInputStream);

        pCscInputStream->m_dataInputStream = NULL;
    }
    else
        rc = 0;

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read short.
//
short readShortCscInputStream(ClientSideCursorInputStream *pCscInputStream) 
{
    short v;

    freadCsc(pCscInputStream,&v,sizeof(short),1,pCscInputStream->m_dataInputStream);

    return v;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read short.
//
short readShortCscInputStreamForNumberOfCols(ClientSideCursorInputStream *pCscInputStream) 
{
    short v;

    freadCsc(pCscInputStream,&v,sizeof(short),1,pCscInputStream->m_dataInputStream);

    if(feofCscInputStream(pCscInputStream))
        v = 0;

    return v;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read Integer.
//
int readIntCscInputStream(ClientSideCursorInputStream *pCscInputStream)
{
    int v;

    freadCsc(pCscInputStream,&v,sizeof(int),1,pCscInputStream->m_dataInputStream);

    return v;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read long.
//
long long readLongLongCscInputStream(ClientSideCursorInputStream *pCscInputStream) 
{
    long long v;

    freadCsc(pCscInputStream,&v,sizeof(long long),1,pCscInputStream->m_dataInputStream);

    return v;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read bytes.
//
int readCscInputStream(ClientSideCursorInputStream *pCscInputStream,char b[], int off, int len) 
{
    return (int) freadCsc(pCscInputStream,b+off,sizeof(char),len,pCscInputStream->m_dataInputStream);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Seek to new position. 0 means successful.
//
int seekCscInputStream(ClientSideCursorInputStream *pCscInputStream, long long pos) 
{
    int rc;

    if(!doesItForwardOnlyCursor(pCscInputStream->m_resultsettype))
    {
        rc = rs_fseeko(pCscInputStream->m_dataInputStream, pos, SEEK_SET);

        fflush(pCscInputStream->m_dataInputStream);
    }
    else
        rc = 0;

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read, set IO error or eof
//
size_t freadCsc(ClientSideCursorInputStream *pCscInputStream, void * _DstBuf, size_t _ElementSize, size_t _Count, FILE * _File)
{
    size_t rc;

    if(!feof(pCscInputStream->m_dataInputStream))
    {
        rc = fread(_DstBuf,_ElementSize,_Count,pCscInputStream->m_dataInputStream);

        if(rc == 0)
        {
            // Check one more time EOF
            if(feof(pCscInputStream->m_dataInputStream))
            {
                pCscInputStream->m_eof = TRUE;
            }
            else
                setIOErrorCsc(&(pCscInputStream->m_error),TRUE);
            }
        }
        else
        {
        rc = 0;
        pCscInputStream->m_eof = TRUE;
    }

    return rc;
}

/*====================================================================================================================================================*/

int feofCscInputStream(ClientSideCursorInputStream *pCscInputStream)
{
    if(!pCscInputStream->m_eof)
        pCscInputStream->m_eof = feof(pCscInputStream->m_dataInputStream);

    return pCscInputStream->m_eof;
}
