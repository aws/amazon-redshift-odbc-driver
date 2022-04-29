/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralService_EXPORTS.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/core/utils/memory/stl/AWSVector.h>
#include <aws/redshiftarcadiacoral/model/DataShareAssociation.h>
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

  class AWS_REDSHIFTARCADIACORALSERVICE_API DataShare
  {
  public:
    DataShare();
    DataShare(Aws::Utils::Json::JsonView jsonValue);
    DataShare& operator=(Aws::Utils::Json::JsonView jsonValue);
    Aws::Utils::Json::JsonValue Jsonize() const;


    
    inline bool GetAllowPubliclyAccessibleConsumers() const{ return m_allowPubliclyAccessibleConsumers; }

    
    inline bool AllowPubliclyAccessibleConsumersHasBeenSet() const { return m_allowPubliclyAccessibleConsumersHasBeenSet; }

    
    inline void SetAllowPubliclyAccessibleConsumers(bool value) { m_allowPubliclyAccessibleConsumersHasBeenSet = true; m_allowPubliclyAccessibleConsumers = value; }

    
    inline DataShare& WithAllowPubliclyAccessibleConsumers(bool value) { SetAllowPubliclyAccessibleConsumers(value); return *this;}


    
    inline const Aws::String& GetDataShareArn() const{ return m_dataShareArn; }

    
    inline bool DataShareArnHasBeenSet() const { return m_dataShareArnHasBeenSet; }

    
    inline void SetDataShareArn(const Aws::String& value) { m_dataShareArnHasBeenSet = true; m_dataShareArn = value; }

    
    inline void SetDataShareArn(Aws::String&& value) { m_dataShareArnHasBeenSet = true; m_dataShareArn = std::move(value); }

    
    inline void SetDataShareArn(const char* value) { m_dataShareArnHasBeenSet = true; m_dataShareArn.assign(value); }

    
    inline DataShare& WithDataShareArn(const Aws::String& value) { SetDataShareArn(value); return *this;}

    
    inline DataShare& WithDataShareArn(Aws::String&& value) { SetDataShareArn(std::move(value)); return *this;}

    
    inline DataShare& WithDataShareArn(const char* value) { SetDataShareArn(value); return *this;}


    
    inline const Aws::Vector<DataShareAssociation>& GetDataShareAssociations() const{ return m_dataShareAssociations; }

    
    inline bool DataShareAssociationsHasBeenSet() const { return m_dataShareAssociationsHasBeenSet; }

    
    inline void SetDataShareAssociations(const Aws::Vector<DataShareAssociation>& value) { m_dataShareAssociationsHasBeenSet = true; m_dataShareAssociations = value; }

    
    inline void SetDataShareAssociations(Aws::Vector<DataShareAssociation>&& value) { m_dataShareAssociationsHasBeenSet = true; m_dataShareAssociations = std::move(value); }

    
    inline DataShare& WithDataShareAssociations(const Aws::Vector<DataShareAssociation>& value) { SetDataShareAssociations(value); return *this;}

    
    inline DataShare& WithDataShareAssociations(Aws::Vector<DataShareAssociation>&& value) { SetDataShareAssociations(std::move(value)); return *this;}

    
    inline DataShare& AddDataShareAssociations(const DataShareAssociation& value) { m_dataShareAssociationsHasBeenSet = true; m_dataShareAssociations.push_back(value); return *this; }

    
    inline DataShare& AddDataShareAssociations(DataShareAssociation&& value) { m_dataShareAssociationsHasBeenSet = true; m_dataShareAssociations.push_back(std::move(value)); return *this; }


    
    inline const Aws::String& GetProducerArn() const{ return m_producerArn; }

    
    inline bool ProducerArnHasBeenSet() const { return m_producerArnHasBeenSet; }

    
    inline void SetProducerArn(const Aws::String& value) { m_producerArnHasBeenSet = true; m_producerArn = value; }

    
    inline void SetProducerArn(Aws::String&& value) { m_producerArnHasBeenSet = true; m_producerArn = std::move(value); }

    
    inline void SetProducerArn(const char* value) { m_producerArnHasBeenSet = true; m_producerArn.assign(value); }

    
    inline DataShare& WithProducerArn(const Aws::String& value) { SetProducerArn(value); return *this;}

    
    inline DataShare& WithProducerArn(Aws::String&& value) { SetProducerArn(std::move(value)); return *this;}

    
    inline DataShare& WithProducerArn(const char* value) { SetProducerArn(value); return *this;}

  private:

    bool m_allowPubliclyAccessibleConsumers;
    bool m_allowPubliclyAccessibleConsumersHasBeenSet;

    Aws::String m_dataShareArn;
    bool m_dataShareArnHasBeenSet;

    Aws::Vector<DataShareAssociation> m_dataShareAssociations;
    bool m_dataShareAssociationsHasBeenSet;

    Aws::String m_producerArn;
    bool m_producerArnHasBeenSet;
  };

} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
