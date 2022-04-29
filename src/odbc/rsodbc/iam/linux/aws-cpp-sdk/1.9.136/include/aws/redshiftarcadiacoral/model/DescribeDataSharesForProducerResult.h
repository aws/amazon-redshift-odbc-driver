/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralService_EXPORTS.h>
#include <aws/core/utils/memory/stl/AWSVector.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/redshiftarcadiacoral/model/DataShare.h>
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
  class AWS_REDSHIFTARCADIACORALSERVICE_API DescribeDataSharesForProducerResult
  {
  public:
    DescribeDataSharesForProducerResult();
    DescribeDataSharesForProducerResult(const Aws::AmazonWebServiceResult<Aws::Utils::Json::JsonValue>& result);
    DescribeDataSharesForProducerResult& operator=(const Aws::AmazonWebServiceResult<Aws::Utils::Json::JsonValue>& result);


    
    inline const Aws::Vector<DataShare>& GetDataShares() const{ return m_dataShares; }

    
    inline void SetDataShares(const Aws::Vector<DataShare>& value) { m_dataShares = value; }

    
    inline void SetDataShares(Aws::Vector<DataShare>&& value) { m_dataShares = std::move(value); }

    
    inline DescribeDataSharesForProducerResult& WithDataShares(const Aws::Vector<DataShare>& value) { SetDataShares(value); return *this;}

    
    inline DescribeDataSharesForProducerResult& WithDataShares(Aws::Vector<DataShare>&& value) { SetDataShares(std::move(value)); return *this;}

    
    inline DescribeDataSharesForProducerResult& AddDataShares(const DataShare& value) { m_dataShares.push_back(value); return *this; }

    
    inline DescribeDataSharesForProducerResult& AddDataShares(DataShare&& value) { m_dataShares.push_back(std::move(value)); return *this; }


    
    inline const Aws::String& GetMarker() const{ return m_marker; }

    
    inline void SetMarker(const Aws::String& value) { m_marker = value; }

    
    inline void SetMarker(Aws::String&& value) { m_marker = std::move(value); }

    
    inline void SetMarker(const char* value) { m_marker.assign(value); }

    
    inline DescribeDataSharesForProducerResult& WithMarker(const Aws::String& value) { SetMarker(value); return *this;}

    
    inline DescribeDataSharesForProducerResult& WithMarker(Aws::String&& value) { SetMarker(std::move(value)); return *this;}

    
    inline DescribeDataSharesForProducerResult& WithMarker(const char* value) { SetMarker(value); return *this;}

  private:

    Aws::Vector<DataShare> m_dataShares;

    Aws::String m_marker;
  };

} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
