#include "RsIamEntry.h"
#include "RsIamHelper.h"

void RsIamEntry::IamAuthentication(
                  bool isIAMAuth,
                  RS_IAM_CONN_PROPS_INFO *pIamProps,
                  RS_PROXY_CONN_PROPS_INFO *pHttpsProps,
                  RsSettings& settings)
{
  RsIamHelper::IamAuthentication(isIAMAuth, pIamProps, pHttpsProps, settings);
}

/*================================================================================================*/

void RsIamEntry::NativePluginAuthentication(
    bool isIAMAuth,
    RS_IAM_CONN_PROPS_INFO *pIamProps,
    RS_PROXY_CONN_PROPS_INFO *pHttpsProps,
    RsSettings& settings)
{
    RsIamHelper::NativePluginAuthentication(isIAMAuth, pIamProps, pHttpsProps, settings);
}

/*================================================================================================*/

char * RsIamEntry::ReadAuthProfile(
	bool isIAMAuth,
	RS_IAM_CONN_PROPS_INFO *pIamProps,
	RS_PROXY_CONN_PROPS_INFO *pHttpsProps)
{
	return RsIamHelper::ReadAuthProfile(isIAMAuth, pIamProps, pHttpsProps);
}
