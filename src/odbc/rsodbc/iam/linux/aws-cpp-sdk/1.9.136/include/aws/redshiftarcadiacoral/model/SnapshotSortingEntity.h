/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralService_EXPORTS.h>
#include <aws/redshiftarcadiacoral/model/SnapshotAttributeToSortBy.h>
#include <aws/redshiftarcadiacoral/model/SortByOrder.h>
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

  class AWS_REDSHIFTARCADIACORALSERVICE_API SnapshotSortingEntity
  {
  public:
    SnapshotSortingEntity();
    SnapshotSortingEntity(Aws::Utils::Json::JsonView jsonValue);
    SnapshotSortingEntity& operator=(Aws::Utils::Json::JsonView jsonValue);
    Aws::Utils::Json::JsonValue Jsonize() const;


    
    inline const SnapshotAttributeToSortBy& GetAttribute() const{ return m_attribute; }

    
    inline bool AttributeHasBeenSet() const { return m_attributeHasBeenSet; }

    
    inline void SetAttribute(const SnapshotAttributeToSortBy& value) { m_attributeHasBeenSet = true; m_attribute = value; }

    
    inline void SetAttribute(SnapshotAttributeToSortBy&& value) { m_attributeHasBeenSet = true; m_attribute = std::move(value); }

    
    inline SnapshotSortingEntity& WithAttribute(const SnapshotAttributeToSortBy& value) { SetAttribute(value); return *this;}

    
    inline SnapshotSortingEntity& WithAttribute(SnapshotAttributeToSortBy&& value) { SetAttribute(std::move(value)); return *this;}


    
    inline const SortByOrder& GetSortOrder() const{ return m_sortOrder; }

    
    inline bool SortOrderHasBeenSet() const { return m_sortOrderHasBeenSet; }

    
    inline void SetSortOrder(const SortByOrder& value) { m_sortOrderHasBeenSet = true; m_sortOrder = value; }

    
    inline void SetSortOrder(SortByOrder&& value) { m_sortOrderHasBeenSet = true; m_sortOrder = std::move(value); }

    
    inline SnapshotSortingEntity& WithSortOrder(const SortByOrder& value) { SetSortOrder(value); return *this;}

    
    inline SnapshotSortingEntity& WithSortOrder(SortByOrder&& value) { SetSortOrder(std::move(value)); return *this;}

  private:

    SnapshotAttributeToSortBy m_attribute;
    bool m_attributeHasBeenSet;

    SortByOrder m_sortOrder;
    bool m_sortOrderHasBeenSet;
  };

} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
