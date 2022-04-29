/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralService_EXPORTS.h>
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralServiceRequest.h>
#include <aws/redshiftarcadiacoral/model/UsageLimitFeatureType.h>
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
  class AWS_REDSHIFTARCADIACORALSERVICE_API DescribeUsageLimitRequest : public RedshiftArcadiaCoralServiceRequest
  {
  public:
    DescribeUsageLimitRequest();

    // Service request name is the Operation name which will send this request out,
    // each operation should has unique request name, so that we can get operation's name from this request.
    // Note: this is not true for response, multiple operations may have the same response name,
    // so we can not get operation's name from response.
    inline virtual const char* GetServiceRequestName() const override { return "DescribeUsageLimit"; }

    Aws::String SerializePayload() const override;

    Aws::Http::HeaderValueCollection GetRequestSpecificHeaders() const override;


    
    inline const UsageLimitFeatureType& GetFeatureType() const{ return m_featureType; }

    
    inline bool FeatureTypeHasBeenSet() const { return m_featureTypeHasBeenSet; }

    
    inline void SetFeatureType(const UsageLimitFeatureType& value) { m_featureTypeHasBeenSet = true; m_featureType = value; }

    
    inline void SetFeatureType(UsageLimitFeatureType&& value) { m_featureTypeHasBeenSet = true; m_featureType = std::move(value); }

    
    inline DescribeUsageLimitRequest& WithFeatureType(const UsageLimitFeatureType& value) { SetFeatureType(value); return *this;}

    
    inline DescribeUsageLimitRequest& WithFeatureType(UsageLimitFeatureType&& value) { SetFeatureType(std::move(value)); return *this;}


    
    inline const Aws::String& GetMarker() const{ return m_marker; }

    
    inline bool MarkerHasBeenSet() const { return m_markerHasBeenSet; }

    
    inline void SetMarker(const Aws::String& value) { m_markerHasBeenSet = true; m_marker = value; }

    
    inline void SetMarker(Aws::String&& value) { m_markerHasBeenSet = true; m_marker = std::move(value); }

    
    inline void SetMarker(const char* value) { m_markerHasBeenSet = true; m_marker.assign(value); }

    
    inline DescribeUsageLimitRequest& WithMarker(const Aws::String& value) { SetMarker(value); return *this;}

    
    inline DescribeUsageLimitRequest& WithMarker(Aws::String&& value) { SetMarker(std::move(value)); return *this;}

    
    inline DescribeUsageLimitRequest& WithMarker(const char* value) { SetMarker(value); return *this;}


    
    inline int GetMaxRecords() const{ return m_maxRecords; }

    
    inline bool MaxRecordsHasBeenSet() const { return m_maxRecordsHasBeenSet; }

    
    inline void SetMaxRecords(int value) { m_maxRecordsHasBeenSet = true; m_maxRecords = value; }

    
    inline DescribeUsageLimitRequest& WithMaxRecords(int value) { SetMaxRecords(value); return *this;}


    
    inline const Aws::String& GetUsageLimitId() const{ return m_usageLimitId; }

    
    inline bool UsageLimitIdHasBeenSet() const { return m_usageLimitIdHasBeenSet; }

    
    inline void SetUsageLimitId(const Aws::String& value) { m_usageLimitIdHasBeenSet = true; m_usageLimitId = value; }

    
    inline void SetUsageLimitId(Aws::String&& value) { m_usageLimitIdHasBeenSet = true; m_usageLimitId = std::move(value); }

    
    inline void SetUsageLimitId(const char* value) { m_usageLimitIdHasBeenSet = true; m_usageLimitId.assign(value); }

    
    inline DescribeUsageLimitRequest& WithUsageLimitId(const Aws::String& value) { SetUsageLimitId(value); return *this;}

    
    inline DescribeUsageLimitRequest& WithUsageLimitId(Aws::String&& value) { SetUsageLimitId(std::move(value)); return *this;}

    
    inline DescribeUsageLimitRequest& WithUsageLimitId(const char* value) { SetUsageLimitId(value); return *this;}

  private:

    UsageLimitFeatureType m_featureType;
    bool m_featureTypeHasBeenSet;

    Aws::String m_marker;
    bool m_markerHasBeenSet;

    int m_maxRecords;
    bool m_maxRecordsHasBeenSet;

    Aws::String m_usageLimitId;
    bool m_usageLimitIdHasBeenSet;
  };

} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
