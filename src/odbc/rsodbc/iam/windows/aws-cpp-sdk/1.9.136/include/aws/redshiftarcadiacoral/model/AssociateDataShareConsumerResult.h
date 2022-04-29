/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralService_EXPORTS.h>
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
  class AWS_REDSHIFTARCADIACORALSERVICE_API AssociateDataShareConsumerResult
  {
  public:
    AssociateDataShareConsumerResult();
    AssociateDataShareConsumerResult(const Aws::AmazonWebServiceResult<Aws::Utils::Json::JsonValue>& result);
    AssociateDataShareConsumerResult& operator=(const Aws::AmazonWebServiceResult<Aws::Utils::Json::JsonValue>& result);


    
    inline const DataShare& GetDataShare() const{ return m_dataShare; }

    
    inline void SetDataShare(const DataShare& value) { m_dataShare = value; }

    
    inline void SetDataShare(DataShare&& value) { m_dataShare = std::move(value); }

    
    inline AssociateDataShareConsumerResult& WithDataShare(const DataShare& value) { SetDataShare(value); return *this;}

    
    inline AssociateDataShareConsumerResult& WithDataShare(DataShare&& value) { SetDataShare(std::move(value)); return *this;}

  private:

    DataShare m_dataShare;
  };

} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
