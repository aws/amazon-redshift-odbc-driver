/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralService_EXPORTS.h>
#include <aws/core/utils/memory/stl/AWSVector.h>
#include <aws/redshiftarcadiacoral/model/ConfigStatusString.h>
#include <aws/core/utils/DateTime.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/redshiftarcadiacoral/model/Endpoint.h>
#include <aws/redshiftarcadiacoral/model/VpcConfig.h>
#include <aws/redshiftarcadiacoral/model/ConfigParameter.h>
#include <aws/redshiftarcadiacoral/model/IamRole.h>
#include <utility>

namespace Aws
{
template<typename RESULT_TYPE>
class AmazonWebServiceResult;

namespace Utils
{
namespace Json
{
  class JsonValue;
} // namespace Json
} // namespace Utils
namespace RedshiftArcadiaCoralService
{
namespace Model
{
  class AWS_REDSHIFTARCADIACORALSERVICE_API DescribeConfigurationResult
  {
  public:
    DescribeConfigurationResult();
    DescribeConfigurationResult(const Aws::AmazonWebServiceResult<Aws::Utils::Json::JsonValue>& result);
    DescribeConfigurationResult& operator=(const Aws::AmazonWebServiceResult<Aws::Utils::Json::JsonValue>& result);


    
    inline int GetBaseCapacity() const{ return m_baseCapacity; }

    
    inline void SetBaseCapacity(int value) { m_baseCapacity = value; }

    
    inline DescribeConfigurationResult& WithBaseCapacity(int value) { SetBaseCapacity(value); return *this;}


    
    inline const Aws::Vector<ConfigParameter>& GetConfigParameters() const{ return m_configParameters; }

    
    inline void SetConfigParameters(const Aws::Vector<ConfigParameter>& value) { m_configParameters = value; }

    
    inline void SetConfigParameters(Aws::Vector<ConfigParameter>&& value) { m_configParameters = std::move(value); }

    
    inline DescribeConfigurationResult& WithConfigParameters(const Aws::Vector<ConfigParameter>& value) { SetConfigParameters(value); return *this;}

    
    inline DescribeConfigurationResult& WithConfigParameters(Aws::Vector<ConfigParameter>&& value) { SetConfigParameters(std::move(value)); return *this;}

    
    inline DescribeConfigurationResult& AddConfigParameters(const ConfigParameter& value) { m_configParameters.push_back(value); return *this; }

    
    inline DescribeConfigurationResult& AddConfigParameters(ConfigParameter&& value) { m_configParameters.push_back(std::move(value)); return *this; }


    
    inline const ConfigStatusString& GetConfigStatus() const{ return m_configStatus; }

    
    inline void SetConfigStatus(const ConfigStatusString& value) { m_configStatus = value; }

    
    inline void SetConfigStatus(ConfigStatusString&& value) { m_configStatus = std::move(value); }

    
    inline DescribeConfigurationResult& WithConfigStatus(const ConfigStatusString& value) { SetConfigStatus(value); return *this;}

    
    inline DescribeConfigurationResult& WithConfigStatus(ConfigStatusString&& value) { SetConfigStatus(std::move(value)); return *this;}


    
    inline const Aws::Utils::DateTime& GetCreatedDate() const{ return m_createdDate; }

    
    inline void SetCreatedDate(const Aws::Utils::DateTime& value) { m_createdDate = value; }

    
    inline void SetCreatedDate(Aws::Utils::DateTime&& value) { m_createdDate = std::move(value); }

    
    inline DescribeConfigurationResult& WithCreatedDate(const Aws::Utils::DateTime& value) { SetCreatedDate(value); return *this;}

    
    inline DescribeConfigurationResult& WithCreatedDate(Aws::Utils::DateTime&& value) { SetCreatedDate(std::move(value)); return *this;}


    
    inline const Aws::String& GetDbName() const{ return m_dbName; }

    
    inline void SetDbName(const Aws::String& value) { m_dbName = value; }

    
    inline void SetDbName(Aws::String&& value) { m_dbName = std::move(value); }

    
    inline void SetDbName(const char* value) { m_dbName.assign(value); }

    
    inline DescribeConfigurationResult& WithDbName(const Aws::String& value) { SetDbName(value); return *this;}

    
    inline DescribeConfigurationResult& WithDbName(Aws::String&& value) { SetDbName(std::move(value)); return *this;}

    
    inline DescribeConfigurationResult& WithDbName(const char* value) { SetDbName(value); return *this;}


    
    inline const Aws::String& GetDefaultIamRoleArn() const{ return m_defaultIamRoleArn; }

    
    inline void SetDefaultIamRoleArn(const Aws::String& value) { m_defaultIamRoleArn = value; }

    
    inline void SetDefaultIamRoleArn(Aws::String&& value) { m_defaultIamRoleArn = std::move(value); }

    
    inline void SetDefaultIamRoleArn(const char* value) { m_defaultIamRoleArn.assign(value); }

    
    inline DescribeConfigurationResult& WithDefaultIamRoleArn(const Aws::String& value) { SetDefaultIamRoleArn(value); return *this;}

    
    inline DescribeConfigurationResult& WithDefaultIamRoleArn(Aws::String&& value) { SetDefaultIamRoleArn(std::move(value)); return *this;}

    
    inline DescribeConfigurationResult& WithDefaultIamRoleArn(const char* value) { SetDefaultIamRoleArn(value); return *this;}


    
    inline const Endpoint& GetEndpoint() const{ return m_endpoint; }

    
    inline void SetEndpoint(const Endpoint& value) { m_endpoint = value; }

    
    inline void SetEndpoint(Endpoint&& value) { m_endpoint = std::move(value); }

    
    inline DescribeConfigurationResult& WithEndpoint(const Endpoint& value) { SetEndpoint(value); return *this;}

    
    inline DescribeConfigurationResult& WithEndpoint(Endpoint&& value) { SetEndpoint(std::move(value)); return *this;}


    
    inline const Aws::Vector<Aws::String>& GetExportedLogs() const{ return m_exportedLogs; }

    
    inline void SetExportedLogs(const Aws::Vector<Aws::String>& value) { m_exportedLogs = value; }

    
    inline void SetExportedLogs(Aws::Vector<Aws::String>&& value) { m_exportedLogs = std::move(value); }

    
    inline DescribeConfigurationResult& WithExportedLogs(const Aws::Vector<Aws::String>& value) { SetExportedLogs(value); return *this;}

    
    inline DescribeConfigurationResult& WithExportedLogs(Aws::Vector<Aws::String>&& value) { SetExportedLogs(std::move(value)); return *this;}

    
    inline DescribeConfigurationResult& AddExportedLogs(const Aws::String& value) { m_exportedLogs.push_back(value); return *this; }

    
    inline DescribeConfigurationResult& AddExportedLogs(Aws::String&& value) { m_exportedLogs.push_back(std::move(value)); return *this; }

    
    inline DescribeConfigurationResult& AddExportedLogs(const char* value) { m_exportedLogs.push_back(value); return *this; }


    
    inline const Aws::Vector<IamRole>& GetIamRoles() const{ return m_iamRoles; }

    
    inline void SetIamRoles(const Aws::Vector<IamRole>& value) { m_iamRoles = value; }

    
    inline void SetIamRoles(Aws::Vector<IamRole>&& value) { m_iamRoles = std::move(value); }

    
    inline DescribeConfigurationResult& WithIamRoles(const Aws::Vector<IamRole>& value) { SetIamRoles(value); return *this;}

    
    inline DescribeConfigurationResult& WithIamRoles(Aws::Vector<IamRole>&& value) { SetIamRoles(std::move(value)); return *this;}

    
    inline DescribeConfigurationResult& AddIamRoles(const IamRole& value) { m_iamRoles.push_back(value); return *this; }

    
    inline DescribeConfigurationResult& AddIamRoles(IamRole&& value) { m_iamRoles.push_back(std::move(value)); return *this; }


    
    inline const Aws::String& GetKmsKeyId() const{ return m_kmsKeyId; }

    
    inline void SetKmsKeyId(const Aws::String& value) { m_kmsKeyId = value; }

    
    inline void SetKmsKeyId(Aws::String&& value) { m_kmsKeyId = std::move(value); }

    
    inline void SetKmsKeyId(const char* value) { m_kmsKeyId.assign(value); }

    
    inline DescribeConfigurationResult& WithKmsKeyId(const Aws::String& value) { SetKmsKeyId(value); return *this;}

    
    inline DescribeConfigurationResult& WithKmsKeyId(Aws::String&& value) { SetKmsKeyId(std::move(value)); return *this;}

    
    inline DescribeConfigurationResult& WithKmsKeyId(const char* value) { SetKmsKeyId(value); return *this;}


    
    inline const Aws::String& GetMasterUsername() const{ return m_masterUsername; }

    
    inline void SetMasterUsername(const Aws::String& value) { m_masterUsername = value; }

    
    inline void SetMasterUsername(Aws::String&& value) { m_masterUsername = std::move(value); }

    
    inline void SetMasterUsername(const char* value) { m_masterUsername.assign(value); }

    
    inline DescribeConfigurationResult& WithMasterUsername(const Aws::String& value) { SetMasterUsername(value); return *this;}

    
    inline DescribeConfigurationResult& WithMasterUsername(Aws::String&& value) { SetMasterUsername(std::move(value)); return *this;}

    
    inline DescribeConfigurationResult& WithMasterUsername(const char* value) { SetMasterUsername(value); return *this;}


    
    inline const Aws::String& GetNamespaceArn() const{ return m_namespaceArn; }

    
    inline void SetNamespaceArn(const Aws::String& value) { m_namespaceArn = value; }

    
    inline void SetNamespaceArn(Aws::String&& value) { m_namespaceArn = std::move(value); }

    
    inline void SetNamespaceArn(const char* value) { m_namespaceArn.assign(value); }

    
    inline DescribeConfigurationResult& WithNamespaceArn(const Aws::String& value) { SetNamespaceArn(value); return *this;}

    
    inline DescribeConfigurationResult& WithNamespaceArn(Aws::String&& value) { SetNamespaceArn(std::move(value)); return *this;}

    
    inline DescribeConfigurationResult& WithNamespaceArn(const char* value) { SetNamespaceArn(value); return *this;}


    
    inline const VpcConfig& GetVpcConfig() const{ return m_vpcConfig; }

    
    inline void SetVpcConfig(const VpcConfig& value) { m_vpcConfig = value; }

    
    inline void SetVpcConfig(VpcConfig&& value) { m_vpcConfig = std::move(value); }

    
    inline DescribeConfigurationResult& WithVpcConfig(const VpcConfig& value) { SetVpcConfig(value); return *this;}

    
    inline DescribeConfigurationResult& WithVpcConfig(VpcConfig&& value) { SetVpcConfig(std::move(value)); return *this;}

  private:

    int m_baseCapacity;

    Aws::Vector<ConfigParameter> m_configParameters;

    ConfigStatusString m_configStatus;

    Aws::Utils::DateTime m_createdDate;

    Aws::String m_dbName;

    Aws::String m_defaultIamRoleArn;

    Endpoint m_endpoint;

    Aws::Vector<Aws::String> m_exportedLogs;

    Aws::Vector<IamRole> m_iamRoles;

    Aws::String m_kmsKeyId;

    Aws::String m_masterUsername;

    Aws::String m_namespaceArn;

    VpcConfig m_vpcConfig;
  };

} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
