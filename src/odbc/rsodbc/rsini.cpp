
#if defined LINUX || defined SunOS || defined AIX

#include "rsini.h"

#if defined LINUX
#include "linux_port.h"
#endif
#if defined SunOS
#include "solaris_port.h"
#endif

#if defined AIX
#include "aix_port.h"
#endif

/*====================================================================================================================================================*/

int RsIni::getPrivateProfileInt(char *pSectionName, char *pKey, int iDflt, char *pFile)
{
   FILE *fp = NULL;
   char buffer[INI_BUFFER_SIZE];
   char search[INI_KEY_SIZE];
   char *pChar = NULL;
   int iStrCode = -1;
   char pFullFilePath[MAX_PATH + 1];

   if(pFile == NULL || pSectionName == NULL || pKey == NULL)
	   return iDflt;

   getFullOdbcIniPath(pFullFilePath, MAX_PATH, pFile);

   fp = fopen(pFullFilePath, "r");

   if(fp == NULL)
	   return(iDflt);

   snprintf(search, INI_KEY_SIZE, "[%s]", pSectionName);

    while(fgets(buffer, INI_BUFFER_SIZE, fp) != 0)
    {
        if (buffer[strlen(buffer)-1] == '\n')
            buffer[strlen(buffer)-1]='\0';

         if (isCommentLine(buffer))
        	 continue;

        if ((iStrCode = _stricmp(search, buffer)) == 0)
        	break;
    } // Loop

    if(iStrCode == 0)
    {
        iStrCode = -1;
        strncpy(search, pKey, INI_KEY_SIZE);
        while (fgets(buffer, INI_BUFFER_SIZE, fp) != 0)
        {
            if (buffer[strlen(buffer)-1] == '\n')
                buffer[strlen(buffer)-1]='\0';

            if (isCommentLine(buffer))
            	continue;

            if (isSection(buffer))
            	break;

            if((pChar = strchr(buffer, '=')) == 0)
            	continue;
            *pChar++=0;
            if ((iStrCode = _stricmp(search, buffer)) == 0)
            	break;
         } // Loop

      if(iStrCode == 0)
      {
           fclose(fp);
           if (pChar == NULL || pChar[0] == '\0')
               return (iDflt);
           else
             return(atoi(pChar));
      } // Key found
    } // Section found

   fclose(fp);
   return(iDflt);
 }

/*====================================================================================================================================================*/

int RsIni::getPrivateProfileString(const char *pSectionName, char *pKey, char *pDflt, char *pReturn, int iSize, char *pFile)
{
   char pFullFilePath[MAX_PATH + 1];

   if(pFile == NULL || pSectionName == NULL)
   {
      strncpy(pReturn, pDflt, iSize);
      return((iSize <= strlen(pDflt)) ? iSize : strlen(pDflt));
   }

   getFullOdbcIniPath(pFullFilePath, MAX_PATH, pFile);

   return getPrivateProfileStringWithFullPath(pSectionName, pKey, pDflt, pReturn, iSize, pFullFilePath);
}

/*====================================================================================================================================================*/

