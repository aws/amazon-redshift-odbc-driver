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
  enum class ConfigStatusString
  {
    NOT_SET,
    AVAILABLE,
    MODIFYING
  };

namespace ConfigStatusStringMapper
{
AWS_REDSHIFTARCADIACORALSERVICE_API ConfigStatusString GetConfigStatusStringForName(const Aws::String& name);

AWS_REDSHIFTARCADIACORALSERVICE_API Aws::String GetNameForConfigStatusString(ConfigStatusString value);
} // namespace ConfigStatusStringMapper
} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
