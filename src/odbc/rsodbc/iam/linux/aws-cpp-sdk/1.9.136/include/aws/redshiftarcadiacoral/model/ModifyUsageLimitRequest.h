/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralService_EXPORTS.h>
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralServiceRequest.h>
#include <aws/redshiftarcadiacoral/model/UsageLimitBreachAction.h>
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
  class AWS_REDSHIFTARCADIACORALSERVICE_API ModifyUsageLimitRequest : public RedshiftArcadiaCoralServiceRequest
  {
  public:
    ModifyUsageLimitRequest();

    // Service request name is the Operation name which will send this request out,
    // each operation should has unique request name, so that we can get operation's name from this request.
    // Note: this is not true for response, multiple operations may have the same response name,
    // so we can not get operation's name from response.
    inline virtual const char* GetServiceRequestName() const override { return "ModifyUsageLimit"; }

    Aws::String SerializePayload() const override;

    Aws::Http::HeaderValueCollection GetRequestSpecificHeaders() const override;


    
    inline long long GetAmount() const{ return m_amount; }

    
    inline bool AmountHasBeenSet() const { return m_amountHasBeenSet; }

    
    inline void SetAmount(long long value) { m_amountHasBeenSet = true; m_amount = value; }

    
    inline ModifyUsageLimitRequest& WithAmount(long long value) { SetAmount(value); return *this;}


    
    inline const UsageLimitBreachAction& GetBreachAction() const{ return m_breachAction; }

    
    inline bool BreachActionHasBeenSet() const { return m_breachActionHasBeenSet; }

    
    inline void SetBreachAction(const UsageLimitBreachAction& value) { m_breachActionHasBeenSet = true; m_breachAction = value; }

    
    inline void SetBreachAction(UsageLimitBreachAction&& value) { m_breachActionHasBeenSet = true; m_breachAction = std::move(value); }

    
    inline ModifyUsageLimitRequest& WithBreachAction(const UsageLimitBreachAction& value) { SetBreachAction(value); return *this;}

    
    inline ModifyUsageLimitRequest& WithBreachAction(UsageLimitBreachAction&& value) { SetBreachAction(std::move(value)); return *this;}


    
    inline const Aws::String& GetUsageLimitId() const{ return m_usageLimitId; }

    
    inline bool UsageLimitIdHasBeenSet() const { return m_usageLimitIdHasBeenSet; }

    
    inline void SetUsageLimitId(const Aws::String& value) { m_usageLimitIdHasBeenSet = true; m_usageLimitId = value; }

    
    inline void SetUsageLimitId(Aws::String&& value) { m_usageLimitIdHasBeenSet = true; m_usageLimitId = std::move(value); }

    
    inline void SetUsageLimitId(const char* value) { m_usageLimitIdHasBeenSet = true; m_usageLimitId.assign(value); }

    
    inline ModifyUsageLimitRequest& WithUsageLimitId(const Aws::String& value) { SetUsageLimitId(value); return *this;}

    
    inline ModifyUsageLimitRequest& WithUsageLimitId(Aws::String&& value) { SetUsageLimitId(std::move(value)); return *this;}

    
    inline ModifyUsageLimitRequest& WithUsageLimitId(const char* value) { SetUsageLimitId(value); return *this;}

  private:

    long long m_amount;
    bool m_amountHasBeenSet;

    UsageLimitBreachAction m_breachAction;
    bool m_breachActionHasBeenSet;

    Aws::String m_usageLimitId;
    bool m_usageLimitIdHasBeenSet;
  };

} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
