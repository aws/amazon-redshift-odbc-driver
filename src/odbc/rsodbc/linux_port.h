/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#ifdef LINUX
#pragma once

#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#include "csc_linux_port.h"

#define DRIVER_SO_NAME "librsodbc64.so"

typedef SQLLEN * SQLLEN_POINTER; // void *

#endif // LINUX




