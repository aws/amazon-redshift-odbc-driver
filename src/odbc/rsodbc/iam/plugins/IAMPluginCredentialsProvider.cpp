#include "IAMPluginCredentialsProvider.h"
#include "IAMUtils.h"
#include "IAMWinHttpClientDelegate.h"
#include "IAMCurlHttpClient.h"

#include <regex>
#include <unordered_set>
#include <aws/sts/STSClient.h>
#include <aws/core/http/HttpClientFactory.h>
#include <aws/core/utils/StringUtils.h>

/* Json::JsonValue class contains a member function: GetObject. There is a predefined
MACRO GetObject in wingdi.h that will cause the conflict. We need to undef GetObject
in order to use the GetObject memeber function from Json::JsonValue */
#ifdef GetObject
    #undef GetObject
#endif

#include <aws/core/utils/json/JsonSerializer.h>

using namespace Redshift::IamSupport;
using namespace Aws::Auth;
using namespace Aws::Client;
using namespace Aws::STS;
using namespace Aws::Utils;
using namespace Aws::Http;

namespace 
{
    static const char* LOG_TAG = "IAMPluginCredentialsProvider";

    /* Regex pattern used to extract input tag from html body */
    const rs_string INPUT_TAG_PATTERN = R"(<input([^>]*)\/?>)";

