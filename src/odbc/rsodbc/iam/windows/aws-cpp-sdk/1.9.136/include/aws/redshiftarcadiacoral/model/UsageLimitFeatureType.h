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
  enum class UsageLimitFeatureType
  {
    NOT_SET,
    serverless_compute,
    cross_region_datasharing
  };

namespace UsageLimitFeatureTypeMapper
{
AWS_REDSHIFTARCADIACORALSERVICE_API UsageLimitFeatureType GetUsageLimitFeatureTypeForName(const Aws::String& name);

AWS_REDSHIFTARCADIACORALSERVICE_API Aws::String GetNameForUsageLimitFeatureType(UsageLimitFeatureType value);
} // namespace UsageLimitFeatureTypeMapper
} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
