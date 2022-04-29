/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralService_EXPORTS.h>
#include <aws/core/utils/memory/stl/AWSString.h>

namespace Aws
{
namespace RedshiftArcadiaCoralService
{
namespace Model
{
  enum class IamRoleApplyStatusString
  {
    NOT_SET,
    IN_SYNC,
    ADDING,
    REMOVING
  };

namespace IamRoleApplyStatusStringMapper
{
AWS_REDSHIFTARCADIACORALSERVICE_API IamRoleApplyStatusString GetIamRoleApplyStatusStringForName(const Aws::String& name);

AWS_REDSHIFTARCADIACORALSERVICE_API Aws::String GetNameForIamRoleApplyStatusString(IamRoleApplyStatusString value);
} // namespace IamRoleApplyStatusStringMapper
} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