int RsIni::getPrivateProfileStringWithFullPath(const char *pSectionName, const char *pKey, const char *pDflt, char *pReturn, int iSize, char *pFullFilePath)
{
	FILE *fp;
	char buffer[INI_BUFFER_SIZE];
	char search[INI_KEY_SIZE];
	char *pChar = NULL;
	int 	iStrCode = -1;
	int  iSizeLeft = 0;
	char *pNextWrite;

	fp = fopen(pFullFilePath, "r");

	if (fp == NULL)
	{
		strncpy(pReturn, pDflt, iSize);
		return((iSize <= strlen(pDflt)) ? iSize : strlen(pDflt));
	}

	iSizeLeft = iSize - 1;
	pReturn[0] = '\0';
	pNextWrite = pReturn;

	snprintf(search, INI_KEY_SIZE, "[%s]", pSectionName);

	while (fgets(buffer, INI_BUFFER_SIZE, fp) != 0)
	{
		if (buffer[strlen(buffer) - 1] == '\n')
			buffer[strlen(buffer) - 1] = '\0';

		if (isCommentLine(buffer))
			continue;

		if ((iStrCode = _stricmp(search, buffer)) == 0)
			break;
	} // Loop to find section

	if (iStrCode == 0)
	{
		iStrCode = -1;
		if (pKey != NULL)
			strncpy(search, pKey, INI_KEY_SIZE);

		while (fgets(buffer, INI_BUFFER_SIZE, fp) != 0)
		{
			if (buffer[strlen(buffer) - 1] == '\n')
				buffer[strlen(buffer) - 1] = '\0';

			if (isCommentLine(buffer))
				continue;

			if (isSection(buffer))
				break;

			if (pKey == NULL)
			{
				strncpy(pNextWrite, buffer, iSizeLeft);
				iSizeLeft = iSizeLeft - strlen(buffer) - 1;
				if (iSizeLeft <= 1)
				{
					pNextWrite += strlen(buffer) + 1;
					*pNextWrite = '\0';

					pReturn[iSize - 1] = '\0';
					iSizeLeft = 0;
					break;
				}
				else
				{
					pNextWrite += strlen(buffer) + 1;
					*pNextWrite = '\0';
				}
			}
			else
			{
				if ((pChar = strchr(buffer, '=')) == 0)
					continue;

				*pChar++ = '\0';
				if ((iStrCode = _stricmp(search, trim_whitespaces(buffer))) == 0)
					break;
			}
		} // Loop for key

		if (iStrCode == 0)
		{
			fclose(fp);
			if (pChar)
				strncpy(pReturn, trim_whitespaces(pChar), iSize);
			return((iSize <= strlen(pChar)) ? iSize : strlen(pChar));
		} // Key found
	} // Section found

	fclose(fp);
	if (pKey == NULL)
		return(iSize - iSizeLeft);

	strncpy(pReturn, pDflt, iSize);

	return((iSize <= strlen(pDflt)) ? iSize : strlen(pDflt));
}

/*====================================================================================================================================================*/

int RsIni::isCommentLine(char *pBuffer)
{
    char   *pTempChar = pBuffer;

    for (;pTempChar && *pTempChar==' ';pTempChar++);

    if (pTempChar && *pTempChar == ';')
       return 1;
    else if (pTempChar && *pTempChar == '\0')
		return 1;
	else
       return 0;
}

/*====================================================================================================================================================*/

int RsIni::isSection(char *pBuffer)
{
    char   *pTempChar = pBuffer;

    for (;pTempChar && *pTempChar==' ';pTempChar++);

    if (pTempChar && *pTempChar == '[')
    {
       pTempChar++;

       /* search for  ']' */
       for (;pTempChar && *pTempChar != ']';pTempChar++);
       if (pTempChar)
          return 1;
       else
          return 0;
    }
    else
       return 0;
}

/*====================================================================================================================================================*/

