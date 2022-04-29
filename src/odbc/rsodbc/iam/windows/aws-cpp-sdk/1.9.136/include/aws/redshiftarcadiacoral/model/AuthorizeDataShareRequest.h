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
  class AWS_REDSHIFTARCADIACORALSERVICE_API AuthorizeDataShareRequest : public RedshiftArcadiaCoralServiceRequest
  {
  public:
    AuthorizeDataShareRequest();

    // Service request name is the Operation name which will send this request out,
    // each operation should has unique request name, so that we can get operation's name from this request.
    // Note: this is not true for response, multiple operations may have the same response name,
    // so we can not get operation's name from response.
    inline virtual const char* GetServiceRequestName() const override { return "AuthorizeDataShare"; }

    Aws::String SerializePayload() const override;

    Aws::Http::HeaderValueCollection GetRequestSpecificHeaders() const override;


    
    inline const Aws::String& GetConsumerIdentifier() const{ return m_consumerIdentifier; }

    
    inline bool ConsumerIdentifierHasBeenSet() const { return m_consumerIdentifierHasBeenSet; }

    
    inline void SetConsumerIdentifier(const Aws::String& value) { m_consumerIdentifierHasBeenSet = true; m_consumerIdentifier = value; }

    
    inline void SetConsumerIdentifier(Aws::String&& value) { m_consumerIdentifierHasBeenSet = true; m_consumerIdentifier = std::move(value); }

    
    inline void SetConsumerIdentifier(const char* value) { m_consumerIdentifierHasBeenSet = true; m_consumerIdentifier.assign(value); }

    
    inline AuthorizeDataShareRequest& WithConsumerIdentifier(const Aws::String& value) { SetConsumerIdentifier(value); return *this;}

    
    inline AuthorizeDataShareRequest& WithConsumerIdentifier(Aws::String&& value) { SetConsumerIdentifier(std::move(value)); return *this;}

    
    inline AuthorizeDataShareRequest& WithConsumerIdentifier(const char* value) { SetConsumerIdentifier(value); return *this;}


    
    inline const Aws::String& GetDataShareArn() const{ return m_dataShareArn; }

    
    inline bool DataShareArnHasBeenSet() const { return m_dataShareArnHasBeenSet; }

    
    inline void SetDataShareArn(const Aws::String& value) { m_dataShareArnHasBeenSet = true; m_dataShareArn = value; }

    
    inline void SetDataShareArn(Aws::String&& value) { m_dataShareArnHasBeenSet = true; m_dataShareArn = std::move(value); }

    
    inline void SetDataShareArn(const char* value) { m_dataShareArnHasBeenSet = true; m_dataShareArn.assign(value); }

    
    inline AuthorizeDataShareRequest& WithDataShareArn(const Aws::String& value) { SetDataShareArn(value); return *this;}

    
    inline AuthorizeDataShareRequest& WithDataShareArn(Aws::String&& value) { SetDataShareArn(std::move(value)); return *this;}

    
    inline AuthorizeDataShareRequest& WithDataShareArn(const char* value) { SetDataShareArn(value); return *this;}

  private:

    Aws::String m_consumerIdentifier;
    bool m_consumerIdentifierHasBeenSet;

    Aws::String m_dataShareArn;
    bool m_dataShareArnHasBeenSet;
  };

} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
