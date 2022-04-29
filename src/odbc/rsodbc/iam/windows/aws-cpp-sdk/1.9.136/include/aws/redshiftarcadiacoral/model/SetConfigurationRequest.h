/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralService_EXPORTS.h>
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralServiceRequest.h>
#include <aws/core/utils/memory/stl/AWSVector.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/redshiftarcadiacoral/model/VpcConfig.h>
#include <aws/redshiftarcadiacoral/model/ConfigParameter.h>
#include <utility>

namespace Aws
{
namespace RedshiftArcadiaCoralService
{
namespace Model
{

  /**
   */
  class AWS_REDSHIFTARCADIACORALSERVICE_API SetConfigurationRequest : public RedshiftArcadiaCoralServiceRequest
  {
  public:
    SetConfigurationRequest();

    // Service request name is the Operation name which will send this request out,
    // each operation should has unique request name, so that we can get operation's name from this request.
    // Note: this is not true for response, multiple operations may have the same response name,
    // so we can not get operation's name from response.
    inline virtual const char* GetServiceRequestName() const override { return "SetConfiguration"; }

    Aws::String SerializePayload() const override;

    Aws::Http::HeaderValueCollection GetRequestSpecificHeaders() const override;


    
    inline int GetBaseCapacity() const{ return m_baseCapacity; }

    
    inline bool BaseCapacityHasBeenSet() const { return m_baseCapacityHasBeenSet; }

    
    inline void SetBaseCapacity(int value) { m_baseCapacityHasBeenSet = true; m_baseCapacity = value; }

    
    inline SetConfigurationRequest& WithBaseCapacity(int value) { SetBaseCapacity(value); return *this;}


    
    inline const Aws::Vector<ConfigParameter>& GetConfigParameters() const{ return m_configParameters; }

    
    inline bool ConfigParametersHasBeenSet() const { return m_configParametersHasBeenSet; }

    
    inline void SetConfigParameters(const Aws::Vector<ConfigParameter>& value) { m_configParametersHasBeenSet = true; m_configParameters = value; }

    
    inline void SetConfigParameters(Aws::Vector<ConfigParameter>&& value) { m_configParametersHasBeenSet = true; m_configParameters = std::move(value); }

    
    inline SetConfigurationRequest& WithConfigParameters(const Aws::Vector<ConfigParameter>& value) { SetConfigParameters(value); return *this;}

    
    inline SetConfigurationRequest& WithConfigParameters(Aws::Vector<ConfigParameter>&& value) { SetConfigParameters(std::move(value)); return *this;}

    
    inline SetConfigurationRequest& AddConfigParameters(const ConfigParameter& value) { m_configParametersHasBeenSet = true; m_configParameters.push_back(value); return *this; }

    
    inline SetConfigurationRequest& AddConfigParameters(ConfigParameter&& value) { m_configParametersHasBeenSet = true; m_configParameters.push_back(std::move(value)); return *this; }


    
    inline const Aws::String& GetDbName() const{ return m_dbName; }

    
    inline bool DbNameHasBeenSet() const { return m_dbNameHasBeenSet; }

    
    inline void SetDbName(const Aws::String& value) { m_dbNameHasBeenSet = true; m_dbName = value; }

    
    inline void SetDbName(Aws::String&& value) { m_dbNameHasBeenSet = true; m_dbName = std::move(value); }

    
    inline void SetDbName(const char* value) { m_dbNameHasBeenSet = true; m_dbName.assign(value); }

    
    inline SetConfigurationRequest& WithDbName(const Aws::String& value) { SetDbName(value); return *this;}

    
    inline SetConfigurationRequest& WithDbName(Aws::String&& value) { SetDbName(std::move(value)); return *this;}

    
    inline SetConfigurationRequest& WithDbName(const char* value) { SetDbName(value); return *this;}


    
    inline const Aws::String& GetDefaultIamRoleArn() const{ return m_defaultIamRoleArn; }

    
    inline bool DefaultIamRoleArnHasBeenSet() const { return m_defaultIamRoleArnHasBeenSet; }

    
    inline void SetDefaultIamRoleArn(const Aws::String& value) { m_defaultIamRoleArnHasBeenSet = true; m_defaultIamRoleArn = value; }

    
    inline void SetDefaultIamRoleArn(Aws::String&& value) { m_defaultIamRoleArnHasBeenSet = true; m_defaultIamRoleArn = std::move(value); }

    
    inline void SetDefaultIamRoleArn(const char* value) { m_defaultIamRoleArnHasBeenSet = true; m_defaultIamRoleArn.assign(value); }

    
    inline SetConfigurationRequest& WithDefaultIamRoleArn(const Aws::String& value) { SetDefaultIamRoleArn(value); return *this;}

    
    inline SetConfigurationRequest& WithDefaultIamRoleArn(Aws::String&& value) { SetDefaultIamRoleArn(std::move(value)); return *this;}

    
    inline SetConfigurationRequest& WithDefaultIamRoleArn(const char* value) { SetDefaultIamRoleArn(value); return *this;}


    
    inline const Aws::Vector<Aws::String>& GetExportedLogs() const{ return m_exportedLogs; }

    
    inline bool ExportedLogsHasBeenSet() const { return m_exportedLogsHasBeenSet; }

    
    inline void SetExportedLogs(const Aws::Vector<Aws::String>& value) { m_exportedLogsHasBeenSet = true; m_exportedLogs = value; }

    
    inline void SetExportedLogs(Aws::Vector<Aws::String>&& value) { m_exportedLogsHasBeenSet = true; m_exportedLogs = std::move(value); }

    
    inline SetConfigurationRequest& WithExportedLogs(const Aws::Vector<Aws::String>& value) { SetExportedLogs(value); return *this;}

    
    inline SetConfigurationRequest& WithExportedLogs(Aws::Vector<Aws::String>&& value) { SetExportedLogs(std::move(value)); return *this;}

    
    inline SetConfigurationRequest& AddExportedLogs(const Aws::String& value) { m_exportedLogsHasBeenSet = true; m_exportedLogs.push_back(value); return *this; }

    
    inline SetConfigurationRequest& AddExportedLogs(Aws::String&& value) { m_exportedLogsHasBeenSet = true; m_exportedLogs.push_back(std::move(value)); return *this; }

    
    inline SetConfigurationRequest& AddExportedLogs(const char* value) { m_exportedLogsHasBeenSet = true; m_exportedLogs.push_back(value); return *this; }


    
    inline const Aws::Vector<Aws::String>& GetIamRoles() const{ return m_iamRoles; }

    
    inline bool IamRolesHasBeenSet() const { return m_iamRolesHasBeenSet; }

    
    inline void SetIamRoles(const Aws::Vector<Aws::String>& value) { m_iamRolesHasBeenSet = true; m_iamRoles = value; }

    
    inline void SetIamRoles(Aws::Vector<Aws::String>&& value) { m_iamRolesHasBeenSet = true; m_iamRoles = std::move(value); }

    
    inline SetConfigurationRequest& WithIamRoles(const Aws::Vector<Aws::String>& value) { SetIamRoles(value); return *this;}

    
    inline SetConfigurationRequest& WithIamRoles(Aws::Vector<Aws::String>&& value) { SetIamRoles(std::move(value)); return *this;}

    
    inline SetConfigurationRequest& AddIamRoles(const Aws::String& value) { m_iamRolesHasBeenSet = true; m_iamRoles.push_back(value); return *this; }

    
    inline SetConfigurationRequest& AddIamRoles(Aws::String&& value) { m_iamRolesHasBeenSet = true; m_iamRoles.push_back(std::move(value)); return *this; }

    
    inline SetConfigurationRequest& AddIamRoles(const char* value) { m_iamRolesHasBeenSet = true; m_iamRoles.push_back(value); return *this; }


    
    inline const Aws::String& GetKmsKeyId() const{ return m_kmsKeyId; }

    
    inline bool KmsKeyIdHasBeenSet() const { return m_kmsKeyIdHasBeenSet; }

    
    inline void SetKmsKeyId(const Aws::String& value) { m_kmsKeyIdHasBeenSet = true; m_kmsKeyId = value; }

    
    inline void SetKmsKeyId(Aws::String&& value) { m_kmsKeyIdHasBeenSet = true; m_kmsKeyId = std::move(value); }

    
    inline void SetKmsKeyId(const char* value) { m_kmsKeyIdHasBeenSet = true; m_kmsKeyId.assign(value); }

    
    inline SetConfigurationRequest& WithKmsKeyId(const Aws::String& value) { SetKmsKeyId(value); return *this;}

    
    inline SetConfigurationRequest& WithKmsKeyId(Aws::String&& value) { SetKmsKeyId(std::move(value)); return *this;}

    
    inline SetConfigurationRequest& WithKmsKeyId(const char* value) { SetKmsKeyId(value); return *this;}


    
    inline const Aws::String& GetMasterUserPassword() const{ return m_masterUserPassword; }

    
    inline bool MasterUserPasswordHasBeenSet() const { return m_masterUserPasswordHasBeenSet; }

    
    inline void SetMasterUserPassword(const Aws::String& value) { m_masterUserPasswordHasBeenSet = true; m_masterUserPassword = value; }

    
    inline void SetMasterUserPassword(Aws::String&& value) { m_masterUserPasswordHasBeenSet = true; m_masterUserPassword = std::move(value); }

    
    inline void SetMasterUserPassword(const char* value) { m_masterUserPasswordHasBeenSet = true; m_masterUserPassword.assign(value); }

    
    inline SetConfigurationRequest& WithMasterUserPassword(const Aws::String& value) { SetMasterUserPassword(value); return *this;}

    
    inline SetConfigurationRequest& WithMasterUserPassword(Aws::String&& value) { SetMasterUserPassword(std::move(value)); return *this;}

    
    inline SetConfigurationRequest& WithMasterUserPassword(const char* value) { SetMasterUserPassword(value); return *this;}


    
    inline const Aws::String& GetMasterUsername() const{ return m_masterUsername; }

    
    inline bool MasterUsernameHasBeenSet() const { return m_masterUsernameHasBeenSet; }

    
    inline void SetMasterUsername(const Aws::String& value) { m_masterUsernameHasBeenSet = true; m_masterUsername = value; }

    
    inline void SetMasterUsername(Aws::String&& value) { m_masterUsernameHasBeenSet = true; m_masterUsername = std::move(value); }

    
    inline void SetMasterUsername(const char* value) { m_masterUsernameHasBeenSet = true; m_masterUsername.assign(value); }

    
    inline SetConfigurationRequest& WithMasterUsername(const Aws::String& value) { SetMasterUsername(value); return *this;}

    
    inline SetConfigurationRequest& WithMasterUsername(Aws::String&& value) { SetMasterUsername(std::move(value)); return *this;}

    
    inline SetConfigurationRequest& WithMasterUsername(const char* value) { SetMasterUsername(value); return *this;}


    
    inline const VpcConfig& GetVpcConfig() const{ return m_vpcConfig; }

    
    inline bool VpcConfigHasBeenSet() const { return m_vpcConfigHasBeenSet; }

    
    inline void SetVpcConfig(const VpcConfig& value) { m_vpcConfigHasBeenSet = true; m_vpcConfig = value; }

    
    inline void SetVpcConfig(VpcConfig&& value) { m_vpcConfigHasBeenSet = true; m_vpcConfig = std::move(value); }

    
    inline SetConfigurationRequest& WithVpcConfig(const VpcConfig& value) { SetVpcConfig(value); return *this;}

    
    inline SetConfigurationRequest& WithVpcConfig(VpcConfig&& value) { SetVpcConfig(std::move(value)); return *this;}

  private:

    int m_baseCapacity;
    bool m_baseCapacityHasBeenSet;

    Aws::Vector<ConfigParameter> m_configParameters;
    bool m_configParametersHasBeenSet;

    Aws::String m_dbName;
    bool m_dbNameHasBeenSet;

    Aws::String m_defaultIamRoleArn;
    bool m_defaultIamRoleArnHasBeenSet;

    Aws::Vector<Aws::String> m_exportedLogs;
    bool m_exportedLogsHasBeenSet;

    Aws::Vector<Aws::String> m_iamRoles;
    bool m_iamRolesHasBeenSet;

    Aws::String m_kmsKeyId;
    bool m_kmsKeyIdHasBeenSet;

    Aws::String m_masterUserPassword;
    bool m_masterUserPasswordHasBeenSet;

    Aws::String m_masterUsername;
    bool m_masterUsernameHasBeenSet;

    VpcConfig m_vpcConfig;
    bool m_vpcConfigHasBeenSet;
  };

} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
