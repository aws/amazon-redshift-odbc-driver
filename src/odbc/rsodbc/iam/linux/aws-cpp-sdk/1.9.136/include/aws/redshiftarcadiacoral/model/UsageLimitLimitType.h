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
  enum class UsageLimitLimitType
  {
    NOT_SET,
    compute_used,
    data_scanned
  };

namespace UsageLimitLimitTypeMapper
{
AWS_REDSHIFTARCADIACORALSERVICE_API UsageLimitLimitType GetUsageLimitLimitTypeForName(const Aws::String& name);

AWS_REDSHIFTARCADIACORALSERVICE_API Aws::String GetNameForUsageLimitLimitType(UsageLimitLimitType value);
} // namespace UsageLimitLimitTypeMapper
} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
