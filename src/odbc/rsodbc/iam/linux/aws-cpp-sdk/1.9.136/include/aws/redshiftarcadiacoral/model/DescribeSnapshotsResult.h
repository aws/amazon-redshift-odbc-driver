/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralService_EXPORTS.h>
#include <aws/redshiftarcadiacoral/model/SnapshotMessage.h>
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
  class AWS_REDSHIFTARCADIACORALSERVICE_API DescribeSnapshotsResult
  {
  public:
    DescribeSnapshotsResult();
    DescribeSnapshotsResult(const Aws::AmazonWebServiceResult<Aws::Utils::Json::JsonValue>& result);
    DescribeSnapshotsResult& operator=(const Aws::AmazonWebServiceResult<Aws::Utils::Json::JsonValue>& result);


    
    inline const SnapshotMessage& GetSnapshotMessage() const{ return m_snapshotMessage; }

    
    inline void SetSnapshotMessage(const SnapshotMessage& value) { m_snapshotMessage = value; }

    
    inline void SetSnapshotMessage(SnapshotMessage&& value) { m_snapshotMessage = std::move(value); }

    
    inline DescribeSnapshotsResult& WithSnapshotMessage(const SnapshotMessage& value) { SetSnapshotMessage(value); return *this;}

    
    inline DescribeSnapshotsResult& WithSnapshotMessage(SnapshotMessage&& value) { SetSnapshotMessage(std::move(value)); return *this;}

  private:

    SnapshotMessage m_snapshotMessage;
  };

} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
