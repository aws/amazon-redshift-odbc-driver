/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "rslock.h"
#include "rsmem.h"

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Create a OS thread.
//
THREAD_HANDLE rsCreateThread(void *pThreadProc,void *pArg)
{
    THREAD_HANDLE hThread = (THREAD_HANDLE)(long)NULL;

#ifdef WIN32
    hThread = (THREAD_HANDLE)_beginthread((void( __cdecl *)( void * ))pThreadProc, 0, pArg);
#endif
#if defined LINUX 
    pthread_create(&hThread, NULL, (void *(*)( void * ))pThreadProc, pArg);
#endif

    return hThread;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// 1 means join the thread successfully, 0 means error joining the thread.
//
int rsJoinThread(THREAD_HANDLE hThread)
{
    int rc;

#ifdef WIN32
    rc = WaitForSingleObject(hThread, INFINITE);  

    if(rc == WAIT_OBJECT_0) 
        rc = TRUE;
    else
        rc = FALSE;
#endif

#if defined LINUX 
    rc = pthread_join(hThread, NULL);
    if(rc == 0)
        rc = TRUE;
    else
        rc = FALSE;
#endif

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get current thread id.
//
RS_DWORD rsGetCurrentThreadId(void)
{
    RS_DWORD  lThreadId = 0;
    
#ifdef WIN32
    lThreadId = GetCurrentThreadId();
#endif

#if defined LINUX 
    lThreadId = pthread_self();
#endif

    return lThreadId;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get current process id.
//
RS_DWORD rsGetCurrentProcessId(void)
{
    RS_DWORD  lProcessId = 0;
    
#ifdef WIN32
    lProcessId = GetCurrentProcessId();
#endif

#if defined LINUX 
    lProcessId = getpid();
#endif

    return lProcessId;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Terminate the thread.
//
void rsKillThread(THREAD_HANDLE hThread)
{
#ifdef WIN32
    TerminateThread(hThread, 0);
#endif
#if defined LINUX 
    pthread_cancel(hThread);
#endif
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release resources related to thread.
//
void rsEndThread(void)
{
#ifdef WIN32
    _endthread();
#endif
#if defined LINUX 
    // No need for Linux.
#endif
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Close the thread handle.
//
THREAD_HANDLE rsCloseThreadHandle(THREAD_HANDLE hThread)
{
#ifdef WIN32
    if(hThread)
    {
        CloseHandle(hThread);
    }
#endif

    return (THREAD_HANDLE)(long)NULL;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Create a mutex.
//
MUTEX_HANDLE rsCreateMutex(void)
{
    MUTEX_HANDLE hMutex;

#ifdef WIN32
    hMutex = CreateMutex(NULL, FALSE, NULL); // Get the mutex as non-signal state.
#endif
#if defined LINUX 
    hMutex = rs_calloc(1,sizeof(pthread_mutex_t));
    pthread_mutex_init(hMutex, NULL);
//    pthread_mutexattr_settype(hMutex, PTHREAD_MUTEX_RECURSIVE);
#endif

    return hMutex;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// 1 means get the mutex lock, 0 means error geting mutex lock.
//
int rsLockMutex(MUTEX_HANDLE hMutex)
{
    int rc;

#ifdef WIN32
    if(hMutex)
    {
        rc = WaitForSingleObject(hMutex, INFINITE);  

        if(rc == WAIT_OBJECT_0) 
            rc = TRUE;
        else
            rc = FALSE;
    }
    else
        rc = FALSE;
#endif

#if defined LINUX 
    if(hMutex)
    {
        rc = pthread_mutex_lock(hMutex);
        rc = (rc == 0) ? TRUE : FALSE;
    }
    else
        rc = FALSE;
#endif

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// 1 means unlock mutex successful, 0 means error releasing mutex lock.
//
int rsUnlockMutex(MUTEX_HANDLE hMutex)
{
    int rc;

#ifdef WIN32
    if(hMutex)
    {
        rc = ReleaseMutex(hMutex);
    }
    else
        rc = 1;
#endif

#if defined LINUX 
    if(hMutex)
    {
        rc = pthread_mutex_unlock(hMutex);
        rc = (rc == 0) ? TRUE : FALSE;
    }
    else
        rc = 1;
#endif

    return (rc != 0);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// 1 means destroy mutex successfully, 0 means error.
//
int rsDestroyMutex(MUTEX_HANDLE hMutex)
{
    int rc;

#ifdef WIN32
    if(hMutex)
    {
        rc = CloseHandle(hMutex);
    }
    else
        rc = 1;
#endif

#if defined LINUX 
    if(hMutex)
    {
        rc = pthread_mutex_destroy(hMutex);
        rc = (rc == 0) ? TRUE : FALSE;
        hMutex = rs_free(hMutex);
    }
    else
        rc = 1;
#endif

    return (rc != 0);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Creat a binary semaphore.
//
SEM_HANDLE rsCreateBinarySem(int initialState)
{
    SEM_HANDLE hSem;

#ifdef WIN32
    hSem = CreateSemaphore(NULL, initialState,1, NULL); // Get the sem in signal/non-signal state.
#endif
#if defined LINUX 
    int rc;
    hSem = rs_calloc(1,sizeof(sem_t));
    rc = sem_init(hSem, 0, initialState);
    if(rc != 0)
    {
        // Error
    }
#endif

    return hSem;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// 1 means get the semaphore lock, 0 means error geting semaphore lock.
//
int rsLockSem(SEM_HANDLE hSem)
{
    int rc;

#ifdef WIN32
    if(hSem)
    {
        rc = WaitForSingleObject(hSem, INFINITE);  

        if(rc == WAIT_OBJECT_0) 
            rc = TRUE;
        else
            rc = FALSE;
    }
    else
        rc = FALSE;
#endif

#if defined LINUX 
    if(hSem)
    {
        rc = sem_wait(hSem);
        rc = (rc == 0) ? TRUE : FALSE;
    }
    else
        rc = FALSE;
#endif

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// 1 means unlock semaphore successfully, 0 means error releasing semaphore lock.
//
int rsUnlockSem(SEM_HANDLE hSem)
{
    int rc;

#ifdef WIN32
    if(hSem)
    {
        rc = ReleaseSemaphore(hSem,1,NULL);
    }
    else
        rc = 1;
#endif

#if defined LINUX 
    if(hSem)
    {
        rc = sem_post(hSem);
        rc = (rc == 0) ? TRUE : FALSE;
    }
    else
        rc = 1;
#endif

    return (rc != 0);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// 1 means destroy semaphore successfully, 0 means error.
//
int rsDestroySem(SEM_HANDLE hSem)
{
    int rc;

#ifdef WIN32
    if(hSem)
    {
        rc = CloseHandle(hSem);
    }
    else
        rc = 1;
#endif

#if defined LINUX 
    if(hSem)
    {
        rc = sem_destroy(hSem);
        rc = (rc == 0) ? TRUE : FALSE;
        hSem = rs_free(hSem);
    }
    else
        rc = 1;
#endif

    return (rc != 0);
}