int RsIni::writePrivateProfileString(char *pSectionName, char *pKey, char *pData, char *pFile)
{
   FILE *fp;
   char buffer[INI_BUFFER_SIZE];
   char search[INI_KEY_SIZE];
   char original[INI_BUFFER_SIZE];
   char *pChar = NULL;
   int iStrCode = -1;
   size_t iFileLength;
   size_t iBufferLength;
   char * pFileBuffer;
   char pFullFilePath[MAX_PATH + 1];
   int iTempLen;

   if(pFile == NULL || pSectionName == NULL || pKey == NULL)
   {
	   return 0;
   }

   getFullOdbcIniPath(pFullFilePath, MAX_PATH, pFile);

   fp = fopen(pFullFilePath, "r");
   if(fp == NULL)
   {
		fp = fopen(pFullFilePath, "w");
		if (fp == NULL)
			return 0;
		fclose(fp);
        iFileLength = 0;
   }
   else
   {
      fseek(fp, 0, SEEK_END );
      iFileLength = ftell(fp);
      fclose(fp);
   }

   iTempLen = iBufferLength = iFileLength +  strlen(pKey) + 1 + ((pData) ? strlen(pData) : 0) + 2 + strlen(pSectionName) + 4;
   pFileBuffer = (char *) malloc(iBufferLength);
   pFileBuffer[0] = '\0';

   fp = fopen(pFullFilePath, "r");
   snprintf(search, INI_KEY_SIZE, "[%s]", pSectionName);

   while(fgets(buffer, INI_BUFFER_SIZE, fp) != 0)
   {
	  strncat(pFileBuffer, buffer, iTempLen);

	  iTempLen -= strlen(buffer);

	  if(buffer[strlen(buffer)-1] == '\n')
          buffer[strlen(buffer)-1]= '\0';

      if(isCommentLine(buffer))
    	  continue;

	  if ((iStrCode = _stricmp(search, buffer)) == 0)
		  break;
   } // Loop

   if (iStrCode == 0)
   {
      iStrCode = -1;
      strncpy(search, pKey, INI_KEY_SIZE);
      while(fgets(buffer, INI_BUFFER_SIZE, fp) != 0)
      {
		 original[0] = '\0';

         if(isCommentLine(buffer))
         {
			  strncat( pFileBuffer, buffer, INI_BUFFER_SIZE);
			  continue;
		 }

		 strncpy(original, buffer, INI_BUFFER_SIZE);

         if (isSection(buffer))
        	 break;

         if (buffer[strlen(buffer)-1] == '\n')
            buffer[strlen(buffer)-1]=0;

         if ((pChar = strchr(buffer, '=')) != NULL)
			 *pChar=0;

         if ((iStrCode = _stricmp(search, buffer)) == 0)
         {
			 original[0] = '\0';
			 break;
		 }

		 strncat(pFileBuffer, original, INI_BUFFER_SIZE);
		 original[0] = '\0';
      } // Loop

      if(pData)
      {
		  snprintf(buffer, INI_BUFFER_SIZE, "%s=%s\n", pKey, pData );
		  strncat(pFileBuffer, buffer, INI_BUFFER_SIZE);
      }

	  if(original[0] != '\0')
		  strncat( pFileBuffer, original, INI_BUFFER_SIZE);
   }
   else
   {
		snprintf(buffer, INI_BUFFER_SIZE, "[%s]\n", pSectionName);
		strncat(pFileBuffer, buffer, INI_BUFFER_SIZE);

		if(pData)
		{
			snprintf(buffer, INI_BUFFER_SIZE, "%s=%s\n", pKey, pData);
			strncat(pFileBuffer, buffer, INI_BUFFER_SIZE);
		}
   }

   while (fgets(buffer, INI_BUFFER_SIZE, fp) != 0)
		strncat(pFileBuffer, buffer, INI_BUFFER_SIZE);

   fclose(fp);

   fp = fopen(pFullFilePath, "w");
   if(fp == NULL)
   {
	   free(pFileBuffer);
	   return 0;
   }

   iFileLength = strlen(pFileBuffer);
   fputs(pFileBuffer, fp);
   fclose(fp);
   free(pFileBuffer);

   return 1;
}

/*====================================================================================================================================================*/

void RsIni::getFullOdbcIniPath(char *pFullFilePath, int iMaxsize, char *pFile)
{
	char *pOdbcIniEnv = getenv(ODBCINI_ENV_VAR);
	int  iSize = ((pOdbcIniEnv) ? strlen(pOdbcIniEnv) : 0 ) + 1 + strlen(pFile);

	if(iSize < iMaxsize)
	{
		if(pOdbcIniEnv)
		{
			strncpy(pFullFilePath,pOdbcIniEnv, iMaxsize);
		}
		else
			strncpy(pFullFilePath,pFile, iMaxsize);
	}
	else
		pFullFilePath[0] = '\0';
}

#endif // LINUX or SunOS or AIX







