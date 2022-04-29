/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralService_EXPORTS.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/core/utils/DateTime.h>
#include <aws/redshiftarcadiacoral/model/DataShareStatus.h>
#include <utility>

namespace Aws
{
namespace Utils
{
namespace Json
{
  class JsonValue;
  class JsonView;
} // namespace Json
} // namespace Utils
namespace RedshiftArcadiaCoralService
{
namespace Model
{

  class AWS_REDSHIFTARCADIACORALSERVICE_API DataShareAssociation
  {
  public:
    DataShareAssociation();
    DataShareAssociation(Aws::Utils::Json::JsonView jsonValue);
    DataShareAssociation& operator=(Aws::Utils::Json::JsonView jsonValue);
    Aws::Utils::Json::JsonValue Jsonize() const;


    
    inline const Aws::String& GetConsumerIdentifier() const{ return m_consumerIdentifier; }

    
    inline bool ConsumerIdentifierHasBeenSet() const { return m_consumerIdentifierHasBeenSet; }

    
    inline void SetConsumerIdentifier(const Aws::String& value) { m_consumerIdentifierHasBeenSet = true; m_consumerIdentifier = value; }

    
    inline void SetConsumerIdentifier(Aws::String&& value) { m_consumerIdentifierHasBeenSet = true; m_consumerIdentifier = std::move(value); }

    
    inline void SetConsumerIdentifier(const char* value) { m_consumerIdentifierHasBeenSet = true; m_consumerIdentifier.assign(value); }

    
    inline DataShareAssociation& WithConsumerIdentifier(const Aws::String& value) { SetConsumerIdentifier(value); return *this;}

    
    inline DataShareAssociation& WithConsumerIdentifier(Aws::String&& value) { SetConsumerIdentifier(std::move(value)); return *this;}

    
    inline DataShareAssociation& WithConsumerIdentifier(const char* value) { SetConsumerIdentifier(value); return *this;}


    
    inline const Aws::Utils::DateTime& GetCreatedDate() const{ return m_createdDate; }

    
    inline bool CreatedDateHasBeenSet() const { return m_createdDateHasBeenSet; }

    
    inline void SetCreatedDate(const Aws::Utils::DateTime& value) { m_createdDateHasBeenSet = true; m_createdDate = value; }

    
    inline void SetCreatedDate(Aws::Utils::DateTime&& value) { m_createdDateHasBeenSet = true; m_createdDate = std::move(value); }

    
    inline DataShareAssociation& WithCreatedDate(const Aws::Utils::DateTime& value) { SetCreatedDate(value); return *this;}

    
    inline DataShareAssociation& WithCreatedDate(Aws::Utils::DateTime&& value) { SetCreatedDate(std::move(value)); return *this;}


    
    inline const DataShareStatus& GetStatus() const{ return m_status; }

    
    inline bool StatusHasBeenSet() const { return m_statusHasBeenSet; }

    
    inline void SetStatus(const DataShareStatus& value) { m_statusHasBeenSet = true; m_status = value; }

    
    inline void SetStatus(DataShareStatus&& value) { m_statusHasBeenSet = true; m_status = std::move(value); }

    
    inline DataShareAssociation& WithStatus(const DataShareStatus& value) { SetStatus(value); return *this;}

    
    inline DataShareAssociation& WithStatus(DataShareStatus&& value) { SetStatus(std::move(value)); return *this;}


    
    inline const Aws::Utils::DateTime& GetStatusChangeDate() const{ return m_statusChangeDate; }

    
    inline bool StatusChangeDateHasBeenSet() const { return m_statusChangeDateHasBeenSet; }

    
    inline void SetStatusChangeDate(const Aws::Utils::DateTime& value) { m_statusChangeDateHasBeenSet = true; m_statusChangeDate = value; }

    
    inline void SetStatusChangeDate(Aws::Utils::DateTime&& value) { m_statusChangeDateHasBeenSet = true; m_statusChangeDate = std::move(value); }

    
    inline DataShareAssociation& WithStatusChangeDate(const Aws::Utils::DateTime& value) { SetStatusChangeDate(value); return *this;}

    
    inline DataShareAssociation& WithStatusChangeDate(Aws::Utils::DateTime&& value) { SetStatusChangeDate(std::move(value)); return *this;}

  private:

    Aws::String m_consumerIdentifier;
    bool m_consumerIdentifierHasBeenSet;

    Aws::Utils::DateTime m_createdDate;
    bool m_createdDateHasBeenSet;

    DataShareStatus m_status;
    bool m_statusHasBeenSet;

    Aws::Utils::DateTime m_statusChangeDate;
    bool m_statusChangeDateHasBeenSet;
  };

} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
