﻿/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/sts/STS_EXPORTS.h>
#include <aws/core/utils/memory/stl/AWSStreamFwd.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <utility>

namespace Aws
{
namespace Utils
{
namespace Xml
{
  class XmlNode;
} // namespace Xml
} // namespace Utils
namespace STS
{
namespace Model
{

  /**
   * <p>Contains information about the provided context. This includes the signed and
   * encrypted trusted context assertion and the context provider ARN from which the
   * trusted context assertion was generated.</p><p><h3>See Also:</h3>   <a
   * href="http://docs.aws.amazon.com/goto/WebAPI/sts-2011-06-15/ProvidedContext">AWS
   * API Reference</a></p>
   */
  class ProvidedContext
  {
  public:
    AWS_STS_API ProvidedContext();
    AWS_STS_API ProvidedContext(const Aws::Utils::Xml::XmlNode& xmlNode);
    AWS_STS_API ProvidedContext& operator=(const Aws::Utils::Xml::XmlNode& xmlNode);

    AWS_STS_API void OutputToStream(Aws::OStream& ostream, const char* location, unsigned index, const char* locationValue) const;
    AWS_STS_API void OutputToStream(Aws::OStream& oStream, const char* location) const;


    /**
     * <p>The context provider ARN from which the trusted context assertion was
     * generated.</p>
     */
    inline const Aws::String& GetProviderArn() const{ return m_providerArn; }

    /**
     * <p>The context provider ARN from which the trusted context assertion was
     * generated.</p>
     */
    inline bool ProviderArnHasBeenSet() const { return m_providerArnHasBeenSet; }

    /**
     * <p>The context provider ARN from which the trusted context assertion was
     * generated.</p>
     */
    inline void SetProviderArn(const Aws::String& value) { m_providerArnHasBeenSet = true; m_providerArn = value; }

    /**
     * <p>The context provider ARN from which the trusted context assertion was
     * generated.</p>
     */
    inline void SetProviderArn(Aws::String&& value) { m_providerArnHasBeenSet = true; m_providerArn = std::move(value); }

    /**
     * <p>The context provider ARN from which the trusted context assertion was
     * generated.</p>
     */
    inline void SetProviderArn(const char* value) { m_providerArnHasBeenSet = true; m_providerArn.assign(value); }

    /**
     * <p>The context provider ARN from which the trusted context assertion was
     * generated.</p>
     */
    inline ProvidedContext& WithProviderArn(const Aws::String& value) { SetProviderArn(value); return *this;}

    /**
     * <p>The context provider ARN from which the trusted context assertion was
     * generated.</p>
     */
    inline ProvidedContext& WithProviderArn(Aws::String&& value) { SetProviderArn(std::move(value)); return *this;}

    /**
     * <p>The context provider ARN from which the trusted context assertion was
     * generated.</p>
     */
    inline ProvidedContext& WithProviderArn(const char* value) { SetProviderArn(value); return *this;}


    /**
     * <p>The signed and encrypted trusted context assertion generated by the context
     * provider. The trusted context assertion is signed and encrypted by Amazon Web
     * Services STS.</p>
     */
    inline const Aws::String& GetContextAssertion() const{ return m_contextAssertion; }

    /**
     * <p>The signed and encrypted trusted context assertion generated by the context
     * provider. The trusted context assertion is signed and encrypted by Amazon Web
     * Services STS.</p>
     */
    inline bool ContextAssertionHasBeenSet() const { return m_contextAssertionHasBeenSet; }

    /**
     * <p>The signed and encrypted trusted context assertion generated by the context
     * provider. The trusted context assertion is signed and encrypted by Amazon Web
     * Services STS.</p>
     */
    inline void SetContextAssertion(const Aws::String& value) { m_contextAssertionHasBeenSet = true; m_contextAssertion = value; }

    /**
     * <p>The signed and encrypted trusted context assertion generated by the context
     * provider. The trusted context assertion is signed and encrypted by Amazon Web
     * Services STS.</p>
     */
    inline void SetContextAssertion(Aws::String&& value) { m_contextAssertionHasBeenSet = true; m_contextAssertion = std::move(value); }

    /**
     * <p>The signed and encrypted trusted context assertion generated by the context
     * provider. The trusted context assertion is signed and encrypted by Amazon Web
     * Services STS.</p>
     */
    inline void SetContextAssertion(const char* value) { m_contextAssertionHasBeenSet = true; m_contextAssertion.assign(value); }

    /**
     * <p>The signed and encrypted trusted context assertion generated by the context
     * provider. The trusted context assertion is signed and encrypted by Amazon Web
     * Services STS.</p>
     */
    inline ProvidedContext& WithContextAssertion(const Aws::String& value) { SetContextAssertion(value); return *this;}

    /**
     * <p>The signed and encrypted trusted context assertion generated by the context
     * provider. The trusted context assertion is signed and encrypted by Amazon Web
     * Services STS.</p>
     */
    inline ProvidedContext& WithContextAssertion(Aws::String&& value) { SetContextAssertion(std::move(value)); return *this;}

    /**
     * <p>The signed and encrypted trusted context assertion generated by the context
     * provider. The trusted context assertion is signed and encrypted by Amazon Web
     * Services STS.</p>
     */
    inline ProvidedContext& WithContextAssertion(const char* value) { SetContextAssertion(value); return *this;}

  private:

    Aws::String m_providerArn;
    bool m_providerArnHasBeenSet = false;

    Aws::String m_contextAssertion;
    bool m_contextAssertionHasBeenSet = false;
  };

} // namespace Model
} // namespace STS
} // namespace Aws
