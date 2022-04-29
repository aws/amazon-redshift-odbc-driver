/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralService_EXPORTS.h>
#include <aws/core/utils/memory/stl/AWSString.h>
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

  class AWS_REDSHIFTARCADIACORALSERVICE_API ConfigParameter
  {
  public:
    ConfigParameter();
    ConfigParameter(Aws::Utils::Json::JsonView jsonValue);
    ConfigParameter& operator=(Aws::Utils::Json::JsonView jsonValue);
    Aws::Utils::Json::JsonValue Jsonize() const;


    
    inline const Aws::String& GetParameterKey() const{ return m_parameterKey; }

    
    inline bool ParameterKeyHasBeenSet() const { return m_parameterKeyHasBeenSet; }

    
    inline void SetParameterKey(const Aws::String& value) { m_parameterKeyHasBeenSet = true; m_parameterKey = value; }

    
    inline void SetParameterKey(Aws::String&& value) { m_parameterKeyHasBeenSet = true; m_parameterKey = std::move(value); }

    
    inline void SetParameterKey(const char* value) { m_parameterKeyHasBeenSet = true; m_parameterKey.assign(value); }

    
    inline ConfigParameter& WithParameterKey(const Aws::String& value) { SetParameterKey(value); return *this;}

    
    inline ConfigParameter& WithParameterKey(Aws::String&& value) { SetParameterKey(std::move(value)); return *this;}

    
    inline ConfigParameter& WithParameterKey(const char* value) { SetParameterKey(value); return *this;}


    
    inline const Aws::String& GetParameterValue() const{ return m_parameterValue; }

    
    inline bool ParameterValueHasBeenSet() const { return m_parameterValueHasBeenSet; }

    
    inline void SetParameterValue(const Aws::String& value) { m_parameterValueHasBeenSet = true; m_parameterValue = value; }

    
    inline void SetParameterValue(Aws::String&& value) { m_parameterValueHasBeenSet = true; m_parameterValue = std::move(value); }

    
    inline void SetParameterValue(const char* value) { m_parameterValueHasBeenSet = true; m_parameterValue.assign(value); }

    
    inline ConfigParameter& WithParameterValue(const Aws::String& value) { SetParameterValue(value); return *this;}

    
    inline ConfigParameter& WithParameterValue(Aws::String&& value) { SetParameterValue(std::move(value)); return *this;}

    
    inline ConfigParameter& WithParameterValue(const char* value) { SetParameterValue(value); return *this;}

  private:

    Aws::String m_parameterKey;
    bool m_parameterKeyHasBeenSet;

    Aws::String m_parameterValue;
    bool m_parameterValueHasBeenSet;
  };

} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
