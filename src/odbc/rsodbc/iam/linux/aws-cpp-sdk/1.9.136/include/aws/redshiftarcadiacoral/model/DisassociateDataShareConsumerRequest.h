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
  class AWS_REDSHIFTARCADIACORALSERVICE_API DisassociateDataShareConsumerRequest : public RedshiftArcadiaCoralServiceRequest
  {
  public:
    DisassociateDataShareConsumerRequest();

    // Service request name is the Operation name which will send this request out,
    // each operation should has unique request name, so that we can get operation's name from this request.
    // Note: this is not true for response, multiple operations may have the same response name,
    // so we can not get operation's name from response.
    inline virtual const char* GetServiceRequestName() const override { return "DisassociateDataShareConsumer"; }

    Aws::String SerializePayload() const override;

    Aws::Http::HeaderValueCollection GetRequestSpecificHeaders() const override;


    
    inline const Aws::String& GetConsumerArn() const{ return m_consumerArn; }

    
    inline bool ConsumerArnHasBeenSet() const { return m_consumerArnHasBeenSet; }

    
    inline void SetConsumerArn(const Aws::String& value) { m_consumerArnHasBeenSet = true; m_consumerArn = value; }

    
    inline void SetConsumerArn(Aws::String&& value) { m_consumerArnHasBeenSet = true; m_consumerArn = std::move(value); }

    
    inline void SetConsumerArn(const char* value) { m_consumerArnHasBeenSet = true; m_consumerArn.assign(value); }

    
    inline DisassociateDataShareConsumerRequest& WithConsumerArn(const Aws::String& value) { SetConsumerArn(value); return *this;}

    
    inline DisassociateDataShareConsumerRequest& WithConsumerArn(Aws::String&& value) { SetConsumerArn(std::move(value)); return *this;}

    
    inline DisassociateDataShareConsumerRequest& WithConsumerArn(const char* value) { SetConsumerArn(value); return *this;}


    
    inline const Aws::String& GetConsumerRegion() const{ return m_consumerRegion; }

    
    inline bool ConsumerRegionHasBeenSet() const { return m_consumerRegionHasBeenSet; }

    
    inline void SetConsumerRegion(const Aws::String& value) { m_consumerRegionHasBeenSet = true; m_consumerRegion = value; }

    
    inline void SetConsumerRegion(Aws::String&& value) { m_consumerRegionHasBeenSet = true; m_consumerRegion = std::move(value); }

    
    inline void SetConsumerRegion(const char* value) { m_consumerRegionHasBeenSet = true; m_consumerRegion.assign(value); }

    
    inline DisassociateDataShareConsumerRequest& WithConsumerRegion(const Aws::String& value) { SetConsumerRegion(value); return *this;}

    
    inline DisassociateDataShareConsumerRequest& WithConsumerRegion(Aws::String&& value) { SetConsumerRegion(std::move(value)); return *this;}

    
    inline DisassociateDataShareConsumerRequest& WithConsumerRegion(const char* value) { SetConsumerRegion(value); return *this;}


    
    inline const Aws::String& GetDataShareArn() const{ return m_dataShareArn; }

    
    inline bool DataShareArnHasBeenSet() const { return m_dataShareArnHasBeenSet; }

    
    inline void SetDataShareArn(const Aws::String& value) { m_dataShareArnHasBeenSet = true; m_dataShareArn = value; }

    
    inline void SetDataShareArn(Aws::String&& value) { m_dataShareArnHasBeenSet = true; m_dataShareArn = std::move(value); }

    
    inline void SetDataShareArn(const char* value) { m_dataShareArnHasBeenSet = true; m_dataShareArn.assign(value); }

    
    inline DisassociateDataShareConsumerRequest& WithDataShareArn(const Aws::String& value) { SetDataShareArn(value); return *this;}

    
    inline DisassociateDataShareConsumerRequest& WithDataShareArn(Aws::String&& value) { SetDataShareArn(std::move(value)); return *this;}

    
    inline DisassociateDataShareConsumerRequest& WithDataShareArn(const char* value) { SetDataShareArn(value); return *this;}


    
    inline bool GetDisassociateEntireAccount() const{ return m_disassociateEntireAccount; }

    
    inline bool DisassociateEntireAccountHasBeenSet() const { return m_disassociateEntireAccountHasBeenSet; }

    
    inline void SetDisassociateEntireAccount(bool value) { m_disassociateEntireAccountHasBeenSet = true; m_disassociateEntireAccount = value; }

    
    inline DisassociateDataShareConsumerRequest& WithDisassociateEntireAccount(bool value) { SetDisassociateEntireAccount(value); return *this;}

  private:

    Aws::String m_consumerArn;
    bool m_consumerArnHasBeenSet;

    Aws::String m_consumerRegion;
    bool m_consumerRegionHasBeenSet;

    Aws::String m_dataShareArn;
    bool m_dataShareArnHasBeenSet;

    bool m_disassociateEntireAccount;
    bool m_disassociateEntireAccountHasBeenSet;
  };

} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
