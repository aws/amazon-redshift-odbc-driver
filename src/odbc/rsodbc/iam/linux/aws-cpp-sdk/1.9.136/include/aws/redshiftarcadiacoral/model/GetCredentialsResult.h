/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralService_EXPORTS.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/core/utils/DateTime.h>
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
  class AWS_REDSHIFTARCADIACORALSERVICE_API GetCredentialsResult
  {
  public:
    GetCredentialsResult();
    GetCredentialsResult(const Aws::AmazonWebServiceResult<Aws::Utils::Json::JsonValue>& result);
    GetCredentialsResult& operator=(const Aws::AmazonWebServiceResult<Aws::Utils::Json::JsonValue>& result);


    
    inline const Aws::String& GetDbPassword() const{ return m_dbPassword; }

    
    inline void SetDbPassword(const Aws::String& value) { m_dbPassword = value; }

    
    inline void SetDbPassword(Aws::String&& value) { m_dbPassword = std::move(value); }

    
    inline void SetDbPassword(const char* value) { m_dbPassword.assign(value); }

    
    inline GetCredentialsResult& WithDbPassword(const Aws::String& value) { SetDbPassword(value); return *this;}

    
    inline GetCredentialsResult& WithDbPassword(Aws::String&& value) { SetDbPassword(std::move(value)); return *this;}

    
    inline GetCredentialsResult& WithDbPassword(const char* value) { SetDbPassword(value); return *this;}


    
    inline const Aws::String& GetDbUser() const{ return m_dbUser; }

    
    inline void SetDbUser(const Aws::String& value) { m_dbUser = value; }

    
    inline void SetDbUser(Aws::String&& value) { m_dbUser = std::move(value); }

    
    inline void SetDbUser(const char* value) { m_dbUser.assign(value); }

    
    inline GetCredentialsResult& WithDbUser(const Aws::String& value) { SetDbUser(value); return *this;}

    
    inline GetCredentialsResult& WithDbUser(Aws::String&& value) { SetDbUser(std::move(value)); return *this;}

    
    inline GetCredentialsResult& WithDbUser(const char* value) { SetDbUser(value); return *this;}


    
    inline const Aws::Utils::DateTime& GetExpiration() const{ return m_expiration; }

    
    inline void SetExpiration(const Aws::Utils::DateTime& value) { m_expiration = value; }

    
    inline void SetExpiration(Aws::Utils::DateTime&& value) { m_expiration = std::move(value); }

    
    inline GetCredentialsResult& WithExpiration(const Aws::Utils::DateTime& value) { SetExpiration(value); return *this;}

    
    inline GetCredentialsResult& WithExpiration(Aws::Utils::DateTime&& value) { SetExpiration(std::move(value)); return *this;}


    
    inline const Aws::Utils::DateTime& GetNextRefreshTime() const{ return m_nextRefreshTime; }

    
    inline void SetNextRefreshTime(const Aws::Utils::DateTime& value) { m_nextRefreshTime = value; }

    
    inline void SetNextRefreshTime(Aws::Utils::DateTime&& value) { m_nextRefreshTime = std::move(value); }

    
    inline GetCredentialsResult& WithNextRefreshTime(const Aws::Utils::DateTime& value) { SetNextRefreshTime(value); return *this;}

    
    inline GetCredentialsResult& WithNextRefreshTime(Aws::Utils::DateTime&& value) { SetNextRefreshTime(std::move(value)); return *this;}

  private:

    Aws::String m_dbPassword;

    Aws::String m_dbUser;

    Aws::Utils::DateTime m_expiration;

    Aws::Utils::DateTime m_nextRefreshTime;
  };

} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
