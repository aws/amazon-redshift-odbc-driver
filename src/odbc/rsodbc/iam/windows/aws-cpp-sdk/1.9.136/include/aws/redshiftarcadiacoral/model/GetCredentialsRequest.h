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
  class AWS_REDSHIFTARCADIACORALSERVICE_API GetCredentialsRequest : public RedshiftArcadiaCoralServiceRequest
  {
  public:
    GetCredentialsRequest();

    // Service request name is the Operation name which will send this request out,
    // each operation should has unique request name, so that we can get operation's name from this request.
    // Note: this is not true for response, multiple operations may have the same response name,
    // so we can not get operation's name from response.
    inline virtual const char* GetServiceRequestName() const override { return "GetCredentials"; }

    Aws::String SerializePayload() const override;

    Aws::Http::HeaderValueCollection GetRequestSpecificHeaders() const override;


    
    inline const Aws::String& GetDbName() const{ return m_dbName; }

    
    inline bool DbNameHasBeenSet() const { return m_dbNameHasBeenSet; }

    
    inline void SetDbName(const Aws::String& value) { m_dbNameHasBeenSet = true; m_dbName = value; }

    
    inline void SetDbName(Aws::String&& value) { m_dbNameHasBeenSet = true; m_dbName = std::move(value); }

    
    inline void SetDbName(const char* value) { m_dbNameHasBeenSet = true; m_dbName.assign(value); }

    
    inline GetCredentialsRequest& WithDbName(const Aws::String& value) { SetDbName(value); return *this;}

    
    inline GetCredentialsRequest& WithDbName(Aws::String&& value) { SetDbName(std::move(value)); return *this;}

    
    inline GetCredentialsRequest& WithDbName(const char* value) { SetDbName(value); return *this;}


    
    inline int GetDurationSeconds() const{ return m_durationSeconds; }

    
    inline bool DurationSecondsHasBeenSet() const { return m_durationSecondsHasBeenSet; }

    
    inline void SetDurationSeconds(int value) { m_durationSecondsHasBeenSet = true; m_durationSeconds = value; }

    
    inline GetCredentialsRequest& WithDurationSeconds(int value) { SetDurationSeconds(value); return *this;}

  private:

    Aws::String m_dbName;
    bool m_dbNameHasBeenSet;

    int m_durationSeconds;
    bool m_durationSecondsHasBeenSet;
  };

} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
