/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#pragma once

#ifdef WIN32
#include <windows.h>
#endif

#include <stdlib.h>

#ifdef __cplusplus
extern "C" 
{
#endif /* C++ */

void *rs_malloc(size_t size);
void *rs_calloc(size_t NumOfElements, size_t SizeOfElements);
void *rs_realloc(void *memory, size_t newSize);
void *rs_free(void * block);

#ifdef __cplusplus
}
#endif /* C++ */
