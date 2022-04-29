/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralService_EXPORTS.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <utility>

namespace Aws
{
template<typename RESULT_TYPE>
class AmazonWebServiceResult;

namespace Utils
{
namespace Json
{
  class JsonValue;
} // namespace Json
} // namespace Utils
namespace RedshiftArcadiaCoralService
{
namespace Model
{
  class AWS_REDSHIFTARCADIACORALSERVICE_API RestoreFromSnapshotResult
  {
  public:
    RestoreFromSnapshotResult();
    RestoreFromSnapshotResult(const Aws::AmazonWebServiceResult<Aws::Utils::Json::JsonValue>& result);
    RestoreFromSnapshotResult& operator=(const Aws::AmazonWebServiceResult<Aws::Utils::Json::JsonValue>& result);


    
    inline const Aws::String& GetSnapshotId() const{ return m_snapshotId; }

    
    inline void SetSnapshotId(const Aws::String& value) { m_snapshotId = value; }

    
    inline void SetSnapshotId(Aws::String&& value) { m_snapshotId = std::move(value); }

    
    inline void SetSnapshotId(const char* value) { m_snapshotId.assign(value); }

    
    inline RestoreFromSnapshotResult& WithSnapshotId(const Aws::String& value) { SetSnapshotId(value); return *this;}

    
    inline RestoreFromSnapshotResult& WithSnapshotId(Aws::String&& value) { SetSnapshotId(std::move(value)); return *this;}

    
    inline RestoreFromSnapshotResult& WithSnapshotId(const char* value) { SetSnapshotId(value); return *this;}

  private:

    Aws::String m_snapshotId;
  };

} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
