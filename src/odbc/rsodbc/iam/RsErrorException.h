#ifndef _RS_ERROR_EXCEPTION_H_
#define _RS_ERROR_EXCEPTION_H_

#include "rs_string.h"
#include "rs_wstring.h"

#include <vector>
#define MAX_IAM_ERROR_MSG_LEN 1024

class RsErrorException {

private:
  char m_errorMessage[MAX_IAM_ERROR_MSG_LEN];

public :
  RsErrorException(
      int in_stateKey,
      int in_componentId,
      const rs_string& in_msgKey,
      const std::vector<rs_string>& in_msgParams);

  RsErrorException(
      int in_stateKey,
      int in_componentId,
      const rs_string& in_msgKey,
      const std::vector<rs_wstring>& in_msgParams);

  char *getErrorMessage();

};

#endif

