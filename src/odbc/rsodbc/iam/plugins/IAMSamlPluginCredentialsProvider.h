#ifndef _IAMSAMLPLUGINCREDENTIALSPROVIDER_H_
#define _IAMSAMLPLUGINCREDENTIALSPROVIDER_H_

#include "../rs_iam_support.h"
#include "IAMPluginCredentialsProvider.h"

namespace Redshift
{
namespace IamSupport
{
    class IAMSamlPluginCredentialsProvider : public IAMPluginCredentialsProvider
    {
    public:
        /// @brief Constructor        Construct credentials provider using argument map
        ///
        /// @param in_log             The logger. (NOT OWN)
        /// @param in_config          The IAM Connection Configuration
        /// @param in_argsMap         Optional arguments map passed to the credentials provider
        explicit IAMSamlPluginCredentialsProvider(
            RsLogger* in_log,
            const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());

        /// @brief  Get AWS credentials for the given credentials provider
        /// 
        /// @return AwsCredentials from the credentials provider
        virtual Aws::Auth::AWSCredentials GetAWSCredentials() override;

        /// @brief  Validate if the arguments map contains all required arguments
        ///         that later will be used by plugin. E.g., app_id in Okta
        /// 
        /// @exception ErrorException with the error message indicated,
        ///            If any required arguments are missing.
        virtual void ValidateArgumentsMap() override;

        /// @brief Get the AWS credentials from the SAML assertion
        /// 
        /// @param in_samlAssertion   SAML assertion used to retrieve AWSCredentials
        /// 
        /// @return AwsCredentials from the SAML assertion
        virtual Aws::Auth::AWSCredentials GetAWSCredentialsWithSaml(
            const rs_string& in_samlAssertion);

        /// @brief  Assume role using SAML Request
        /// 
        /// @param in_samlAssertion        The SAML assertion
        /// @param in_roleArn              The AWS role arn
        /// @param in_principalArn         The AWS principal arn
        /// 
        /// @return AWSCredentials
        /// 
        /// @exception ErrorException if assume role with saml request failed
        virtual Aws::Auth::AWSCredentials AssumeRoleWithSamlRequest(
            const rs_string& in_samlAssertion,
            const rs_string& in_roleArn,
            const rs_string& in_principalArn);

        /// @brief  Get saml assertion from given connection settings
        /// 
        /// @return Saml assertion
        virtual rs_string GetSamlAssertion()
        { 
            return rs_string();
        }

        /// @brief Traverse the entire XmlNode to extract predefined Attribute
        ///        node names for attrVal given the corresponding attrKey.
        /// 
        /// @param  in_rootNode
        /// @param  in_attrKey
        /// @param  in_attrVal
        /// @param  out_attrValues
        virtual void GetSamlXmlAttributeValues(
            const Aws::Utils::Xml::XmlNode& in_rootNode,
            const rs_string& in_attrKey,
            const rs_string& in_attrVal,
            std::vector<rs_string>& out_attrValues);

        /// @brief Destructor
        virtual ~IAMSamlPluginCredentialsProvider();

    protected:
        /// @brief Disabled assignment operator to avoid warning.
        IAMSamlPluginCredentialsProvider& operator=(const IAMSamlPluginCredentialsProvider& in_adfsProvider);

        /// @brief  Extract settings from the SAML assertion and save to m_argsMap
        ///         For now we only save dbUser, dbGroups and autocreate
        /// 
        /// @param  in_rootNode    SAML assertion in XML node format that contains additional settings
        void ExtractArgsFromSamlAssertion(const Aws::Utils::Xml::XmlNode& in_rootNode);

        /// @brief  Filter vector of dbGroups by applying reg_exp.
        /// 
        /// @param  io_groups           Vector of dbGroups, extracted from SAML response
        /// @param  in_reg_exp          Regex filter
        void FilterDbGroups(std::vector<rs_string>& io_groups, const rs_string& in_reg_exp);

        /// @brief  Extract saml assertion from the response content
        /// 
        /// @param  in_content     Content used to extract saml assertion
        /// @param  in_pattern     Pattern used to extract saml assertion from the content
        /// 
        /// @return Saml assertion string 
        rs_string ExtractSamlAssertion(const rs_string& in_content, const rs_string& in_pattern);

    };
}
}

#endif
