/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralService_EXPORTS.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/core/utils/memory/stl/AWSVector.h>
#include <aws/redshiftarcadiacoral/model/UsageLimit.h>
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
  class AWS_REDSHIFTARCADIACORALSERVICE_API DescribeUsageLimitResult
  {
  public:
    DescribeUsageLimitResult();
    DescribeUsageLimitResult(const Aws::AmazonWebServiceResult<Aws::Utils::Json::JsonValue>& result);
    DescribeUsageLimitResult& operator=(const Aws::AmazonWebServiceResult<Aws::Utils::Json::JsonValue>& result);


    
    inline const Aws::String& GetMarker() const{ return m_marker; }

    
    inline void SetMarker(const Aws::String& value) { m_marker = value; }

    
    inline void SetMarker(Aws::String&& value) { m_marker = std::move(value); }

    
    inline void SetMarker(const char* value) { m_marker.assign(value); }

    
    inline DescribeUsageLimitResult& WithMarker(const Aws::String& value) { SetMarker(value); return *this;}

    
    inline DescribeUsageLimitResult& WithMarker(Aws::String&& value) { SetMarker(std::move(value)); return *this;}

    
    inline DescribeUsageLimitResult& WithMarker(const char* value) { SetMarker(value); return *this;}


    
    inline const Aws::Vector<UsageLimit>& GetUsageLimits() const{ return m_usageLimits; }

    
    inline void SetUsageLimits(const Aws::Vector<UsageLimit>& value) { m_usageLimits = value; }

    
    inline void SetUsageLimits(Aws::Vector<UsageLimit>&& value) { m_usageLimits = std::move(value); }

    
    inline DescribeUsageLimitResult& WithUsageLimits(const Aws::Vector<UsageLimit>& value) { SetUsageLimits(value); return *this;}

    
    inline DescribeUsageLimitResult& WithUsageLimits(Aws::Vector<UsageLimit>&& value) { SetUsageLimits(std::move(value)); return *this;}

    
    inline DescribeUsageLimitResult& AddUsageLimits(const UsageLimit& value) { m_usageLimits.push_back(value); return *this; }

    
    inline DescribeUsageLimitResult& AddUsageLimits(UsageLimit&& value) { m_usageLimits.push_back(std::move(value)); return *this; }

  private:

    Aws::String m_marker;

    Aws::Vector<UsageLimit> m_usageLimits;
  };

} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
