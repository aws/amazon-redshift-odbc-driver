#pragma once

#if defined LINUX 

#ifndef __RS_INI_H__

#define __RS_INI_H__


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "rsodbc.h"
#include "rsutil.h"
#include "rsunicode.h"
#include "rstrace.h"

#define INI_BUFFER_SIZE   (16 * 1024) // Increase to 16k from 1K because of JWT
#define INI_KEY_SIZE      128

class RsIni {
  public:
    static int isCommentLine(char *pBuffer);
    static int isSection(char *pBuffer);

    static int getPrivateProfileInt(char *pSectionName, char *pKey, int iDflt, char *pFile);
    static int getPrivateProfileString(char *pSectionName, char *pKey, char *pDflt, char *pReturn, int iSize, char *pFile);
    static int writePrivateProfileString(char *pSectionName, char *pKey, char *pData, char *pFile);
    static void getFullOdbcIniPath(char *pFullFilePath, int iMaxsize, char *pFile);
	static int getPrivateProfileStringWithFullPath(char *pSectionName, char *pKey, char *pDflt, char *pReturn, int iSize, char *pFullFilePath);

};

#endif // __RS_INI_H__


#endif // LINUX 

