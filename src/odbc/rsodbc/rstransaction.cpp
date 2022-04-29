/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "rstransaction.h"

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// In ODBC 3.x, the ODBC 2.x function SQLTransact has been replaced by SQLEndTran.
//
SQLRETURN  SQL_API SQLTransact(SQLHENV phenv,
                               SQLHDBC phdbc, 
                               SQLUSMALLINT hCompletionType)
{
    SQLRETURN   rc;

    beginApiMutex((phdbc) ? NULL : phenv, phdbc);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLTransact(FUNC_CALL, 0, phenv, phdbc, hCompletionType);

    rc = RsTransaction::RS_SQLTransact(phenv, phdbc, hCompletionType);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLTransact(FUNC_RETURN, rc, phenv, phdbc, hCompletionType);

    endApiMutex((phdbc) ? NULL : phenv, phdbc);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLTransact and SQLEndTran.
//

SQLRETURN  SQL_API RsTransaction::RS_SQLTransact(SQLHENV phenv,
                                   SQLHDBC phdbc, 
                                   SQLUSMALLINT hCompletionType)
{
    SQLRETURN   rc = SQL_SUCCESS;
    char *cmd;

    if(!VALID_HENV(phenv)
        && !VALID_HENV(phdbc))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    if(hCompletionType == SQL_COMMIT)
        cmd = COMMIT_CMD;
    else
    if(hCompletionType == SQL_ROLLBACK)
        cmd = ROLLBACK_CMD;
    else
    {
        rc = SQL_ERROR;
        goto error;
    }

    if(phdbc != NULL)
    {
        RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;

        // Clear error list
        pConn->pErrorList = clearErrorList(pConn->pErrorList);

        if(!libpqIsTransactionIdle(pConn))
        {
            rc = libpqExecuteTransactionCommand(pConn, cmd, TRUE);
            if(rc == SQL_ERROR)
                goto error;
        }
    }
    else
    if(phenv != NULL)
    {
        RS_ENV_INFO *pEnv = (RS_ENV_INFO *)phenv;
        RS_CONN_INFO *curr;

        // Clear error list
        pEnv->pErrorList = clearErrorList(pEnv->pErrorList);

        // Do it for each connection
        // Disconnect/free connection
        curr = pEnv->phdbcHead;
        while(curr != NULL)
        {
            RS_CONN_INFO *next = curr->pNext;

            // Clear error list
            curr->pErrorList = clearErrorList(curr->pErrorList);

            if(!libpqIsTransactionIdle(curr))
            {
                rc = libpqExecuteTransactionCommand(curr, cmd, TRUE);

                if(rc == SQL_ERROR)
                    break;
            }

            curr = next;
        }

        if(rc == SQL_ERROR)
            goto error;
    }

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLEndTran requests a commit or rollback operation for all active operations on all statements associated with a connection. 
// SQLEndTran can also request that a commit or rollback operation be performed for all connections associated with an environment.
//
SQLRETURN  SQL_API SQLEndTran(SQLSMALLINT hHandleType, 
                                SQLHANDLE pHandle,
                                SQLSMALLINT hCompletionType)
{
    SQLRETURN   rc;

    beginApiMutex((hHandleType == SQL_HANDLE_ENV) ? pHandle : NULL, 
                    (hHandleType == SQL_HANDLE_DBC) ? pHandle : NULL);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLEndTran(FUNC_CALL, 0, hHandleType, pHandle, hCompletionType);

    if(hHandleType == SQL_HANDLE_ENV)
        rc =  RsTransaction::RS_SQLTransact(pHandle,NULL,hCompletionType);
    else
    if(hHandleType == SQL_HANDLE_DBC)
        rc =  RsTransaction::RS_SQLTransact(NULL,pHandle,hCompletionType);
    else
        rc = SQL_ERROR;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLEndTran(FUNC_RETURN, rc, hHandleType, pHandle, hCompletionType);

    endApiMutex((hHandleType == SQL_HANDLE_ENV) ? pHandle : NULL, 
                    (hHandleType == SQL_HANDLE_DBC) ? pHandle : NULL);

    return rc;
}


