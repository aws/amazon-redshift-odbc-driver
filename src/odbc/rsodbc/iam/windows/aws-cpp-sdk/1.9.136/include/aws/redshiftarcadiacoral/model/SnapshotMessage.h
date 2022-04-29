/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralService_EXPORTS.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/core/utils/memory/stl/AWSVector.h>
#include <aws/redshiftarcadiacoral/model/Snapshot.h>
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

  class AWS_REDSHIFTARCADIACORALSERVICE_API SnapshotMessage
  {
  public:
    SnapshotMessage();
    SnapshotMessage(Aws::Utils::Json::JsonView jsonValue);
    SnapshotMessage& operator=(Aws::Utils::Json::JsonView jsonValue);
    Aws::Utils::Json::JsonValue Jsonize() const;


    
    inline const Aws::String& GetMarker() const{ return m_marker; }

    
    inline bool MarkerHasBeenSet() const { return m_markerHasBeenSet; }

    
    inline void SetMarker(const Aws::String& value) { m_markerHasBeenSet = true; m_marker = value; }

    
    inline void SetMarker(Aws::String&& value) { m_markerHasBeenSet = true; m_marker = std::move(value); }

    
    inline void SetMarker(const char* value) { m_markerHasBeenSet = true; m_marker.assign(value); }

    
    inline SnapshotMessage& WithMarker(const Aws::String& value) { SetMarker(value); return *this;}

    
    inline SnapshotMessage& WithMarker(Aws::String&& value) { SetMarker(std::move(value)); return *this;}

    
    inline SnapshotMessage& WithMarker(const char* value) { SetMarker(value); return *this;}


    
    inline const Aws::Vector<Snapshot>& GetSnapshots() const{ return m_snapshots; }

    
    inline bool SnapshotsHasBeenSet() const { return m_snapshotsHasBeenSet; }

    
    inline void SetSnapshots(const Aws::Vector<Snapshot>& value) { m_snapshotsHasBeenSet = true; m_snapshots = value; }

    
    inline void SetSnapshots(Aws::Vector<Snapshot>&& value) { m_snapshotsHasBeenSet = true; m_snapshots = std::move(value); }

    
    inline SnapshotMessage& WithSnapshots(const Aws::Vector<Snapshot>& value) { SetSnapshots(value); return *this;}

    
    inline SnapshotMessage& WithSnapshots(Aws::Vector<Snapshot>&& value) { SetSnapshots(std::move(value)); return *this;}

    
    inline SnapshotMessage& AddSnapshots(const Snapshot& value) { m_snapshotsHasBeenSet = true; m_snapshots.push_back(value); return *this; }

    
    inline SnapshotMessage& AddSnapshots(Snapshot&& value) { m_snapshotsHasBeenSet = true; m_snapshots.push_back(std::move(value)); return *this; }

  private:

    Aws::String m_marker;
    bool m_markerHasBeenSet;

    Aws::Vector<Snapshot> m_snapshots;
    bool m_snapshotsHasBeenSet;
  };

} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
