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
  enum class UsageLimitBreachAction
  {
    NOT_SET,
    log,
    emit_metric,
    disable
  };

namespace UsageLimitBreachActionMapper
{
AWS_REDSHIFTARCADIACORALSERVICE_API UsageLimitBreachAction GetUsageLimitBreachActionForName(const Aws::String& name);

AWS_REDSHIFTARCADIACORALSERVICE_API Aws::String GetNameForUsageLimitBreachAction(UsageLimitBreachAction value);
} // namespace UsageLimitBreachActionMapper
} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
