#include "IdpTokenAuthPlugin.h"

using namespace Redshift::IamSupport;

IdpTokenAuthPlugin::IdpTokenAuthPlugin(
    RsLogger *in_log, const IAMConfiguration &in_config,
    const std::map<rs_string, rs_string> &in_argsMap)
    : NativePluginCredentialsProvider(in_log, in_config, in_argsMap) {
    RS_LOG(m_log)("IdpTokenAuthPlugin::IdpTokenAuthPlugin");
    InitArgumentsMap();
}

IdpTokenAuthPlugin::~IdpTokenAuthPlugin() {
    RS_LOG(m_log)("IdpTokenAuthPlugin::~IdpTokenAuthPlugin");
}

rs_string IdpTokenAuthPlugin::GetAuthToken() {
    RS_LOG(m_log)("IdpTokenAuthPlugin::GetAuthToken");
    ValidateArgumentsMap();
    return m_argsMap[KEY_IDP_AUTH_TOKEN];
}

void IdpTokenAuthPlugin::InitArgumentsMap() {
    RS_LOG(m_log)("IdpTokenAuthPlugin::InitArgumentsMap");

    const rs_string authToken = m_config.GetIdpAuthToken();
    const rs_string tokenType = m_config.GetIdpAuthTokenType();
    if (!IAMUtils::rs_trim(authToken).empty()) {
        m_argsMap[KEY_IDP_AUTH_TOKEN] = authToken;
    }
    if (!IAMUtils::rs_trim(tokenType).empty()) {
        m_argsMap[KEY_IDP_AUTH_TOKEN_TYPE] = tokenType;
        RS_LOG(m_log)("Setting token_type=%s", tokenType.c_str());
    }
}

void IdpTokenAuthPlugin::ValidateArgumentsMap() {
    RS_LOG(m_log)("IdpTokenAuthPlugin::ValidateArgumentsMap");

    // Validate the parameters passed in and make sure we have the required auth
    // token and token type
    if (!m_argsMap.count(KEY_IDP_AUTH_TOKEN)) {
        RS_LOG(m_log)
        ("IdC authentication failed: token needs to be provided in connection "
         "params");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed: The token must be included in the "
            "connection parameters.");
    }
    if (!m_argsMap.count(KEY_IDP_AUTH_TOKEN_TYPE)) {
        RS_LOG(m_log)
        ("IdC authentication failed: token type needs to be provided in "
         "connection params");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed: The token type must be included in the "
            "connection parameters.");
    }
}