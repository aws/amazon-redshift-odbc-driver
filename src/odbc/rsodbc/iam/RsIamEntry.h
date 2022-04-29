#ifndef _RS_IAM_ENTRY_H_
#define _RS_IAM_ENTRY_H_

#include "RsSettings.h"
#include "RsLogger.h"
#include "../rsiam.h"


class RsIamEntry {

public:

    // Entry point function from ODBC connection call for IAM
    static void IamAuthentication(bool isIAMAuth,
                                    RS_IAM_CONN_PROPS_INFO *pIamProps,
                                    RS_PROXY_CONN_PROPS_INFO *pHttpsProps,
                                    RsSettings& settings, RsLogger *logger);

	static char *ReadAuthProfile(
		bool isIAMAuth,
		RS_IAM_CONN_PROPS_INFO *pIamProps,
		RS_PROXY_CONN_PROPS_INFO *pHttpsProps,
		RsLogger *logger);

};


#endif
