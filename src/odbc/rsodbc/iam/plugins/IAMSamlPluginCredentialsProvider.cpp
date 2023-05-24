#include "IAMSamlPluginCredentialsProvider.h"
#include "IAMUtils.h"
#include "RsLogger.h"

#include <regex>
#include <aws/sts/STSClient.h>
#include <aws/sts/model/AssumeRoleWithSAMLRequest.h>
#include <aws/core/utils/base64/Base64.h>

using namespace Redshift::IamSupport;
using namespace Aws::Auth;
using namespace Aws::Client;
using namespace Aws::STS;
using namespace Aws::Utils;
using namespace Aws::Http;

namespace
{
    static const char* LOG_TAG = "IAMSamlPluginCredentialsProvider";

    /* Attribute names used when extracting connection attribute information from
    the SAML response in XML format */
    static const rs_string SAML_ATTR_ROLE = "https://aws.amazon.com/SAML/Attributes/Role";
    static const rs_string SAML_ATTR_DBUSER = "https://redshift.amazon.com/SAML/Attributes/DbUser";
    static const rs_string SAML_ATTR_DBUSER_ALT = "https://aws.amazon.com/SAML/Attributes/RoleSessionName";
    static const rs_string SAML_ATTR_DBGROUPS = "https://redshift.amazon.com/SAML/Attributes/DbGroups";
    static const rs_string SAML_ATTR_FORCELOWERCASE = "https://redshift.amazon.com/SAML/Attributes/ForceLowercase";
    static const rs_string SAML_ATTR_AUTOCREATE = "https://redshift.amazon.com/SAML/Attributes/AutoCreate";

    /* A Attribute node, for instance:

    <Attribute Name="https://redshift.amazon.com/SAML/Attributes/DbGroups">
    <AttributeValue>group1</AttributeValue>
    </Attribute>

    Has a Name attribute. This is the key we use to identify the attribute node
    and extract information such as dbUser, dbGroups, etc.
    */

