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
  enum class SnapshotAttributeToSortBy
  {
    NOT_SET,
    SOURCE_TYPE,
    TOTAL_SIZE,
    CREATE_TIME
  };

namespace SnapshotAttributeToSortByMapper
{
AWS_REDSHIFTARCADIACORALSERVICE_API SnapshotAttributeToSortBy GetSnapshotAttributeToSortByForName(const Aws::String& name);

AWS_REDSHIFTARCADIACORALSERVICE_API Aws::String GetNameForSnapshotAttributeToSortBy(SnapshotAttributeToSortBy value);
} // namespace SnapshotAttributeToSortByMapper
} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
