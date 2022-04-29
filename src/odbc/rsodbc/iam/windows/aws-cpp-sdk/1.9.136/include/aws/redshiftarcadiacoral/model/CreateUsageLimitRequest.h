/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralService_EXPORTS.h>
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralServiceRequest.h>
#include <aws/redshiftarcadiacoral/model/UsageLimitBreachAction.h>
#include <aws/redshiftarcadiacoral/model/UsageLimitFeatureType.h>
#include <aws/redshiftarcadiacoral/model/UsageLimitLimitType.h>
#include <aws/redshiftarcadiacoral/model/UsageLimitPeriod.h>
#include <utility>

namespace Aws
{
namespace RedshiftArcadiaCoralService
{
namespace Model
{

  /**
   */
  class AWS_REDSHIFTARCADIACORALSERVICE_API CreateUsageLimitRequest : public RedshiftArcadiaCoralServiceRequest
  {
  public:
    CreateUsageLimitRequest();

    // Service request name is the Operation name which will send this request out,
    // each operation should has unique request name, so that we can get operation's name from this request.
    // Note: this is not true for response, multiple operations may have the same response name,
    // so we can not get operation's name from response.
    inline virtual const char* GetServiceRequestName() const override { return "CreateUsageLimit"; }

    Aws::String SerializePayload() const override;

    Aws::Http::HeaderValueCollection GetRequestSpecificHeaders() const override;


    
    inline long long GetAmount() const{ return m_amount; }

    
    inline bool AmountHasBeenSet() const { return m_amountHasBeenSet; }

    
    inline void SetAmount(long long value) { m_amountHasBeenSet = true; m_amount = value; }

    
    inline CreateUsageLimitRequest& WithAmount(long long value) { SetAmount(value); return *this;}


    
    inline const UsageLimitBreachAction& GetBreachAction() const{ return m_breachAction; }

    
    inline bool BreachActionHasBeenSet() const { return m_breachActionHasBeenSet; }

    
    inline void SetBreachAction(const UsageLimitBreachAction& value) { m_breachActionHasBeenSet = true; m_breachAction = value; }

    
    inline void SetBreachAction(UsageLimitBreachAction&& value) { m_breachActionHasBeenSet = true; m_breachAction = std::move(value); }

    
    inline CreateUsageLimitRequest& WithBreachAction(const UsageLimitBreachAction& value) { SetBreachAction(value); return *this;}

    
    inline CreateUsageLimitRequest& WithBreachAction(UsageLimitBreachAction&& value) { SetBreachAction(std::move(value)); return *this;}


    
    inline const UsageLimitFeatureType& GetFeatureType() const{ return m_featureType; }

    
    inline bool FeatureTypeHasBeenSet() const { return m_featureTypeHasBeenSet; }

    
    inline void SetFeatureType(const UsageLimitFeatureType& value) { m_featureTypeHasBeenSet = true; m_featureType = value; }

    
    inline void SetFeatureType(UsageLimitFeatureType&& value) { m_featureTypeHasBeenSet = true; m_featureType = std::move(value); }

    
    inline CreateUsageLimitRequest& WithFeatureType(const UsageLimitFeatureType& value) { SetFeatureType(value); return *this;}

    
    inline CreateUsageLimitRequest& WithFeatureType(UsageLimitFeatureType&& value) { SetFeatureType(std::move(value)); return *this;}


    
    inline const UsageLimitLimitType& GetLimitType() const{ return m_limitType; }

    
    inline bool LimitTypeHasBeenSet() const { return m_limitTypeHasBeenSet; }

    
    inline void SetLimitType(const UsageLimitLimitType& value) { m_limitTypeHasBeenSet = true; m_limitType = value; }

    
    inline void SetLimitType(UsageLimitLimitType&& value) { m_limitTypeHasBeenSet = true; m_limitType = std::move(value); }

    
    inline CreateUsageLimitRequest& WithLimitType(const UsageLimitLimitType& value) { SetLimitType(value); return *this;}

    
    inline CreateUsageLimitRequest& WithLimitType(UsageLimitLimitType&& value) { SetLimitType(std::move(value)); return *this;}


    
    inline const UsageLimitPeriod& GetPeriod() const{ return m_period; }

    
    inline bool PeriodHasBeenSet() const { return m_periodHasBeenSet; }

    
    inline void SetPeriod(const UsageLimitPeriod& value) { m_periodHasBeenSet = true; m_period = value; }

    
    inline void SetPeriod(UsageLimitPeriod&& value) { m_periodHasBeenSet = true; m_period = std::move(value); }

    
    inline CreateUsageLimitRequest& WithPeriod(const UsageLimitPeriod& value) { SetPeriod(value); return *this;}

    
    inline CreateUsageLimitRequest& WithPeriod(UsageLimitPeriod&& value) { SetPeriod(std::move(value)); return *this;}

  private:

    long long m_amount;
    bool m_amountHasBeenSet;

    UsageLimitBreachAction m_breachAction;
    bool m_breachActionHasBeenSet;

    UsageLimitFeatureType m_featureType;
    bool m_featureTypeHasBeenSet;

    UsageLimitLimitType m_limitType;
    bool m_limitTypeHasBeenSet;

    UsageLimitPeriod m_period;
    bool m_periodHasBeenSet;
  };

} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
