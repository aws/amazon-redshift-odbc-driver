/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "ClientSideCursorShutdownHook.h"

static ClientSideCursorShutdownHook *gpCscShutdownHook = NULL;

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Initialize the csc shutdown hook
//
ClientSideCursorShutdownHook *createCscShutdownHook(void)
{
    ClientSideCursorShutdownHook *pCscShutdownHook = rs_calloc(1,sizeof(ClientSideCursorShutdownHook));

    if(pCscShutdownHook)
    {
        pCscShutdownHook->m_hHashSem = rsCreateBinarySem(1);

        // Add at exit handler
        atexit(runCscShutdownHook);
    }

    gpCscShutdownHook = pCscShutdownHook;

    return pCscShutdownHook;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release the csc shutdown hook
//
ClientSideCursorShutdownHook *releaseCscShutdownHook(ClientSideCursorShutdownHook *pCscShutdownHook)
{
    if(pCscShutdownHook)
    {
        rsDestroySem(pCscShutdownHook->m_hHashSem);
        pCscShutdownHook->m_hHashSem = NULL;

        pCscShutdownHook = rs_free(pCscShutdownHook);
        gpCscShutdownHook = NULL;
    }

    return NULL;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get the csc shutdown hook
//
ClientSideCursorShutdownHook *getCscShutdownHook(void)
{
    return gpCscShutdownHook;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Method executed on exit.
//
void runCscShutdownHook(void) 
{
    // Close all
    if(gpCscShutdownHook && gpCscShutdownHook->m_unclosedCscConnectionMap != NULL)
    {
        ClientSideCursorShutdownHook *pCscShutdownHook = gpCscShutdownHook;

        // Get connection key values.
        ClientSideCursorHashMap *pCscHashMap = pCscShutdownHook->m_unclosedCscConnectionMap;
        ClientSideCursorHashMap *pCscNextHashMap;

        // iterate through HashMap key values iterator.   
        while(pCscHashMap != NULL)
        {
            pCscNextHashMap = pCscHashMap->pNext;

            closeAllCsc(pCscHashMap->m_key);

            pCscHashMap = pCscNextHashMap;
        } // Connection loop

        pCscShutdownHook->m_unclosedCscConnectionMap = NULL;
    }

    gpCscShutdownHook = releaseCscShutdownHook(gpCscShutdownHook);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Add CSC object as unclosed object.
//
void addCscForCleanupOnExit(ClientSideCursorResult *pCsc)
{
    if(gpCscShutdownHook)
    {
        // Get list of cscs for a connection
        int connectionPid = getConnectionPidCsc(pCsc);

        addCscHashMapKeyAndData(gpCscShutdownHook, connectionPid, pCsc);
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Remove CSC object from unclosed object list.
//
void removeCscFromCleanupOnExit(ClientSideCursorResult *pCsc,int iCalledFromShutdownHook)
{
    if(gpCscShutdownHook)
    {
        // Get list of cscs for a connection
        int connectionPid = getConnectionPidCsc(pCsc);

        // Remove from the list for the connection.
        // If all csc for a connection is closed then remove from the map.
        releaseCscHashMapKeyAndData(gpCscShutdownHook, connectionPid, pCsc, iCalledFromShutdownHook);
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Close all of a connection related csc(s). Caller must be synchronized.
//
void closeAllCsc(int connectionPid)
{
    if(gpCscShutdownHook)
    {
        ClientSideCursorShutdownHook *pCscShutdownHook = gpCscShutdownHook;

        // Lock the map
        rsLockSem(pCscShutdownHook->m_hHashSem);

        if(pCscShutdownHook->m_unclosedCscConnectionMap != NULL)
        {
            ClientSideCursorHashMap *pCscHashMap = findCscHashMapUsingKey(pCscShutdownHook, connectionPid, FALSE);

            if(pCscHashMap)
            {
              ClientSideCursorHashMapData *pUnclosedCscs = pCscHashMap->pDataHead;
                
                // Is there any unclosed object?
                if(pUnclosedCscs != NULL)
                {
                    if(IS_TRACE_ON_CSC())
                    {
                        traceInfoCsc("closeAllCsc: cloing csc for connection: %d", connectionPid);
                    }

                    // Unclosed CSC object list
                    while(pUnclosedCscs != NULL)
                    {
                        ClientSideCursorResult *pCsc = (ClientSideCursorResult *)(pUnclosedCscs->pData);
                        ClientSideCursorHashMapData *pNextUnclosedCscs = pUnclosedCscs->pNext;
                        
                        if(pCsc != NULL)
                        {
                            closeCsc(pCsc, TRUE);
                            pUnclosedCscs->pData = NULL;
                        }

                        pUnclosedCscs = pNextUnclosedCscs;
                    } // Object loop
                }
            }
        }

        // Unlock the map
        rsUnlockSem(pCscShutdownHook->m_hHashSem);
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Add hashmap data in the list.
//
void addCscHashMapData(ClientSideCursorHashMap *pCscHashMap, void *pData)
{
    ClientSideCursorHashMapData *pMapData = rs_calloc(1,sizeof(ClientSideCursorHashMapData));

    if(pMapData)
    {
        pMapData->pData = pData;

        // Put the data in front of the list
        pMapData->pNext = pCscHashMap->pDataHead;
        pCscHashMap->pDataHead = pMapData;
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release hashmap data from the list.
//
void releaseCscHashMapData(ClientSideCursorHashMap *pCscHashMap, void *pData)
{
    if(pData)
    {
        ClientSideCursorHashMapData *curr;
        ClientSideCursorHashMapData *prev;

        // Remove from hashmap data list
        curr  = pCscHashMap->pDataHead;
        prev  = NULL;

        while(curr != NULL)
        {
            if(curr->pData == pData)
            {
                if(prev == NULL)
                    pCscHashMap->pDataHead = pCscHashMap->pDataHead->pNext;
                else
                    prev->pNext = curr->pNext;

                curr->pNext = NULL;

                // Free memory
                curr = rs_free(curr);

                break;
            }

            prev = curr;
            curr = curr->pNext;
        }
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Add hashmap key and data in the list.
//
void addCscHashMapKeyAndData(ClientSideCursorShutdownHook *pCscShutdownHook, int key, void *pData)
{
    ClientSideCursorHashMap *pCscHashMap;

    // Lock the map
    rsLockSem(pCscShutdownHook->m_hHashSem);

    pCscHashMap = findCscHashMapUsingKey(pCscShutdownHook, key, FALSE);

    if(pCscHashMap == NULL)
    {
        // Add key node
        pCscHashMap = rs_calloc(1,sizeof(ClientSideCursorHashMap));

        if(pCscHashMap)
        {
            pCscHashMap->m_key = key;

            // Put the key in front of the list
            pCscHashMap->pNext = pCscShutdownHook->m_unclosedCscConnectionMap;
            pCscShutdownHook->m_unclosedCscConnectionMap = pCscHashMap;
        }
    }

    if(pCscHashMap)
    {
        // Add the data node
        addCscHashMapData(pCscHashMap, pData);
    }

    // Unlock the map
    rsUnlockSem(pCscShutdownHook->m_hHashSem);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release hashmap data from the list.
//
void releaseCscHashMapKeyAndData(ClientSideCursorShutdownHook *pCscShutdownHook, int key, void *pData,int iCalledFromShutdownHook)
{
    ClientSideCursorHashMap *pCscHashMap;

    if(!iCalledFromShutdownHook)
    {
        // Lock the map
        rsLockSem(pCscShutdownHook->m_hHashSem);
    }

    if(pData)
    {
        pCscHashMap = findCscHashMapUsingKey(pCscShutdownHook, key, FALSE);

        if(pCscHashMap)
        {
            // Release the data node
            releaseCscHashMapData(pCscHashMap, pData);

            if(pCscHashMap->pDataHead == NULL)
            {
                // Remove the key node, if all csc get closed.
                pCscHashMap = findCscHashMapUsingKey(pCscShutdownHook, key, TRUE);

                pCscHashMap = rs_free(pCscHashMap);
            }
        }
    }

    if(!iCalledFromShutdownHook)
    {
        // Unlock the map
        rsUnlockSem(pCscShutdownHook->m_hHashSem);
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Find the map data related to given key.
//
ClientSideCursorHashMap *findCscHashMapUsingKey(ClientSideCursorShutdownHook *pCscShutdownHook, int key,int removeOpertaion)
{
    ClientSideCursorHashMap *prev = NULL;
    ClientSideCursorHashMap *pCscHashMap;

    // Find from key list
    pCscHashMap  = pCscShutdownHook->m_unclosedCscConnectionMap;

    while(pCscHashMap != NULL)
    {
        if(pCscHashMap->m_key == key)
            break;

        prev = pCscHashMap;
        pCscHashMap = pCscHashMap->pNext;
    }

    if(removeOpertaion && pCscHashMap)
    {
        // Remove from the list
        if(prev == NULL)
            pCscShutdownHook->m_unclosedCscConnectionMap = pCscHashMap->pNext;
        else
            prev->pNext = pCscHashMap->pNext;

        pCscHashMap->pNext = NULL;
    }

    return pCscHashMap;
}


