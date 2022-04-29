#include "RsIamEntry.h"
#include "RsIamHelper.h"

void RsIamEntry::IamAuthentication(
                  bool isIAMAuth,
                  RS_IAM_CONN_PROPS_INFO *pIamProps,
                  RS_PROXY_CONN_PROPS_INFO *pHttpsProps,
                  RsSettings& settings,
                  RsLogger *logger)
{
  RsIamHelper::IamAuthentication(isIAMAuth, pIamProps, pHttpsProps, settings, logger);
}

/*================================================================================================*/

char * RsIamEntry::ReadAuthProfile(
	bool isIAMAuth,
	RS_IAM_CONN_PROPS_INFO *pIamProps,
	RS_PROXY_CONN_PROPS_INFO *pHttpsProps,
	RsLogger *logger)
{
	return RsIamHelper::ReadAuthProfile(isIAMAuth, pIamProps, pHttpsProps, logger);
}
