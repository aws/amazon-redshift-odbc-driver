/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralService_EXPORTS.h>
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralServiceRequest.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <utility>

namespace Aws
{
namespace RedshiftArcadiaCoralService
{
namespace Model
{

  /**
   */
  class AWS_REDSHIFTARCADIACORALSERVICE_API DescribeDataSharesRequest : public RedshiftArcadiaCoralServiceRequest
  {
  public:
    DescribeDataSharesRequest();

    // Service request name is the Operation name which will send this request out,
    // each operation should has unique request name, so that we can get operation's name from this request.
    // Note: this is not true for response, multiple operations may have the same response name,
    // so we can not get operation's name from response.
    inline virtual const char* GetServiceRequestName() const override { return "DescribeDataShares"; }

    Aws::String SerializePayload() const override;

    Aws::Http::HeaderValueCollection GetRequestSpecificHeaders() const override;


    
    inline const Aws::String& GetDataShareArn() const{ return m_dataShareArn; }

    
    inline bool DataShareArnHasBeenSet() const { return m_dataShareArnHasBeenSet; }

    
    inline void SetDataShareArn(const Aws::String& value) { m_dataShareArnHasBeenSet = true; m_dataShareArn = value; }

    
    inline void SetDataShareArn(Aws::String&& value) { m_dataShareArnHasBeenSet = true; m_dataShareArn = std::move(value); }

    
    inline void SetDataShareArn(const char* value) { m_dataShareArnHasBeenSet = true; m_dataShareArn.assign(value); }

    
    inline DescribeDataSharesRequest& WithDataShareArn(const Aws::String& value) { SetDataShareArn(value); return *this;}

    
    inline DescribeDataSharesRequest& WithDataShareArn(Aws::String&& value) { SetDataShareArn(std::move(value)); return *this;}

    
    inline DescribeDataSharesRequest& WithDataShareArn(const char* value) { SetDataShareArn(value); return *this;}


    
    inline const Aws::String& GetMarker() const{ return m_marker; }

    
    inline bool MarkerHasBeenSet() const { return m_markerHasBeenSet; }

    
    inline void SetMarker(const Aws::String& value) { m_markerHasBeenSet = true; m_marker = value; }

    
    inline void SetMarker(Aws::String&& value) { m_markerHasBeenSet = true; m_marker = std::move(value); }

    
    inline void SetMarker(const char* value) { m_markerHasBeenSet = true; m_marker.assign(value); }

    
    inline DescribeDataSharesRequest& WithMarker(const Aws::String& value) { SetMarker(value); return *this;}

    
    inline DescribeDataSharesRequest& WithMarker(Aws::String&& value) { SetMarker(std::move(value)); return *this;}

    
    inline DescribeDataSharesRequest& WithMarker(const char* value) { SetMarker(value); return *this;}


    
    inline int GetMaxRecords() const{ return m_maxRecords; }

    
    inline bool MaxRecordsHasBeenSet() const { return m_maxRecordsHasBeenSet; }

    
    inline void SetMaxRecords(int value) { m_maxRecordsHasBeenSet = true; m_maxRecords = value; }

    
    inline DescribeDataSharesRequest& WithMaxRecords(int value) { SetMaxRecords(value); return *this;}

  private:

    Aws::String m_dataShareArn;
    bool m_dataShareArnHasBeenSet;

    Aws::String m_marker;
    bool m_markerHasBeenSet;

    int m_maxRecords;
    bool m_maxRecordsHasBeenSet;
  };

} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
