/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralService_EXPORTS.h>
#include <aws/redshiftarcadiacoral/model/IamRoleApplyStatusString.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <utility>

namespace Aws
{
namespace Utils
{
namespace Json
{
  class JsonValue;
  class JsonView;
} // namespace Json
} // namespace Utils
namespace RedshiftArcadiaCoralService
{
namespace Model
{

  class AWS_REDSHIFTARCADIACORALSERVICE_API IamRole
  {
  public:
    IamRole();
    IamRole(Aws::Utils::Json::JsonView jsonValue);
    IamRole& operator=(Aws::Utils::Json::JsonView jsonValue);
    Aws::Utils::Json::JsonValue Jsonize() const;


    
    inline const IamRoleApplyStatusString& GetApplyStatus() const{ return m_applyStatus; }

    
    inline bool ApplyStatusHasBeenSet() const { return m_applyStatusHasBeenSet; }

    
    inline void SetApplyStatus(const IamRoleApplyStatusString& value) { m_applyStatusHasBeenSet = true; m_applyStatus = value; }

    
    inline void SetApplyStatus(IamRoleApplyStatusString&& value) { m_applyStatusHasBeenSet = true; m_applyStatus = std::move(value); }

    
    inline IamRole& WithApplyStatus(const IamRoleApplyStatusString& value) { SetApplyStatus(value); return *this;}

    
    inline IamRole& WithApplyStatus(IamRoleApplyStatusString&& value) { SetApplyStatus(std::move(value)); return *this;}


    
    inline const Aws::String& GetIamRoleArn() const{ return m_iamRoleArn; }

    
    inline bool IamRoleArnHasBeenSet() const { return m_iamRoleArnHasBeenSet; }

    
    inline void SetIamRoleArn(const Aws::String& value) { m_iamRoleArnHasBeenSet = true; m_iamRoleArn = value; }

    
    inline void SetIamRoleArn(Aws::String&& value) { m_iamRoleArnHasBeenSet = true; m_iamRoleArn = std::move(value); }

    
    inline void SetIamRoleArn(const char* value) { m_iamRoleArnHasBeenSet = true; m_iamRoleArn.assign(value); }

    
    inline IamRole& WithIamRoleArn(const Aws::String& value) { SetIamRoleArn(value); return *this;}

    
    inline IamRole& WithIamRoleArn(Aws::String&& value) { SetIamRoleArn(std::move(value)); return *this;}

    
    inline IamRole& WithIamRoleArn(const char* value) { SetIamRoleArn(value); return *this;}

  private:

    IamRoleApplyStatusString m_applyStatus;
    bool m_applyStatusHasBeenSet;

    Aws::String m_iamRoleArn;
    bool m_iamRoleArnHasBeenSet;
  };

} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