    /* Regex pattern used to extra form action from html body */
    const rs_string FORM_ACTION_PATTERN = "<form.*?action=\"([^\"]+)\"";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMPluginCredentialsProvider::IAMPluginCredentialsProvider(
    RsLogger* in_log,
    const IAMConfiguration& in_config,
    const std::map<rs_string, rs_string>& in_argsMap) :
    IAMCredentialsProvider(in_log, in_config),
    m_argsMap(in_argsMap)
{
    RS_LOG(m_log)("IAMPluginCredentialsProvider::IAMPluginCredentialsProvider");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMPluginCredentialsProvider::InitArgumentsMap()
{
    RS_LOG(m_log)("IAMPluginCredentialsProvider::InitArgumentsMap");

    /* Precedence of connection attributes: Connection String > Profile > SAML Assertion
    Set these connection attributes if they're already being set in the connection string */

    const rs_string dbUser   = m_config.GetDbUser();
    const rs_string dbGroup  = m_config.GetDbGroups();
    bool forceLowercase = m_config.GetForceLowercase();
    bool userAutoCreate         = m_config.GetAutoCreate();

    /* Plugin and profile related settings */
    const rs_string user          = m_config.GetUser();
    const rs_string password      = m_config.GetPassword();
    const rs_string idpHost       = m_config.GetIdpHost();
    short idpPort             = m_config.GetIdpPort();
    const rs_string idpTenant     = m_config.GetIdpTenant();
    const rs_string clientSecret  = m_config.GetClientSecret();
    const rs_string clientId      = m_config.GetClientId();
    short idpResponseTimeout  = m_config.GetIdpResponseTimeout();
    short listen_port         = m_config.GetListenPort();
    const rs_string login_url     = m_config.GetLoginURL();
    const rs_string preferredRole = m_config.GetPreferredRole();
    bool sslInsecure                 = m_config.GetSslInsecure();
    const rs_string role_arn = m_config.GetRoleARN();
    short duration = m_config.GetDuration();
    const rs_string web_identity_token = m_config.GetWebIdentityToken();
    const rs_string role_session_name = m_config.GetRoleSessionName();
	const rs_string scope = m_config.GetScope();

    
    /* Get regular expression to filter received dbGroups from SAML response */
    m_dbGroupsFilter = m_config.GetDbGroupsFilter();

    if (!dbUser.empty())
    {
        m_argsMap[IAM_KEY_DBUSER] = dbUser;
    }

    if (!dbGroup.empty())
    {
        m_argsMap[IAM_KEY_DBGROUPS] = dbGroup;
    }

    if (forceLowercase)
    {
        m_argsMap[IAM_KEY_FORCELOWERCASE] = "1";
    }

    if (userAutoCreate)
    {
        m_argsMap[IAM_KEY_AUTOCREATE] = "1";
    }

    if (!user.empty())
    {
        m_argsMap[IAM_KEY_USER] = user;
    }

    if (!password.empty())
    {
        m_argsMap[IAM_KEY_PASSWORD] = password;
    }

    if (!idpHost.empty())
    {
        m_argsMap[IAM_KEY_IDP_HOST] = idpHost;
    }

    if (idpPort != 0)
    {
        m_argsMap[IAM_KEY_IDP_PORT] = std::to_string(idpPort);
    }

    if (!idpTenant.empty())
    {
        m_argsMap[IAM_KEY_IDP_TENANT] = idpTenant;
    }

    if (!clientSecret.empty())
    {
        m_argsMap[IAM_KEY_CLIENT_SECRET] = clientSecret;
    }

    if (!clientId.empty())
    {
        m_argsMap[IAM_KEY_CLIENT_ID] = clientId;
    }

    if (idpResponseTimeout != 0)
    {
        m_argsMap[IAM_KEY_IDP_RESPONSE_TIMEOUT] = std::to_string(idpResponseTimeout);
    }

    if (listen_port != 0)
    {
        m_argsMap[IAM_KEY_LISTEN_PORT] = std::to_string(listen_port);
    }

	if (!scope.empty())
	{
		m_argsMap[IAM_KEY_SCOPE] = scope;
	}


    if (!login_url.empty())
    {
        m_argsMap[IAM_KEY_LOGIN_URL] = login_url;
    }

    if (!preferredRole.empty())
    {
        m_argsMap[IAM_KEY_PREFERRED_ROLE] = preferredRole;
    }

    if (sslInsecure)
    {
        m_argsMap[IAM_KEY_SSL_INSECURE] = "1";
    }
    if (!role_arn.empty())
    {
        m_argsMap[IAM_KEY_ROLE_ARN] = role_arn;
    }

    if (!web_identity_token.empty())
    {
        m_argsMap[IAM_KEY_WEB_IDENTITY_TOKEN] = web_identity_token;
    }

    if (duration != 0)
    {
        m_argsMap[IAM_KEY_DURATION] = std::to_string(duration);
    }

    if (!role_session_name.empty())
    {
        m_argsMap[IAM_KEY_ROLE_SESSION_NAME] = role_session_name;
    }

}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMPluginCredentialsProvider::SaveSettings(const Aws::Auth::AWSCredentials& in_credentials) 
{
    RS_LOG(m_log)("IAMPluginCredentialsProvider::SaveSettings");

    m_credentials.SetAWSCredentials(in_credentials);

    if (m_argsMap.count(IAM_KEY_DBUSER))
    {
        m_credentials.SetDbUser(m_argsMap[IAM_KEY_DBUSER]);
    }
    
    if (m_argsMap.count(IAM_KEY_DBGROUPS))
    {
        m_credentials.SetDbGroups(m_argsMap[IAM_KEY_DBGROUPS]);
    }

    if (m_argsMap.count(IAM_KEY_FORCELOWERCASE) &&
        IAMUtils::ConvertStringToBool(m_argsMap[IAM_KEY_FORCELOWERCASE]))
    {
        m_credentials.SetForceLowercase(true);
    }

    if (m_argsMap.count(IAM_KEY_AUTOCREATE) && 
        IAMUtils::ConvertStringToBool(m_argsMap[IAM_KEY_AUTOCREATE]))
    {
        m_credentials.SetAutoCreate(true);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<IAMHttpClient> IAMPluginCredentialsProvider::GetHttpClient(
    const HttpClientConfig& in_config)
{
    RS_LOG(m_log)( "IAMPluginCredentialsProvider", "GetHttpClient");
#ifdef _WIN32
    return Aws::MakeShared<IAMWinHttpClientDelegate>(LOG_TAG, in_config);
#else
    return Aws::MakeShared<IAMCurlHttpClient>(LOG_TAG, in_config);
#endif 
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMPluginCredentialsProvider::SetArgumentKeyValuePair(
    const rs_string& in_key,
    const rs_string& in_value,
    const rs_string& in_defaultValue,
    bool in_urlEncoded)
{
    RS_LOG(m_log)("IAMPluginCredentialsProvider::SetArgumentKeyValuePair");

    // foreach argument: 1. Connection String > 2: AWS Profile > 3: Default Value
    if (!in_value.empty())
    {
        m_argsMap[in_key] = in_value;
    }
    else if (!m_argsMap.count(in_key) || m_argsMap[in_key].empty())
    {
        m_argsMap[in_key] = in_defaultValue;
    }
    /* Whether or not URLEncode the argument value */
    if (in_urlEncoded)
    {
        m_argsMap[in_key] = StringUtils::URLEncode(m_argsMap[in_key].c_str());
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMPluginCredentialsProvider::GetXmlAttributeValues(
    const Xml::XmlNode& in_rootNode,
    const rs_string& in_nodeName,
    const rs_string& in_attrKey,
    const rs_string& in_attrVal,
    std::vector<rs_string>& out_attrValues,
    bool in_ignoreNodeNamespace)
{
    /* In-order traversal of the XmlNode */

    if (const_cast<Xml::XmlNode&>(in_rootNode).IsNull())
    {
        return;
    }

    rs_string nodeName = in_rootNode.GetName();

    if (in_ignoreNodeNamespace)
    {
        size_t index = nodeName.rfind(':');
        if (index != rs_string::npos)
        {
            nodeName.erase(0, index + 1);
        }
    }

    if (nodeName == in_nodeName &&
        in_rootNode.GetAttributeValue(in_attrKey) == in_attrVal)
    {
        if (!in_rootNode.HasChildren())
        {
            /* no role-arn and role-principal information, return */
            return;
        }

        Xml::XmlNode node = in_rootNode.FirstChild();
        while (!node.IsNull())
        {
            out_attrValues.push_back(node.GetText());
            if (!node.HasNextNode())
            {
                break;
            }
            node = node.NextNode();
        }
    }
    if (in_rootNode.HasChildren())
    {
        GetXmlAttributeValues(
            in_rootNode.FirstChild(),
            in_nodeName,
            in_attrKey, 
            in_attrVal, 
            out_attrValues,
            in_ignoreNodeNamespace);
    }

    if (in_rootNode.HasNextNode())
    {
        GetXmlAttributeValues(
            in_rootNode.NextNode(), 
            in_nodeName, 
            in_attrKey,
            in_attrVal, 
            out_attrValues,
            in_ignoreNodeNamespace);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::vector<rs_string> IAMPluginCredentialsProvider::GetInputTagsFromHtml(
    const rs_string& in_htmlBody)
{
    RS_LOG(m_log)("IAMPluginCredentialsProvider::GetInputTagsFromHtml");
    std::vector<rs_string> inputTags;

    std::smatch match;
    std::regex expression(INPUT_TAG_PATTERN, std::regex::optimize);
 
    rs_string htmlBody = in_htmlBody;

    while (std::regex_search(htmlBody, match, expression))
    {
        inputTags.push_back(match.str(1));
        htmlBody = match.suffix().str();
    }
    return inputTags;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::map<rs_string, rs_string> IAMPluginCredentialsProvider::GetNameValuePairFromInputTag(
    const std::vector<rs_string>& in_inputTags)
{
    RS_LOG(m_log)("IAMPluginCredentialsProvider::GetNameValuePairFromInputTag");

    std::map<rs_string, rs_string> paramMap;
    std::unordered_set<std::string> uniqueTagNames;
    for (const rs_string& input : in_inputTags)
    {
        rs_string name = GetValueByKeyFromInput(input, "name");
        rs_string value = GetValueByKeyFromInput(input, "value");
        rs_wstring nameLower = IAMUtils::toLower(IAMUtils::convertStringToWstring(name));

        if (IAMUtils::isEmpty(nameLower) || !uniqueTagNames.insert(IAMUtils::convertToUTF8(nameLower)).second)
        {
            continue;
        }
        else if (nameLower.find(L"username") != rs_wstring::npos)
        {
            paramMap[name] = m_argsMap[IAM_KEY_USER];
        }
        else if (nameLower.find(L"password") != rs_wstring::npos)
        {
            paramMap[name] = m_argsMap[IAM_KEY_PASSWORD];
        }
        else if (!value.empty())
        {
            paramMap[name] = value;
        }
    }
    return paramMap;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMPluginCredentialsProvider::GetFormActionFromHtml(const rs_string& in_htmlBody)
{
    RS_LOG(m_log)("IAMPluginCredentialsProvider::GetFormActionFromHtml");

    std::smatch match;
    std::regex expression(FORM_ACTION_PATTERN);
    if (std::regex_search(in_htmlBody, match, expression))
    {
        return EscapeHtmlEntity(match.str(1));
    }
    return rs_string();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMPluginCredentialsProvider::EscapeHtmlEntity(const rs_string& in_str)
{
    RS_LOG(m_log)( "IAMPluginCredentialsProvider", "EscapeHtmlEntity");

    rs_string res = "";
    size_t length = in_str.size();
    size_t i = 0;

    while (i < length)
    {
        char curChar = in_str[i];
        if (curChar != '&')
        {
            res += curChar;
            i++;
            continue;
        }

        if (0 == in_str.find("&amp;"))
        {
            res += '&';
            i += 5;
        }
        else if (0 == in_str.find("&apos;"))
        {
            res += '\'';
            i += 6;
        }
        else if (0 == in_str.find("&quot;"))
        {
            res += '"';
            i += 6;
        }
        else if (0 == in_str.find("&lt;"))
        {
            res += '<';
            i += 4;
        }
        else if (0 == in_str.find("&gt;"))
        {
            res += '>';
            i += 4;
        }
        else
        {
            res += curChar;
            i++;
        }
    }

    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMPluginCredentialsProvider::GetValueByKeyFromInput(
    const rs_string& in_input,
    const rs_string& in_key)
{
    const rs_string KEY_PATTERN = "(" + in_key + ")\\s*=\\s*\"(.*?)\"";
    std::smatch match;
    std::regex expression(KEY_PATTERN);

    if (std::regex_search(in_input, match, expression))
    {
        return EscapeHtmlEntity(match.str(2));
    }
    return rs_string();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMPluginCredentialsProvider::GetValueByKeyFromJson(
    const Json::JsonValue& in_jsonNode,
    const rs_string& in_key)
{
    Json::JsonView jsonNodeView(in_jsonNode);

    if (jsonNodeView.ValueExists(in_key))
    {
        const rs_string value = jsonNodeView.GetString(in_key);
        return value;
    }

    return rs_string();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMPluginCredentialsProvider::~IAMPluginCredentialsProvider()
{
    /* Do nothing */
}
