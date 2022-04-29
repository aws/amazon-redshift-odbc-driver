/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

// This file is not compile with included file.h.
#include<sys/stat.h>

int fileExists(char * pFileName);

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Check for file existance
//
int fileExists(char * pFileName)
{   
    struct stat buf;   
    int i = (pFileName) ? stat(pFileName, &buf ) : 1;     
    
    /* File found */     
    return ( i == 0 );
}
