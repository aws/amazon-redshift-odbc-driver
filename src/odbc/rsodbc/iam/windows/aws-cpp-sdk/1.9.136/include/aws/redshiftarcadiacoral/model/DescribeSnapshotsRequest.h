/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralService_EXPORTS.h>
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralServiceRequest.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/core/utils/DateTime.h>
#include <aws/core/utils/memory/stl/AWSVector.h>
#include <aws/redshiftarcadiacoral/model/SnapshotSortingEntity.h>
#include <utility>

namespace Aws
{
namespace RedshiftArcadiaCoralService
{
namespace Model
{

  /**
   */
  class AWS_REDSHIFTARCADIACORALSERVICE_API DescribeSnapshotsRequest : public RedshiftArcadiaCoralServiceRequest
  {
  public:
    DescribeSnapshotsRequest();

    // Service request name is the Operation name which will send this request out,
    // each operation should has unique request name, so that we can get operation's name from this request.
    // Note: this is not true for response, multiple operations may have the same response name,
    // so we can not get operation's name from response.
    inline virtual const char* GetServiceRequestName() const override { return "DescribeSnapshots"; }

    Aws::String SerializePayload() const override;

    Aws::Http::HeaderValueCollection GetRequestSpecificHeaders() const override;


    
    inline const Aws::String& GetDbName() const{ return m_dbName; }

    
    inline bool DbNameHasBeenSet() const { return m_dbNameHasBeenSet; }

    
    inline void SetDbName(const Aws::String& value) { m_dbNameHasBeenSet = true; m_dbName = value; }

    
    inline void SetDbName(Aws::String&& value) { m_dbNameHasBeenSet = true; m_dbName = std::move(value); }

    
    inline void SetDbName(const char* value) { m_dbNameHasBeenSet = true; m_dbName.assign(value); }

    
    inline DescribeSnapshotsRequest& WithDbName(const Aws::String& value) { SetDbName(value); return *this;}

    
    inline DescribeSnapshotsRequest& WithDbName(Aws::String&& value) { SetDbName(std::move(value)); return *this;}

    
    inline DescribeSnapshotsRequest& WithDbName(const char* value) { SetDbName(value); return *this;}


    
    inline const Aws::Utils::DateTime& GetEndTime() const{ return m_endTime; }

    
    inline bool EndTimeHasBeenSet() const { return m_endTimeHasBeenSet; }

    
    inline void SetEndTime(const Aws::Utils::DateTime& value) { m_endTimeHasBeenSet = true; m_endTime = value; }

    
    inline void SetEndTime(Aws::Utils::DateTime&& value) { m_endTimeHasBeenSet = true; m_endTime = std::move(value); }

    
    inline DescribeSnapshotsRequest& WithEndTime(const Aws::Utils::DateTime& value) { SetEndTime(value); return *this;}

    
    inline DescribeSnapshotsRequest& WithEndTime(Aws::Utils::DateTime&& value) { SetEndTime(std::move(value)); return *this;}


    
    inline const Aws::String& GetMarker() const{ return m_marker; }

    
    inline bool MarkerHasBeenSet() const { return m_markerHasBeenSet; }

    
    inline void SetMarker(const Aws::String& value) { m_markerHasBeenSet = true; m_marker = value; }

    
    inline void SetMarker(Aws::String&& value) { m_markerHasBeenSet = true; m_marker = std::move(value); }

    
    inline void SetMarker(const char* value) { m_markerHasBeenSet = true; m_marker.assign(value); }

    
    inline DescribeSnapshotsRequest& WithMarker(const Aws::String& value) { SetMarker(value); return *this;}

    
    inline DescribeSnapshotsRequest& WithMarker(Aws::String&& value) { SetMarker(std::move(value)); return *this;}

    
    inline DescribeSnapshotsRequest& WithMarker(const char* value) { SetMarker(value); return *this;}


    
    inline int GetMaxRecords() const{ return m_maxRecords; }

    
    inline bool MaxRecordsHasBeenSet() const { return m_maxRecordsHasBeenSet; }

    
    inline void SetMaxRecords(int value) { m_maxRecordsHasBeenSet = true; m_maxRecords = value; }

    
    inline DescribeSnapshotsRequest& WithMaxRecords(int value) { SetMaxRecords(value); return *this;}


    
    inline const Aws::String& GetOwnerAccount() const{ return m_ownerAccount; }

    
    inline bool OwnerAccountHasBeenSet() const { return m_ownerAccountHasBeenSet; }

    
    inline void SetOwnerAccount(const Aws::String& value) { m_ownerAccountHasBeenSet = true; m_ownerAccount = value; }

    
    inline void SetOwnerAccount(Aws::String&& value) { m_ownerAccountHasBeenSet = true; m_ownerAccount = std::move(value); }

    
    inline void SetOwnerAccount(const char* value) { m_ownerAccountHasBeenSet = true; m_ownerAccount.assign(value); }

    
    inline DescribeSnapshotsRequest& WithOwnerAccount(const Aws::String& value) { SetOwnerAccount(value); return *this;}

    
    inline DescribeSnapshotsRequest& WithOwnerAccount(Aws::String&& value) { SetOwnerAccount(std::move(value)); return *this;}

    
    inline DescribeSnapshotsRequest& WithOwnerAccount(const char* value) { SetOwnerAccount(value); return *this;}


    
    inline const Aws::String& GetSnapshotIdentifier() const{ return m_snapshotIdentifier; }

    
    inline bool SnapshotIdentifierHasBeenSet() const { return m_snapshotIdentifierHasBeenSet; }

    
    inline void SetSnapshotIdentifier(const Aws::String& value) { m_snapshotIdentifierHasBeenSet = true; m_snapshotIdentifier = value; }

    
    inline void SetSnapshotIdentifier(Aws::String&& value) { m_snapshotIdentifierHasBeenSet = true; m_snapshotIdentifier = std::move(value); }

    
    inline void SetSnapshotIdentifier(const char* value) { m_snapshotIdentifierHasBeenSet = true; m_snapshotIdentifier.assign(value); }

    
    inline DescribeSnapshotsRequest& WithSnapshotIdentifier(const Aws::String& value) { SetSnapshotIdentifier(value); return *this;}

    
    inline DescribeSnapshotsRequest& WithSnapshotIdentifier(Aws::String&& value) { SetSnapshotIdentifier(std::move(value)); return *this;}

    
    inline DescribeSnapshotsRequest& WithSnapshotIdentifier(const char* value) { SetSnapshotIdentifier(value); return *this;}


    
    inline const Aws::Vector<SnapshotSortingEntity>& GetSortingEntities() const{ return m_sortingEntities; }

    
    inline bool SortingEntitiesHasBeenSet() const { return m_sortingEntitiesHasBeenSet; }

    
    inline void SetSortingEntities(const Aws::Vector<SnapshotSortingEntity>& value) { m_sortingEntitiesHasBeenSet = true; m_sortingEntities = value; }

    
    inline void SetSortingEntities(Aws::Vector<SnapshotSortingEntity>&& value) { m_sortingEntitiesHasBeenSet = true; m_sortingEntities = std::move(value); }

    
    inline DescribeSnapshotsRequest& WithSortingEntities(const Aws::Vector<SnapshotSortingEntity>& value) { SetSortingEntities(value); return *this;}

    
    inline DescribeSnapshotsRequest& WithSortingEntities(Aws::Vector<SnapshotSortingEntity>&& value) { SetSortingEntities(std::move(value)); return *this;}

    
    inline DescribeSnapshotsRequest& AddSortingEntities(const SnapshotSortingEntity& value) { m_sortingEntitiesHasBeenSet = true; m_sortingEntities.push_back(value); return *this; }

    
    inline DescribeSnapshotsRequest& AddSortingEntities(SnapshotSortingEntity&& value) { m_sortingEntitiesHasBeenSet = true; m_sortingEntities.push_back(std::move(value)); return *this; }


    
    inline const Aws::Utils::DateTime& GetStartTime() const{ return m_startTime; }

    
    inline bool StartTimeHasBeenSet() const { return m_startTimeHasBeenSet; }

    
    inline void SetStartTime(const Aws::Utils::DateTime& value) { m_startTimeHasBeenSet = true; m_startTime = value; }

    
    inline void SetStartTime(Aws::Utils::DateTime&& value) { m_startTimeHasBeenSet = true; m_startTime = std::move(value); }

    
    inline DescribeSnapshotsRequest& WithStartTime(const Aws::Utils::DateTime& value) { SetStartTime(value); return *this;}

    
    inline DescribeSnapshotsRequest& WithStartTime(Aws::Utils::DateTime&& value) { SetStartTime(std::move(value)); return *this;}


    
    inline const Aws::Vector<Aws::String>& GetTagKeys() const{ return m_tagKeys; }

    
    inline bool TagKeysHasBeenSet() const { return m_tagKeysHasBeenSet; }

    
    inline void SetTagKeys(const Aws::Vector<Aws::String>& value) { m_tagKeysHasBeenSet = true; m_tagKeys = value; }

    
    inline void SetTagKeys(Aws::Vector<Aws::String>&& value) { m_tagKeysHasBeenSet = true; m_tagKeys = std::move(value); }

    
    inline DescribeSnapshotsRequest& WithTagKeys(const Aws::Vector<Aws::String>& value) { SetTagKeys(value); return *this;}

    
    inline DescribeSnapshotsRequest& WithTagKeys(Aws::Vector<Aws::String>&& value) { SetTagKeys(std::move(value)); return *this;}

    
    inline DescribeSnapshotsRequest& AddTagKeys(const Aws::String& value) { m_tagKeysHasBeenSet = true; m_tagKeys.push_back(value); return *this; }

    
    inline DescribeSnapshotsRequest& AddTagKeys(Aws::String&& value) { m_tagKeysHasBeenSet = true; m_tagKeys.push_back(std::move(value)); return *this; }

    
    inline DescribeSnapshotsRequest& AddTagKeys(const char* value) { m_tagKeysHasBeenSet = true; m_tagKeys.push_back(value); return *this; }


    
    inline const Aws::Vector<Aws::String>& GetTagValues() const{ return m_tagValues; }

    
    inline bool TagValuesHasBeenSet() const { return m_tagValuesHasBeenSet; }

    
    inline void SetTagValues(const Aws::Vector<Aws::String>& value) { m_tagValuesHasBeenSet = true; m_tagValues = value; }

    
    inline void SetTagValues(Aws::Vector<Aws::String>&& value) { m_tagValuesHasBeenSet = true; m_tagValues = std::move(value); }

    
    inline DescribeSnapshotsRequest& WithTagValues(const Aws::Vector<Aws::String>& value) { SetTagValues(value); return *this;}

    
    inline DescribeSnapshotsRequest& WithTagValues(Aws::Vector<Aws::String>&& value) { SetTagValues(std::move(value)); return *this;}

    
    inline DescribeSnapshotsRequest& AddTagValues(const Aws::String& value) { m_tagValuesHasBeenSet = true; m_tagValues.push_back(value); return *this; }

    
    inline DescribeSnapshotsRequest& AddTagValues(Aws::String&& value) { m_tagValuesHasBeenSet = true; m_tagValues.push_back(std::move(value)); return *this; }

    
    inline DescribeSnapshotsRequest& AddTagValues(const char* value) { m_tagValuesHasBeenSet = true; m_tagValues.push_back(value); return *this; }

  private:

    Aws::String m_dbName;
    bool m_dbNameHasBeenSet;

    Aws::Utils::DateTime m_endTime;
    bool m_endTimeHasBeenSet;

    Aws::String m_marker;
    bool m_markerHasBeenSet;

    int m_maxRecords;
    bool m_maxRecordsHasBeenSet;

    Aws::String m_ownerAccount;
    bool m_ownerAccountHasBeenSet;

    Aws::String m_snapshotIdentifier;
    bool m_snapshotIdentifierHasBeenSet;

    Aws::Vector<SnapshotSortingEntity> m_sortingEntities;
    bool m_sortingEntitiesHasBeenSet;

    Aws::Utils::DateTime m_startTime;
    bool m_startTimeHasBeenSet;

    Aws::Vector<Aws::String> m_tagKeys;
    bool m_tagKeysHasBeenSet;

    Aws::Vector<Aws::String> m_tagValues;
    bool m_tagValuesHasBeenSet;
  };

} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
