#ifndef _IAMPLUGINCREDENTIALSPROVIDER_H_
#define _IAMPLUGINCREDENTIALSPROVIDER_H_

#include "IAMCredentialsProvider.h"
#include "IAMHttpClient.h"
#include "IAMConfiguration.h"

#include <map>
#include <aws/core/http/HttpClient.h>
#include <aws/core/http/HttpRequest.h>
#include <aws/core/http/HttpResponse.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/utils/xml/XmlSerializer.h>

#include "../rs_iam_support.h"

namespace Aws
{
namespace Utils
{
    namespace Json
    {
        class JsonValue;
    }
}
}

namespace Redshift
{
namespace IamSupport
{
    /// @brief IAMPluginCredentialsProvider abstract class.
    ///        Retrieves AWSCredentials using Ping plug-in.
    class IAMPluginCredentialsProvider : public IAMCredentialsProvider
    {
    public:
        /// @brief Constructor        Construct credentials provider using argument map
        ///
        
        /// @param in_config          The IAM Connection Configuration
        /// @param in_args            Optional arguments map passed to the credentials provider
        explicit IAMPluginCredentialsProvider(
                        const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
                = std::map<rs_string, rs_string>());

        /// @brief  Get AWS credentials for the given credentials provider
        /// 
        /// @return AwsCredentials from the credentials provider
        virtual Aws::Auth::AWSCredentials GetAWSCredentials() override = 0;

        /// @brief  Validate if the arguments map contains all required arguments
        ///         that later will be used by plugin. E.g., app_id in Okta
        ///         recommend to be ran in GetAWSCredentials call in sub-class
        /// 
        /// @exception ErrorException with the error message indicated,
        ///            If any required arguments are missing.
        virtual void ValidateArgumentsMap() = 0;

        // @brief Purpose of this function is to set the key-value pair in the arguments map
        //        1. if in_value is not empty, set m_argsMap[in_key] = in_value
        //        2. if in_value is empty, check for:
        //           2.1 if m_argsMap[in_key] is empty, set m_argsMap[in_key] = in_defaultValue
        //           2.2 else, return.
        //
        /// @param  in_key               Key for setting the argument
        /// @param  in_value             Value for setting the argument
        /// @param  in_defaultValue      Default value for setting the argument
        /// @param  in_urlEncoded        Whether or not the value should be URLEncoded
        virtual void SetArgumentKeyValuePair(
            const rs_string& in_key,
            const rs_string& in_value,
            const rs_string& in_defaultValue = "",
            bool in_urlEncoded = false);

        /// @brief Traverse the entire XmlNode for the attribute name.
        /// 
        /// @param  in_rootNode        Root node of XML Document
        /// @param  in_nodeName        XML node name used to look up attrKey and attrValues
        /// @param  in_attrKey         XML node attribute name
        /// @param  in_attrVal         XML node attribute value for the attrKey
        /// @param  out_attrValues     Output attribute values for the given attribute key
        /// #pragma in_ignoreNodeNamespace
        virtual void GetXmlAttributeValues(
            const Aws::Utils::Xml::XmlNode& in_rootNode,
            const rs_string& in_nodeName,
            const rs_string& in_attrKey,
            const rs_string& in_attrVal,
            std::vector<rs_string>& out_attrValues,
            bool in_ignoreNodeNamespace = false);

        /// @brief  Get all the html input tags from the html body
        /// 
        /// @param  in_htmlBody    The html body used to extract input tags
        /// 
        /// @return A vector contains html input tags
        virtual std::vector<rs_string> GetInputTagsFromHtml(const rs_string& in_htmlBody);

        /// @brief  Get the name-value pair from the input tag vector
        /// 
        /// @param  in_inputTag       The input tags vector
        /// 
        /// @return A map contains name-value pair extracted from the input tag vector
        virtual std::map<rs_string, rs_string> GetNameValuePairFromInputTag(
            const std::vector<rs_string>& in_inputTags);

        /// @brief  Get the form action from html form 
        /// 
        /// @param  in_htmlBody    The html body used to extract form action
        /// 
        /// @return A form action URI string 
        virtual rs_string GetFormActionFromHtml(const rs_string& in_htmlBody);

        /// @brief  Escape special html entities for the given input string
        /// 
        /// @param  in_str        Input string
        /// 
        /// @return A string with special html entities escaped and converted.
        virtual rs_string EscapeHtmlEntity(const rs_string& in_str);

        /// @brief  Get the value corresponding to the key in the input string
        /// 
        /// @param  in_input       The input string
        /// @param  in_key         The key to look up for the value in the input string
        /// 
        /// @return The value of the key in the input if found, else empty string
        virtual rs_string GetValueByKeyFromInput(const rs_string& in_input, const rs_string& in_key);

        /// @brief  Get the value corresponding to the key the in top level of the Json node
        /// 
        /// @param  in_jsonNode    The input Json Node
        /// @param  in_key         The key to look up for the value in the input string
        /// 
        /// @return The value of the key in the Json node if found, else empty string
        virtual rs_string GetValueByKeyFromJson(
            const Aws::Utils::Json::JsonValue& in_jsonNode, 
            const rs_string& in_key);

        /// @brief  Save settings to m_credentialsHoder. For now we only save AWSCredentials, 
        ///         dbUser, dbGroups and autocreate from the m_argsMap
        /// @param  in_credentials    The AWS credentials to be saved
        virtual void SaveSettings(const Aws::Auth::AWSCredentials& in_credentials) override;

        /// @brief  Create a IAMHttpClient
        /// 
        /// @param  in_config       HttpClient configuration
        /// 
        /// @return IAMHttpClient
        virtual std::shared_ptr<IAMHttpClient> GetHttpClient(
            const HttpClientConfig& in_config);

        /// @brief Destructor
        virtual ~IAMPluginCredentialsProvider();

    protected:
        /// @brief Disabled assignment operator to avoid warning.
        IAMPluginCredentialsProvider& operator=(const IAMPluginCredentialsProvider& in_credProvider);
        
        /// Initialize default values for argument map
        /// 
        /// @exception ErrorException if required arguments do not exist
        virtual void InitArgumentsMap();

        /* Arguments used for creating SAML request */
        std::map<rs_string, rs_string> m_argsMap;

        /* Used to filter the received dbGroups from SAML response */
        rs_string m_dbGroupsFilter;
    };
}
}

#endif

