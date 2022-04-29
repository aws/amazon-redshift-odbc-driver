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
  enum class SortByOrder
  {
    NOT_SET,
    ASCENDING,
    DESCENDING
  };

namespace SortByOrderMapper
{
AWS_REDSHIFTARCADIACORALSERVICE_API SortByOrder GetSortByOrderForName(const Aws::String& name);

AWS_REDSHIFTARCADIACORALSERVICE_API Aws::String GetNameForSortByOrder(SortByOrder value);
} // namespace SortByOrderMapper
} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
