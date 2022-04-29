/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralService_EXPORTS.h>
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralServiceRequest.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/redshiftarcadiacoral/model/DataShareStatus.h>
#include <utility>

namespace Aws
{
namespace RedshiftArcadiaCoralService
{
namespace Model
{

  /**
   */
  class AWS_REDSHIFTARCADIACORALSERVICE_API DescribeDataSharesForProducerRequest : public RedshiftArcadiaCoralServiceRequest
  {
  public:
    DescribeDataSharesForProducerRequest();

    // Service request name is the Operation name which will send this request out,
    // each operation should has unique request name, so that we can get operation's name from this request.
    // Note: this is not true for response, multiple operations may have the same response name,
    // so we can not get operation's name from response.
    inline virtual const char* GetServiceRequestName() const override { return "DescribeDataSharesForProducer"; }

    Aws::String SerializePayload() const override;

    Aws::Http::HeaderValueCollection GetRequestSpecificHeaders() const override;


    
    inline const Aws::String& GetMarker() const{ return m_marker; }

    
    inline bool MarkerHasBeenSet() const { return m_markerHasBeenSet; }

    
    inline void SetMarker(const Aws::String& value) { m_markerHasBeenSet = true; m_marker = value; }

    
    inline void SetMarker(Aws::String&& value) { m_markerHasBeenSet = true; m_marker = std::move(value); }

    
    inline void SetMarker(const char* value) { m_markerHasBeenSet = true; m_marker.assign(value); }

    
    inline DescribeDataSharesForProducerRequest& WithMarker(const Aws::String& value) { SetMarker(value); return *this;}

    
    inline DescribeDataSharesForProducerRequest& WithMarker(Aws::String&& value) { SetMarker(std::move(value)); return *this;}

    
    inline DescribeDataSharesForProducerRequest& WithMarker(const char* value) { SetMarker(value); return *this;}


    
    inline int GetMaxRecords() const{ return m_maxRecords; }

    
    inline bool MaxRecordsHasBeenSet() const { return m_maxRecordsHasBeenSet; }

    
    inline void SetMaxRecords(int value) { m_maxRecordsHasBeenSet = true; m_maxRecords = value; }

    
    inline DescribeDataSharesForProducerRequest& WithMaxRecords(int value) { SetMaxRecords(value); return *this;}


    
    inline const Aws::String& GetProducerArn() const{ return m_producerArn; }

    
    inline bool ProducerArnHasBeenSet() const { return m_producerArnHasBeenSet; }

    
    inline void SetProducerArn(const Aws::String& value) { m_producerArnHasBeenSet = true; m_producerArn = value; }

    
    inline void SetProducerArn(Aws::String&& value) { m_producerArnHasBeenSet = true; m_producerArn = std::move(value); }

    
    inline void SetProducerArn(const char* value) { m_producerArnHasBeenSet = true; m_producerArn.assign(value); }

    
    inline DescribeDataSharesForProducerRequest& WithProducerArn(const Aws::String& value) { SetProducerArn(value); return *this;}

    
    inline DescribeDataSharesForProducerRequest& WithProducerArn(Aws::String&& value) { SetProducerArn(std::move(value)); return *this;}

    
    inline DescribeDataSharesForProducerRequest& WithProducerArn(const char* value) { SetProducerArn(value); return *this;}


    
    inline const DataShareStatus& GetStatus() const{ return m_status; }

    
    inline bool StatusHasBeenSet() const { return m_statusHasBeenSet; }

    
    inline void SetStatus(const DataShareStatus& value) { m_statusHasBeenSet = true; m_status = value; }

    
    inline void SetStatus(DataShareStatus&& value) { m_statusHasBeenSet = true; m_status = std::move(value); }

    
    inline DescribeDataSharesForProducerRequest& WithStatus(const DataShareStatus& value) { SetStatus(value); return *this;}

    
    inline DescribeDataSharesForProducerRequest& WithStatus(DataShareStatus&& value) { SetStatus(std::move(value)); return *this;}

  private:

    Aws::String m_marker;
    bool m_markerHasBeenSet;

    int m_maxRecords;
    bool m_maxRecordsHasBeenSet;

    Aws::String m_producerArn;
    bool m_producerArnHasBeenSet;

    DataShareStatus m_status;
    bool m_statusHasBeenSet;
  };

} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
