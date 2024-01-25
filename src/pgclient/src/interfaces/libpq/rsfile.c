/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include <time.h>
#if defined LINUX 
#if defined LINUX32 
#include <stdio.h>
#endif
#endif
#include "rsfile.h"

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Open file
//
FILE *rs_fopen(const char * _Filename, const char * _Mode)
{
    return fopen(_Filename, _Mode);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get file descriptor
//
int rs_fileno(FILE *_file)
{
    int fd;

    if(_file)
    {
#ifdef WIN32
        fd = _fileno(_file);
#endif
#if defined LINUX 
        fd = fileno(_file);
#endif
    }
    else
        fd = 0;

    return fd;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// synchronize  a  file in-core state with storage  device
//
int rs_fsync(int fd)
{
    int rc;

#ifdef WIN32
    // We open the file with "c" option.
    rc = 0;
#endif

#if defined LINUX 
    if(fd)
    {
        rc = fsync(fd);
    }
    else
        rc = 0;
#endif

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Return current time as millisec
//
long long getCurrentTimeInMilli(void)
{
    time_t ltime = 0;

    time(&ltime);

    return (long long)(ltime * 1000);
}


