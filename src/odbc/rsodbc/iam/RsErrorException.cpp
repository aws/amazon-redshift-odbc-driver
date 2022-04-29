#include "RsErrorException.h"


 RsErrorException::RsErrorException(
      int in_stateKey,
      int in_componentId,
      const rs_string& in_msgKey,
      const std::vector<rs_string>& in_msgParams)
 {
   char *key = (char *)in_msgKey.c_str();
   rs_string paramStr = in_msgParams[0];
   char *param=(char *)paramStr.c_str();

   snprintf(m_errorMessage,sizeof(m_errorMessage), "[%d:%d:%s]: %s\n",in_componentId, in_stateKey, key,param);
 }

 RsErrorException::RsErrorException(
      int in_stateKey,
      int in_componentId,
      const rs_string& in_msgKey,
      const std::vector<rs_wstring>& in_msgParams)
 {
   char *key = (char *)in_msgKey.c_str();
   rs_wstring paramWstr = in_msgParams[0];
   wchar_t *param=(wchar_t *)paramWstr.c_str();

   snprintf(m_errorMessage, sizeof(m_errorMessage), "[%d:%d:%s]: %S\n",in_componentId, in_stateKey, key,(wchar_t *)param);
 }

 char *RsErrorException::getErrorMessage()
 {
   return (char *)m_errorMessage;
 }