    static const rs_string SAML_ATTR_KEYNAME = "Name";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMSamlPluginCredentialsProvider::IAMSamlPluginCredentialsProvider(
    RsLogger* in_log,
    const IAMConfiguration& in_config,
    const std::map<rs_string, rs_string>& in_argsMap) :
    IAMPluginCredentialsProvider(in_log, in_config, in_argsMap)
{
    RS_LOG(m_log)("IAMSamlPluginCredentialsProvider::IAMSamlPluginCredentialsProvider");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AWSCredentials IAMSamlPluginCredentialsProvider::GetAWSCredentials()
{
    RS_LOG(m_log)("IAMSamlPluginCredentialsProvider::GetAWSCredentials");
    /* return cached AWSCredentials from the IAMCredentialsHolder */
    if (CanUseCachedAwsCredentials())
    {
        return m_credentials.GetAWSCredentials();
    }

    /* Validate that all required arguments for plugin are provided */
    ValidateArgumentsMap();

    AWSCredentials credentials = GetAWSCredentialsWithSaml(GetSamlAssertion());
    SaveSettings(credentials); /* cache returned credentials in IAMCredentials Holder */
    return credentials;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMSamlPluginCredentialsProvider::ValidateArgumentsMap()
{
    RS_LOG(m_log)("IAMSamlPluginCredentialsProvider::ValidateArgumentsMap");

    /* Set default port to 443 */
    if (!m_argsMap.count(IAM_KEY_IDP_PORT))
    {
        m_argsMap[IAM_KEY_IDP_PORT] = std::to_string(HTTPS_DEFAULT_PORT);
    }

    if (!m_argsMap.count(IAM_KEY_IDP_HOST))
    {
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "Authentication failed, please verify that IDP_HOST is provided.");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AWSCredentials IAMSamlPluginCredentialsProvider::GetAWSCredentialsWithSaml(
    const rs_string& in_samlAssertion)
{
    RS_LOG(m_log)("IAMSamlPluginCredentialsProvider::GetAWSCredentialsWithSaml");

    if (in_samlAssertion.empty())
    {
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "Failed to retrieve SAML assertion. Please verify the connection settings.");
    }

    RS_LOG(m_log)("IAMSamlPluginCredentialsProvider::GetAWSCredentialsWithSaml Saml Assertion: %s",
                    in_samlAssertion.c_str());

    Base64::Base64 base64;
    ByteBuffer samlByteBuffer = base64.Decode(in_samlAssertion);
    const rs_string samlContent(
        reinterpret_cast<const char*>(
        samlByteBuffer.GetUnderlyingData()),
        samlByteBuffer.GetLength());

    Xml::XmlDocument samlXmlDoc = Xml::XmlDocument::CreateFromXmlString(samlContent);
    if (!samlXmlDoc.WasParseSuccessful())
    {
        IAMUtils::ThrowConnectionExceptionWithInfo(samlXmlDoc.GetErrorMessage());
    }

    std::vector<rs_string> attrRoles;
    const Xml::XmlNode rootNode = samlXmlDoc.GetRootElement();
    GetSamlXmlAttributeValues(
        rootNode,
        SAML_ATTR_KEYNAME,
        SAML_ATTR_ROLE,
        attrRoles);

    /* Parse the node list (attrRoles) and populate the roles, in RolesArn : principalArn pair */
    std::map<rs_string, rs_string> roles;

    for (size_t i = 0; i < attrRoles.size(); i++)
    {
        // Sample role attribute (Note: saml-provider (PrincipleArn) and role and be arbitrary order): 
        // arn:aws:iam::518627716765:saml-provider/ADFS,arn:aws:iam::518627716765:role/ADFS-Production
        std::vector<rs_string> roleInfo = IAMUtils::TokenizeSetting(attrRoles[i], ",");

        if (roleInfo.size() == 2)
        {
            // Role arn and role principal pair
            const rs_string& first = roleInfo[0];
            const rs_string& second = roleInfo[1];

            // trimming whitespaces at the begining and the end of the Role arn & role principal pair
            // std::string trim(std::string& first);
            // std::string trim(std::string& second);

            if (first.find(":saml-provider/") == rs_string::npos)
            {
                roles[first] = second;
            }
            else
            {
                roles[second] = first;
            }
        }
    }

    if (roles.empty())
    {
        IAMUtils::ThrowConnectionExceptionWithInfo("No role found in the SAML assertion.");
    }

    rs_string roleArn, principalArn;

    if (m_argsMap.count(IAM_KEY_PREFERRED_ROLE))
    {
        roleArn = m_argsMap[IAM_KEY_PREFERRED_ROLE];
        principalArn = roles[roleArn];
        if (principalArn.empty())
        {
            IAMUtils::ThrowConnectionExceptionWithInfo("Preferred role not found in the SAML assertion.");
        }
    }
    else
    {
        /* Use the first role in the roles map if no preferred role specified */
        roleArn = roles.begin()->first;
        principalArn = roles.begin()->second;
    }

    roleArn = IAMUtils::rs_trim(roleArn);
    principalArn = IAMUtils::rs_trim(principalArn);


    RS_LOG(m_log)("IAMSamlPluginCredentialsProvider::GetAWSCredentialsWithSaml "
           "Using RoleArn: %s, PrincipalArn: %s",
          roleArn.c_str(), principalArn.c_str());

    /* Extracting additional settings such as dbUser, dbGroups, forceLowercase, autocreate */
    ExtractArgsFromSamlAssertion(rootNode);

    return AssumeRoleWithSamlRequest(in_samlAssertion, roleArn, principalArn);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AWSCredentials IAMSamlPluginCredentialsProvider::AssumeRoleWithSamlRequest(
    const rs_string& in_samlAssertion,
    const rs_string& in_roleArn,
    const rs_string& in_principalArn)
{
    RS_LOG(m_log)("IAMSamlPluginCredentialsProvider::AssumeRoleWithSamlRequest");

    ClientConfiguration config;
    
    if(!m_config.GetRegion().empty())
    {
      config.region = m_config.GetRegion();
    }


#ifndef _WIN32
    // Added CA file to the config for verifying STS server certificate
    if (!m_config.GetCaFile().empty())
    {
        config.caFile = m_config.GetCaFile();
    }
    else
    {
        config.caFile = IAMUtils::convertToUTF8(IAMUtils::GetDefaultCaFile()); // .GetAsPlatformString()
    }

#endif // !_WIN32

    if (m_config.GetUsingHTTPSProxy())
    {
        config.proxyHost = m_config.GetHTTPSProxyHost();
        config.proxyPort = m_config.GetHTTPSProxyPort();
        config.proxyUserName = m_config.GetHTTPSProxyUser();
        config.proxyPassword = m_config.GetHTTPSProxyPassword();
    }

	config.endpointOverride = m_config.GetStsEndpointUrl();
	config.httpRequestTimeoutMs = m_config.GetStsConnectionTimeout();
	config.connectTimeoutMs = m_config.GetStsConnectionTimeout();
	config.requestTimeoutMs = m_config.GetStsConnectionTimeout();

	RS_LOG(m_log)("IAMSamlPluginCredentialsProvider::AssumeRoleWithSamlRequest",
		"httpRequestTimeoutMs: %ld, connectTimeoutMs: %ld, requestTimeoutMs: %ld",
		config.httpRequestTimeoutMs, config.connectTimeoutMs, config.requestTimeoutMs);

    STSClient client(Aws::MakeShared<AnonymousAWSCredentialsProvider>(LOG_TAG), config);

    /* configure saml request */
    Model::AssumeRoleWithSAMLRequest request;

    request.SetSAMLAssertion(in_samlAssertion);
    request.SetRoleArn(in_roleArn);
    request.SetPrincipalArn(in_principalArn);

    int durationSecond = 0;
    if (m_argsMap.count(IAM_KEY_DURATION))
    {
        durationSecond = atoi(m_argsMap[IAM_KEY_DURATION].c_str());
    }

    if (durationSecond > 0)
    {
        request.SetDurationSeconds(durationSecond);
    }

    Model::AssumeRoleWithSAMLOutcome outcome = client.AssumeRoleWithSAML(request);

    if (!outcome.IsSuccess())
    {
        const AWSError<STSErrors>& error = outcome.GetError();
        const rs_string& exceptionName = error.GetExceptionName();
        const rs_string& errorMessage = error.GetMessage();

        rs_string fullErrorMsg =
            exceptionName + 
            ": " + 
            errorMessage + 
            " (HTTP response code: " + 
                std::to_string(static_cast<short>(
                    error.GetResponseCode())) +
            ")";
        IAMUtils::ThrowConnectionExceptionWithInfo(fullErrorMsg);
    }

    const Model::Credentials& credentials = outcome.GetResult().GetCredentials();

    return AWSCredentials(
        credentials.GetAccessKeyId(),
        credentials.GetSecretAccessKey(),
        credentials.GetSessionToken());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMSamlPluginCredentialsProvider::ExtractSamlAssertion(
    const rs_string& in_content,
    const rs_string& in_pattern)
{
    RS_LOG(m_log)("IAMSamlPluginCredentialsProvider::ExtractSamlAssertion");

    /* Extract samlAssertion from from the HTML content */
    std::smatch match;
    std::regex expression(in_pattern);
    
    if (!std::regex_search(in_content, match, expression))
    {
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "SAML assertion not found.");
    }

    /* Return SAML assertion */
    return match.str(1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMSamlPluginCredentialsProvider::FilterDbGroups(
    std::vector<rs_string>& io_groups, const rs_string& in_reg_exp)
{
    RS_LOG(m_log)("IAMSamlPluginCredentialsProvider::FilterDbGroups Filter %s", in_reg_exp.c_str());
    
    if (in_reg_exp.empty() || io_groups.empty())
    {
        return;
    }
    
    std::regex filter(in_reg_exp);
    auto predicate = [&](const rs_string& group)
    {
        bool is_excluded = regex_match(group, filter);
        if (is_excluded)
        {
            RS_LOG(m_log)("IAMSamlPluginCredentialsProvider::FilterDbGroups Exclude group %s", group.c_str());
        }
        return is_excluded;
    };
    
    const auto it = remove_if(io_groups.begin(), io_groups.end(), predicate);
    io_groups.erase(it, io_groups.end());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMSamlPluginCredentialsProvider::ExtractArgsFromSamlAssertion(const Aws::Utils::Xml::XmlNode& in_rootNode)
{
    RS_LOG(m_log)( "IAMSamlPluginCredentialsProvider::ExtractArgsFromSamlAssertion");

    std::vector<rs_string> response;

    if (!m_argsMap.count(IAM_KEY_DBUSER))
    {
        /* Get attribute DbUser from the SAML assertion */
        GetSamlXmlAttributeValues(
            in_rootNode,
            SAML_ATTR_KEYNAME,
            SAML_ATTR_DBUSER,
            response);
        
        /* Use the SAML_ATTR_DBUSER_ALT to look for dbUser */
        if (response.empty())
        {
            GetSamlXmlAttributeValues(
                in_rootNode,
                SAML_ATTR_KEYNAME,
                SAML_ATTR_DBUSER_ALT,
                response);
        }

        if (!response.empty())
        {
            m_argsMap[IAM_KEY_DBUSER] = response[0];
            response.clear();
        }
    }

    if (!m_argsMap.count(IAM_KEY_DBGROUPS))
    {
        /* Get attribute dbGroups from the SAML assertion */
        GetSamlXmlAttributeValues(
            in_rootNode,
            SAML_ATTR_KEYNAME,
            SAML_ATTR_DBGROUPS,
            response);

        /* Apply regular expression to filter the dbGroups received from SAML response */
        FilterDbGroups(response, m_dbGroupsFilter);

        if (!response.empty())
        {
            // Create comma-separated string from vector
            Aws::String dbGroups;
            for (int i = 0; i < response.size(); ++i)
            {
                dbGroups += response[i];
                if (response.size() - 1 != i)
                {
                    dbGroups += ",";
                }
            }
            m_argsMap[IAM_KEY_DBGROUPS] = dbGroups;
            response.clear();
        }
    }

    /* Get attribute ForceLowercase from the SAML assertion */
    GetSamlXmlAttributeValues(
        in_rootNode,
        SAML_ATTR_KEYNAME,
        SAML_ATTR_FORCELOWERCASE,
        response);
    
    if (!response.empty() && IAMUtils::ConvertStringToBool(response[0]))
    {
        m_argsMap[IAM_KEY_FORCELOWERCASE] = "1";
        response.clear();
    }

    /* Get attribute AutoCreate from the SAML assertion */
    GetSamlXmlAttributeValues(
        in_rootNode, 
        SAML_ATTR_KEYNAME,
        SAML_ATTR_AUTOCREATE, 
        response);

    if (!response.empty() && IAMUtils::ConvertStringToBool(response[0]))
    {
        m_argsMap[IAM_KEY_AUTOCREATE] = "1";
        response.clear();
    } 
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMSamlPluginCredentialsProvider::GetSamlXmlAttributeValues(
    const Xml::XmlNode& in_rootNode,
    const rs_string& in_attrKey,
    const rs_string& in_attrVal,
    std::vector<rs_string>& out_attrValues)
{
    RS_LOG(m_log)("IAMSamlPluginCredentialsProvider::GetSamlXmlAttributeValues");

    GetXmlAttributeValues(in_rootNode, "Attribute", in_attrKey, in_attrVal, out_attrValues, true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMSamlPluginCredentialsProvider::~IAMSamlPluginCredentialsProvider()
{
    RS_LOG(m_log)("IAMSamlPluginCredentialsProvider::~IAMSamlPluginCredentialsProvider");
    /* Do nothing */
}
