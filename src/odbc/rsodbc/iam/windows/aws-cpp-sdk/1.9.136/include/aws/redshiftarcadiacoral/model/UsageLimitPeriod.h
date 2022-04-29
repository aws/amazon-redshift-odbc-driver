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
  enum class UsageLimitPeriod
  {
    NOT_SET,
    daily,
    weekly,
    monthly
  };

namespace UsageLimitPeriodMapper
{
AWS_REDSHIFTARCADIACORALSERVICE_API UsageLimitPeriod GetUsageLimitPeriodForName(const Aws::String& name);

AWS_REDSHIFTARCADIACORALSERVICE_API Aws::String GetNameForUsageLimitPeriod(UsageLimitPeriod value);
} // namespace UsageLimitPeriodMapper
} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
