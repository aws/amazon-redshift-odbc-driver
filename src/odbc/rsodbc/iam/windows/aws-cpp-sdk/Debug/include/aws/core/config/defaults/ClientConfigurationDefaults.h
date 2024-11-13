/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

/**
 * Please note that this file is autogenerated.
 * The backwards compatibility of the default values provided by new client configuration defaults is not guaranteed; 
 *   the values might change over time.
 */

#pragma once

#include <aws/core/client/ClientConfiguration.h>

namespace Aws
{
    namespace Config
    {
        namespace Defaults
        {
            /** 
             * Set default client configuration parameters per provided default mode
             *
             * @param clientConfig, a ClientConfiguration to update
             * @param defaultMode, requested default mode name
             * @param hasEc2MetadataRegion, if ec2 metadata region has been already queried
             * @param ec2MetadataRegion, a region resolved by EC2 Instance Metadata service
             */
            AWS_CORE_API void SetSmartDefaultsConfigurationParameters(Aws::Client::ClientConfiguration& clientConfig,
                                                                      const Aws::String& defaultMode,
                                                                      bool hasEc2MetadataRegion,
                                                                      const Aws::String& ec2MetadataRegion);

            /**
             * Resolve the name of an actual mode for a default mode "auto"
             *
             * The AUTO mode is an experimental mode that builds on the standard mode. The SDK
             * will attempt to discover the execution environment to determine the appropriate
             * settings automatically.
             *
             * Note that the auto detection is heuristics-based and does not guarantee 100%
             * accuracy. STANDARD mode will be used if the execution environment cannot be
             * determined. The auto detection might query EC2 Instance Metadata service, which
             * might introduce latency. Therefore we recommend choosing an explicit
             * defaults_mode instead if startup latency is critical to your application.
             */
            AWS_CORE_API const char* ResolveAutoClientConfiguration(const Aws::Client::ClientConfiguration& clientConfig,
                                                                    const Aws::String& ec2MetadataRegion);

            /**
             * Default mode "legacy"
             *
             * The LEGACY mode provides default settings that vary per SDK and were used prior
             * to establishment of defaults_mode.
             */
            AWS_CORE_API void SetLegacyClientConfiguration(Aws::Client::ClientConfiguration& clientConfig);

            /**
             * Default mode "standard"
             *
             * The STANDARD mode provides the latest recommended default values that should be
             * safe to run in most scenarios.
             *
             * Note that the default values vended from this mode might change as best
             * practices may evolve. As a result, it is encouraged to perform tests when
             * upgrading the SDK.
             */
            AWS_CORE_API void SetStandardClientConfiguration(Aws::Client::ClientConfiguration& clientConfig);

            /**
             * Default mode "in-region"
             *
             * The IN_REGION mode builds on the standard mode and includes optimization
             * tailored for applications which call AWS services from within the same AWS
             * region.
             *
             * Note that the default values vended from this mode might change as best
             * practices may evolve. As a result, it is encouraged to perform tests when
             * upgrading the SDK.
             */
            AWS_CORE_API void SetInRegionClientConfiguration(Aws::Client::ClientConfiguration& clientConfig);

            /**
             * Default mode "cross-region"
             *
             * The CROSS_REGION mode builds on the standard mode and includes optimization
             * tailored for applications which call AWS services in a different region.
             *
             * Note that the default values vended from this mode might change as best
             * practices may evolve. As a result, it is encouraged to perform tests when
             * upgrading the SDK.
             */
            AWS_CORE_API void SetCrossRegionClientConfiguration(Aws::Client::ClientConfiguration& clientConfig);

            /**
             * Default mode "mobile"
             *
             * The MOBILE mode builds on the standard mode and includes optimization tailored
             * for mobile applications.
             *
             * Note that the default values vended from this mode might change as best
             * practices may evolve. As a result, it is encouraged to perform tests when
             * upgrading the SDK.
             */
            AWS_CORE_API void SetMobileClientConfiguration(Aws::Client::ClientConfiguration& clientConfig);

            /** 
             * Internal helper function to resolve smart defaults mode if not provided
             *
             * @param clientConfig, a ClientConfiguration to update
             * @param requestedDefaultMode, requested default mode name
             * @param configFileDefaultMode, default mode specified in a config file
             * @param hasEc2MetadataRegion, if ec2 metadata region has been already queried
             * @param ec2MetadataRegion, a region resolved by EC2 Instance Metadata service
             */
            AWS_CORE_API Aws::String ResolveDefaultModeName(const Aws::Client::ClientConfiguration& clientConfig,
                                                            Aws::String requestedDefaultMode,
                                                            const Aws::String& configFileDefaultMode,
                                                            bool hasEc2MetadataRegion,
                                                            Aws::String ec2MetadataRegion);
        } //namespace Defaults
    } //namespace Config
} //namespace Aws
