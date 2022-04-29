/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralService_EXPORTS.h>
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
  class AWS_REDSHIFTARCADIACORALSERVICE_API ModifyUsageLimitResult
  {
  public:
    ModifyUsageLimitResult();
    ModifyUsageLimitResult(const Aws::AmazonWebServiceResult<Aws::Utils::Json::JsonValue>& result);
    ModifyUsageLimitResult& operator=(const Aws::AmazonWebServiceResult<Aws::Utils::Json::JsonValue>& result);


    
    inline const UsageLimit& GetUsageLimit() const{ return m_usageLimit; }

    
    inline void SetUsageLimit(const UsageLimit& value) { m_usageLimit = value; }

    
    inline void SetUsageLimit(UsageLimit&& value) { m_usageLimit = std::move(value); }

    
    inline ModifyUsageLimitResult& WithUsageLimit(const UsageLimit& value) { SetUsageLimit(value); return *this;}

    
    inline ModifyUsageLimitResult& WithUsageLimit(UsageLimit&& value) { SetUsageLimit(std::move(value)); return *this;}

  private:

    UsageLimit m_usageLimit;
  };

} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
