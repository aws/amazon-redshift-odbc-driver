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
  enum class DataShareStatus
  {
    NOT_SET,
    ACTIVE,
    PENDING_AUTHORIZATION,
    DEAUTHORIZED,
    REJECTED,
    AVAILABLE,
    AUTHORIZED
  };

namespace DataShareStatusMapper
{
AWS_REDSHIFTARCADIACORALSERVICE_API DataShareStatus GetDataShareStatusForName(const Aws::String& name);

AWS_REDSHIFTARCADIACORALSERVICE_API Aws::String GetNameForDataShareStatus(DataShareStatus value);
} // namespace DataShareStatusMapper
} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
