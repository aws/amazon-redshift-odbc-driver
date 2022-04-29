/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralService_EXPORTS.h>
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralServiceErrors.h>
#include <aws/core/client/AWSError.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/client/AWSClient.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/redshiftarcadiacoral/model/AssociateDataShareConsumerResult.h>
#include <aws/redshiftarcadiacoral/model/AuthorizeDataShareResult.h>
#include <aws/redshiftarcadiacoral/model/CreateSnapshotResult.h>
#include <aws/redshiftarcadiacoral/model/CreateUsageLimitResult.h>
#include <aws/redshiftarcadiacoral/model/DeauthorizeDataShareResult.h>
#include <aws/redshiftarcadiacoral/model/DeleteSnapshotResult.h>
#include <aws/redshiftarcadiacoral/model/DescribeConfigurationResult.h>
#include <aws/redshiftarcadiacoral/model/DescribeDataSharesResult.h>
#include <aws/redshiftarcadiacoral/model/DescribeDataSharesForConsumerResult.h>
#include <aws/redshiftarcadiacoral/model/DescribeDataSharesForProducerResult.h>
#include <aws/redshiftarcadiacoral/model/DescribeSnapshotsResult.h>
#include <aws/redshiftarcadiacoral/model/DescribeUsageLimitResult.h>
#include <aws/redshiftarcadiacoral/model/DisassociateDataShareConsumerResult.h>
#include <aws/redshiftarcadiacoral/model/GetCredentialsResult.h>
#include <aws/redshiftarcadiacoral/model/ModifyUsageLimitResult.h>
#include <aws/redshiftarcadiacoral/model/RejectDataShareResult.h>
#include <aws/redshiftarcadiacoral/model/RestoreFromSnapshotResult.h>
#include <aws/redshiftarcadiacoral/model/SetConfigurationResult.h>
#include <aws/core/NoResult.h>
#include <aws/core/client/AsyncCallerContext.h>
#include <aws/core/http/HttpTypes.h>
#include <future>
#include <functional>

namespace Aws
{

namespace Http
{
  class HttpClient;
  class HttpClientFactory;
} // namespace Http

namespace Utils
{
  template< typename R, typename E> class Outcome;
namespace Threading
{
  class Executor;
} // namespace Threading
} // namespace Utils

namespace Auth
{
  class AWSCredentials;
  class AWSCredentialsProvider;
} // namespace Auth

namespace Client
{
  class RetryStrategy;
} // namespace Client

namespace RedshiftArcadiaCoralService
{

namespace Model
{
        class AssociateDataShareConsumerRequest;
        class AuthorizeDataShareRequest;
        class CreateSnapshotRequest;
        class CreateUsageLimitRequest;
        class DeauthorizeDataShareRequest;
        class DeleteSnapshotRequest;
        class DeleteUsageLimitRequest;
        class DescribeConfigurationRequest;
        class DescribeDataSharesRequest;
        class DescribeDataSharesForConsumerRequest;
        class DescribeDataSharesForProducerRequest;
        class DescribeSnapshotsRequest;
        class DescribeUsageLimitRequest;
        class DisassociateDataShareConsumerRequest;
        class GetCredentialsRequest;
        class ModifyUsageLimitRequest;
        class RejectDataShareRequest;
        class RestoreFromSnapshotRequest;
        class SetConfigurationRequest;

        typedef Aws::Utils::Outcome<AssociateDataShareConsumerResult, RedshiftArcadiaCoralServiceError> AssociateDataShareConsumerOutcome;
        typedef Aws::Utils::Outcome<AuthorizeDataShareResult, RedshiftArcadiaCoralServiceError> AuthorizeDataShareOutcome;
        typedef Aws::Utils::Outcome<CreateSnapshotResult, RedshiftArcadiaCoralServiceError> CreateSnapshotOutcome;
        typedef Aws::Utils::Outcome<CreateUsageLimitResult, RedshiftArcadiaCoralServiceError> CreateUsageLimitOutcome;
        typedef Aws::Utils::Outcome<DeauthorizeDataShareResult, RedshiftArcadiaCoralServiceError> DeauthorizeDataShareOutcome;
        typedef Aws::Utils::Outcome<DeleteSnapshotResult, RedshiftArcadiaCoralServiceError> DeleteSnapshotOutcome;
        typedef Aws::Utils::Outcome<Aws::NoResult, RedshiftArcadiaCoralServiceError> DeleteUsageLimitOutcome;
        typedef Aws::Utils::Outcome<DescribeConfigurationResult, RedshiftArcadiaCoralServiceError> DescribeConfigurationOutcome;
        typedef Aws::Utils::Outcome<DescribeDataSharesResult, RedshiftArcadiaCoralServiceError> DescribeDataSharesOutcome;
        typedef Aws::Utils::Outcome<DescribeDataSharesForConsumerResult, RedshiftArcadiaCoralServiceError> DescribeDataSharesForConsumerOutcome;
        typedef Aws::Utils::Outcome<DescribeDataSharesForProducerResult, RedshiftArcadiaCoralServiceError> DescribeDataSharesForProducerOutcome;
        typedef Aws::Utils::Outcome<DescribeSnapshotsResult, RedshiftArcadiaCoralServiceError> DescribeSnapshotsOutcome;
        typedef Aws::Utils::Outcome<DescribeUsageLimitResult, RedshiftArcadiaCoralServiceError> DescribeUsageLimitOutcome;
        typedef Aws::Utils::Outcome<DisassociateDataShareConsumerResult, RedshiftArcadiaCoralServiceError> DisassociateDataShareConsumerOutcome;
        typedef Aws::Utils::Outcome<GetCredentialsResult, RedshiftArcadiaCoralServiceError> GetCredentialsOutcome;
        typedef Aws::Utils::Outcome<ModifyUsageLimitResult, RedshiftArcadiaCoralServiceError> ModifyUsageLimitOutcome;
        typedef Aws::Utils::Outcome<RejectDataShareResult, RedshiftArcadiaCoralServiceError> RejectDataShareOutcome;
        typedef Aws::Utils::Outcome<RestoreFromSnapshotResult, RedshiftArcadiaCoralServiceError> RestoreFromSnapshotOutcome;
        typedef Aws::Utils::Outcome<SetConfigurationResult, RedshiftArcadiaCoralServiceError> SetConfigurationOutcome;

        typedef std::future<AssociateDataShareConsumerOutcome> AssociateDataShareConsumerOutcomeCallable;
        typedef std::future<AuthorizeDataShareOutcome> AuthorizeDataShareOutcomeCallable;
        typedef std::future<CreateSnapshotOutcome> CreateSnapshotOutcomeCallable;
        typedef std::future<CreateUsageLimitOutcome> CreateUsageLimitOutcomeCallable;
        typedef std::future<DeauthorizeDataShareOutcome> DeauthorizeDataShareOutcomeCallable;
        typedef std::future<DeleteSnapshotOutcome> DeleteSnapshotOutcomeCallable;
        typedef std::future<DeleteUsageLimitOutcome> DeleteUsageLimitOutcomeCallable;
        typedef std::future<DescribeConfigurationOutcome> DescribeConfigurationOutcomeCallable;
        typedef std::future<DescribeDataSharesOutcome> DescribeDataSharesOutcomeCallable;
        typedef std::future<DescribeDataSharesForConsumerOutcome> DescribeDataSharesForConsumerOutcomeCallable;
        typedef std::future<DescribeDataSharesForProducerOutcome> DescribeDataSharesForProducerOutcomeCallable;
        typedef std::future<DescribeSnapshotsOutcome> DescribeSnapshotsOutcomeCallable;
        typedef std::future<DescribeUsageLimitOutcome> DescribeUsageLimitOutcomeCallable;
        typedef std::future<DisassociateDataShareConsumerOutcome> DisassociateDataShareConsumerOutcomeCallable;
        typedef std::future<GetCredentialsOutcome> GetCredentialsOutcomeCallable;
        typedef std::future<ModifyUsageLimitOutcome> ModifyUsageLimitOutcomeCallable;
        typedef std::future<RejectDataShareOutcome> RejectDataShareOutcomeCallable;
        typedef std::future<RestoreFromSnapshotOutcome> RestoreFromSnapshotOutcomeCallable;
        typedef std::future<SetConfigurationOutcome> SetConfigurationOutcomeCallable;
} // namespace Model

  class RedshiftArcadiaCoralServiceClient;

    typedef std::function<void(const RedshiftArcadiaCoralServiceClient*, const Model::AssociateDataShareConsumerRequest&, const Model::AssociateDataShareConsumerOutcome&, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) > AssociateDataShareConsumerResponseReceivedHandler;
    typedef std::function<void(const RedshiftArcadiaCoralServiceClient*, const Model::AuthorizeDataShareRequest&, const Model::AuthorizeDataShareOutcome&, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) > AuthorizeDataShareResponseReceivedHandler;
    typedef std::function<void(const RedshiftArcadiaCoralServiceClient*, const Model::CreateSnapshotRequest&, const Model::CreateSnapshotOutcome&, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) > CreateSnapshotResponseReceivedHandler;
    typedef std::function<void(const RedshiftArcadiaCoralServiceClient*, const Model::CreateUsageLimitRequest&, const Model::CreateUsageLimitOutcome&, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) > CreateUsageLimitResponseReceivedHandler;
    typedef std::function<void(const RedshiftArcadiaCoralServiceClient*, const Model::DeauthorizeDataShareRequest&, const Model::DeauthorizeDataShareOutcome&, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) > DeauthorizeDataShareResponseReceivedHandler;
    typedef std::function<void(const RedshiftArcadiaCoralServiceClient*, const Model::DeleteSnapshotRequest&, const Model::DeleteSnapshotOutcome&, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) > DeleteSnapshotResponseReceivedHandler;
    typedef std::function<void(const RedshiftArcadiaCoralServiceClient*, const Model::DeleteUsageLimitRequest&, const Model::DeleteUsageLimitOutcome&, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) > DeleteUsageLimitResponseReceivedHandler;
    typedef std::function<void(const RedshiftArcadiaCoralServiceClient*, const Model::DescribeConfigurationRequest&, const Model::DescribeConfigurationOutcome&, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) > DescribeConfigurationResponseReceivedHandler;
    typedef std::function<void(const RedshiftArcadiaCoralServiceClient*, const Model::DescribeDataSharesRequest&, const Model::DescribeDataSharesOutcome&, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) > DescribeDataSharesResponseReceivedHandler;
    typedef std::function<void(const RedshiftArcadiaCoralServiceClient*, const Model::DescribeDataSharesForConsumerRequest&, const Model::DescribeDataSharesForConsumerOutcome&, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) > DescribeDataSharesForConsumerResponseReceivedHandler;
    typedef std::function<void(const RedshiftArcadiaCoralServiceClient*, const Model::DescribeDataSharesForProducerRequest&, const Model::DescribeDataSharesForProducerOutcome&, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) > DescribeDataSharesForProducerResponseReceivedHandler;
    typedef std::function<void(const RedshiftArcadiaCoralServiceClient*, const Model::DescribeSnapshotsRequest&, const Model::DescribeSnapshotsOutcome&, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) > DescribeSnapshotsResponseReceivedHandler;
    typedef std::function<void(const RedshiftArcadiaCoralServiceClient*, const Model::DescribeUsageLimitRequest&, const Model::DescribeUsageLimitOutcome&, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) > DescribeUsageLimitResponseReceivedHandler;
    typedef std::function<void(const RedshiftArcadiaCoralServiceClient*, const Model::DisassociateDataShareConsumerRequest&, const Model::DisassociateDataShareConsumerOutcome&, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) > DisassociateDataShareConsumerResponseReceivedHandler;
    typedef std::function<void(const RedshiftArcadiaCoralServiceClient*, const Model::GetCredentialsRequest&, const Model::GetCredentialsOutcome&, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) > GetCredentialsResponseReceivedHandler;
    typedef std::function<void(const RedshiftArcadiaCoralServiceClient*, const Model::ModifyUsageLimitRequest&, const Model::ModifyUsageLimitOutcome&, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) > ModifyUsageLimitResponseReceivedHandler;
    typedef std::function<void(const RedshiftArcadiaCoralServiceClient*, const Model::RejectDataShareRequest&, const Model::RejectDataShareOutcome&, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) > RejectDataShareResponseReceivedHandler;
    typedef std::function<void(const RedshiftArcadiaCoralServiceClient*, const Model::RestoreFromSnapshotRequest&, const Model::RestoreFromSnapshotOutcome&, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) > RestoreFromSnapshotResponseReceivedHandler;
    typedef std::function<void(const RedshiftArcadiaCoralServiceClient*, const Model::SetConfigurationRequest&, const Model::SetConfigurationOutcome&, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) > SetConfigurationResponseReceivedHandler;

  class AWS_REDSHIFTARCADIACORALSERVICE_API RedshiftArcadiaCoralServiceClient : public Aws::Client::AWSJsonClient
  {
    public:
      typedef Aws::Client::AWSJsonClient BASECLASS;

       /**
        * Initializes client to use DefaultCredentialProviderChain, with default http client factory, and optional client config. If client config
        * is not specified, it will be initialized to default values.
        */
        RedshiftArcadiaCoralServiceClient(const Aws::Client::ClientConfiguration& clientConfiguration = Aws::Client::ClientConfiguration());

       /**
        * Initializes client to use SimpleAWSCredentialsProvider, with default http client factory, and optional client config. If client config
        * is not specified, it will be initialized to default values.
        */
        RedshiftArcadiaCoralServiceClient(const Aws::Auth::AWSCredentials& credentials, const Aws::Client::ClientConfiguration& clientConfiguration = Aws::Client::ClientConfiguration());

       /**
        * Initializes client to use specified credentials provider with specified client config. If http client factory is not supplied,
        * the default http client factory will be used
        */
        RedshiftArcadiaCoralServiceClient(const std::shared_ptr<Aws::Auth::AWSCredentialsProvider>& credentialsProvider,
            const Aws::Client::ClientConfiguration& clientConfiguration = Aws::Client::ClientConfiguration());

        virtual ~RedshiftArcadiaCoralServiceClient();


        /**
         * 
         */
        virtual Model::AssociateDataShareConsumerOutcome AssociateDataShareConsumer(const Model::AssociateDataShareConsumerRequest& request) const;

        /**
         * 
         *
         * returns a future to the operation so that it can be executed in parallel to other requests.
         */
        virtual Model::AssociateDataShareConsumerOutcomeCallable AssociateDataShareConsumerCallable(const Model::AssociateDataShareConsumerRequest& request) const;

        /**
         * 
         *
         * Queues the request into a thread executor and triggers associated callback when operation has finished.
         */
        virtual void AssociateDataShareConsumerAsync(const Model::AssociateDataShareConsumerRequest& request, const AssociateDataShareConsumerResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context = nullptr) const;

        /**
         * 
         */
        virtual Model::AuthorizeDataShareOutcome AuthorizeDataShare(const Model::AuthorizeDataShareRequest& request) const;

        /**
         * 
         *
         * returns a future to the operation so that it can be executed in parallel to other requests.
         */
        virtual Model::AuthorizeDataShareOutcomeCallable AuthorizeDataShareCallable(const Model::AuthorizeDataShareRequest& request) const;

        /**
         * 
         *
         * Queues the request into a thread executor and triggers associated callback when operation has finished.
         */
        virtual void AuthorizeDataShareAsync(const Model::AuthorizeDataShareRequest& request, const AuthorizeDataShareResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context = nullptr) const;

        /**
         * 
         */
        virtual Model::CreateSnapshotOutcome CreateSnapshot(const Model::CreateSnapshotRequest& request) const;

        /**
         * 
         *
         * returns a future to the operation so that it can be executed in parallel to other requests.
         */
        virtual Model::CreateSnapshotOutcomeCallable CreateSnapshotCallable(const Model::CreateSnapshotRequest& request) const;

        /**
         * 
         *
         * Queues the request into a thread executor and triggers associated callback when operation has finished.
         */
        virtual void CreateSnapshotAsync(const Model::CreateSnapshotRequest& request, const CreateSnapshotResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context = nullptr) const;

        /**
         * 
         */
        virtual Model::CreateUsageLimitOutcome CreateUsageLimit(const Model::CreateUsageLimitRequest& request) const;

        /**
         * 
         *
         * returns a future to the operation so that it can be executed in parallel to other requests.
         */
        virtual Model::CreateUsageLimitOutcomeCallable CreateUsageLimitCallable(const Model::CreateUsageLimitRequest& request) const;

        /**
         * 
         *
         * Queues the request into a thread executor and triggers associated callback when operation has finished.
         */
        virtual void CreateUsageLimitAsync(const Model::CreateUsageLimitRequest& request, const CreateUsageLimitResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context = nullptr) const;

        /**
         * 
         */
        virtual Model::DeauthorizeDataShareOutcome DeauthorizeDataShare(const Model::DeauthorizeDataShareRequest& request) const;

        /**
         * 
         *
         * returns a future to the operation so that it can be executed in parallel to other requests.
         */
        virtual Model::DeauthorizeDataShareOutcomeCallable DeauthorizeDataShareCallable(const Model::DeauthorizeDataShareRequest& request) const;

        /**
         * 
         *
         * Queues the request into a thread executor and triggers associated callback when operation has finished.
         */
        virtual void DeauthorizeDataShareAsync(const Model::DeauthorizeDataShareRequest& request, const DeauthorizeDataShareResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context = nullptr) const;

        /**
         * 
         */
        virtual Model::DeleteSnapshotOutcome DeleteSnapshot(const Model::DeleteSnapshotRequest& request) const;

        /**
         * 
         *
         * returns a future to the operation so that it can be executed in parallel to other requests.
         */
        virtual Model::DeleteSnapshotOutcomeCallable DeleteSnapshotCallable(const Model::DeleteSnapshotRequest& request) const;

        /**
         * 
         *
         * Queues the request into a thread executor and triggers associated callback when operation has finished.
         */
        virtual void DeleteSnapshotAsync(const Model::DeleteSnapshotRequest& request, const DeleteSnapshotResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context = nullptr) const;

        /**
         * 
         */
        virtual Model::DeleteUsageLimitOutcome DeleteUsageLimit(const Model::DeleteUsageLimitRequest& request) const;

        /**
         * 
         *
         * returns a future to the operation so that it can be executed in parallel to other requests.
         */
        virtual Model::DeleteUsageLimitOutcomeCallable DeleteUsageLimitCallable(const Model::DeleteUsageLimitRequest& request) const;

        /**
         * 
         *
         * Queues the request into a thread executor and triggers associated callback when operation has finished.
         */
        virtual void DeleteUsageLimitAsync(const Model::DeleteUsageLimitRequest& request, const DeleteUsageLimitResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context = nullptr) const;

        /**
         * 
         */
        virtual Model::DescribeConfigurationOutcome DescribeConfiguration(const Model::DescribeConfigurationRequest& request) const;

        /**
         * 
         *
         * returns a future to the operation so that it can be executed in parallel to other requests.
         */
        virtual Model::DescribeConfigurationOutcomeCallable DescribeConfigurationCallable(const Model::DescribeConfigurationRequest& request) const;

        /**
         * 
         *
         * Queues the request into a thread executor and triggers associated callback when operation has finished.
         */
        virtual void DescribeConfigurationAsync(const Model::DescribeConfigurationRequest& request, const DescribeConfigurationResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context = nullptr) const;

        /**
         * 
         */
        virtual Model::DescribeDataSharesOutcome DescribeDataShares(const Model::DescribeDataSharesRequest& request) const;

        /**
         * 
         *
         * returns a future to the operation so that it can be executed in parallel to other requests.
         */
        virtual Model::DescribeDataSharesOutcomeCallable DescribeDataSharesCallable(const Model::DescribeDataSharesRequest& request) const;

        /**
         * 
         *
         * Queues the request into a thread executor and triggers associated callback when operation has finished.
         */
        virtual void DescribeDataSharesAsync(const Model::DescribeDataSharesRequest& request, const DescribeDataSharesResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context = nullptr) const;

        /**
         * 
         */
        virtual Model::DescribeDataSharesForConsumerOutcome DescribeDataSharesForConsumer(const Model::DescribeDataSharesForConsumerRequest& request) const;

        /**
         * 
         *
         * returns a future to the operation so that it can be executed in parallel to other requests.
         */
        virtual Model::DescribeDataSharesForConsumerOutcomeCallable DescribeDataSharesForConsumerCallable(const Model::DescribeDataSharesForConsumerRequest& request) const;

        /**
         * 
         *
         * Queues the request into a thread executor and triggers associated callback when operation has finished.
         */
        virtual void DescribeDataSharesForConsumerAsync(const Model::DescribeDataSharesForConsumerRequest& request, const DescribeDataSharesForConsumerResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context = nullptr) const;

        /**
         * 
         */
        virtual Model::DescribeDataSharesForProducerOutcome DescribeDataSharesForProducer(const Model::DescribeDataSharesForProducerRequest& request) const;

        /**
         * 
         *
         * returns a future to the operation so that it can be executed in parallel to other requests.
         */
        virtual Model::DescribeDataSharesForProducerOutcomeCallable DescribeDataSharesForProducerCallable(const Model::DescribeDataSharesForProducerRequest& request) const;

        /**
         * 
         *
         * Queues the request into a thread executor and triggers associated callback when operation has finished.
         */
        virtual void DescribeDataSharesForProducerAsync(const Model::DescribeDataSharesForProducerRequest& request, const DescribeDataSharesForProducerResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context = nullptr) const;

        /**
         * 
         */
        virtual Model::DescribeSnapshotsOutcome DescribeSnapshots(const Model::DescribeSnapshotsRequest& request) const;

        /**
         * 
         *
         * returns a future to the operation so that it can be executed in parallel to other requests.
         */
        virtual Model::DescribeSnapshotsOutcomeCallable DescribeSnapshotsCallable(const Model::DescribeSnapshotsRequest& request) const;

        /**
         * 
         *
         * Queues the request into a thread executor and triggers associated callback when operation has finished.
         */
        virtual void DescribeSnapshotsAsync(const Model::DescribeSnapshotsRequest& request, const DescribeSnapshotsResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context = nullptr) const;

        /**
         * 
         */
        virtual Model::DescribeUsageLimitOutcome DescribeUsageLimit(const Model::DescribeUsageLimitRequest& request) const;

        /**
         * 
         *
         * returns a future to the operation so that it can be executed in parallel to other requests.
         */
        virtual Model::DescribeUsageLimitOutcomeCallable DescribeUsageLimitCallable(const Model::DescribeUsageLimitRequest& request) const;

        /**
         * 
         *
         * Queues the request into a thread executor and triggers associated callback when operation has finished.
         */
        virtual void DescribeUsageLimitAsync(const Model::DescribeUsageLimitRequest& request, const DescribeUsageLimitResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context = nullptr) const;

        /**
         * 
         */
        virtual Model::DisassociateDataShareConsumerOutcome DisassociateDataShareConsumer(const Model::DisassociateDataShareConsumerRequest& request) const;

        /**
         * 
         *
         * returns a future to the operation so that it can be executed in parallel to other requests.
         */
        virtual Model::DisassociateDataShareConsumerOutcomeCallable DisassociateDataShareConsumerCallable(const Model::DisassociateDataShareConsumerRequest& request) const;

        /**
         * 
         *
         * Queues the request into a thread executor and triggers associated callback when operation has finished.
         */
        virtual void DisassociateDataShareConsumerAsync(const Model::DisassociateDataShareConsumerRequest& request, const DisassociateDataShareConsumerResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context = nullptr) const;

        /**
         * 
         */
        virtual Model::GetCredentialsOutcome GetCredentials(const Model::GetCredentialsRequest& request) const;

        /**
         * 
         *
         * returns a future to the operation so that it can be executed in parallel to other requests.
         */
        virtual Model::GetCredentialsOutcomeCallable GetCredentialsCallable(const Model::GetCredentialsRequest& request) const;

        /**
         * 
         *
         * Queues the request into a thread executor and triggers associated callback when operation has finished.
         */
        virtual void GetCredentialsAsync(const Model::GetCredentialsRequest& request, const GetCredentialsResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context = nullptr) const;

        /**
         * 
         */
        virtual Model::ModifyUsageLimitOutcome ModifyUsageLimit(const Model::ModifyUsageLimitRequest& request) const;

        /**
         * 
         *
         * returns a future to the operation so that it can be executed in parallel to other requests.
         */
        virtual Model::ModifyUsageLimitOutcomeCallable ModifyUsageLimitCallable(const Model::ModifyUsageLimitRequest& request) const;

        /**
         * 
         *
         * Queues the request into a thread executor and triggers associated callback when operation has finished.
         */
        virtual void ModifyUsageLimitAsync(const Model::ModifyUsageLimitRequest& request, const ModifyUsageLimitResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context = nullptr) const;

        /**
         * 
         */
        virtual Model::RejectDataShareOutcome RejectDataShare(const Model::RejectDataShareRequest& request) const;

        /**
         * 
         *
         * returns a future to the operation so that it can be executed in parallel to other requests.
         */
        virtual Model::RejectDataShareOutcomeCallable RejectDataShareCallable(const Model::RejectDataShareRequest& request) const;

        /**
         * 
         *
         * Queues the request into a thread executor and triggers associated callback when operation has finished.
         */
        virtual void RejectDataShareAsync(const Model::RejectDataShareRequest& request, const RejectDataShareResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context = nullptr) const;

        /**
         * 
         */
        virtual Model::RestoreFromSnapshotOutcome RestoreFromSnapshot(const Model::RestoreFromSnapshotRequest& request) const;

        /**
         * 
         *
         * returns a future to the operation so that it can be executed in parallel to other requests.
         */
        virtual Model::RestoreFromSnapshotOutcomeCallable RestoreFromSnapshotCallable(const Model::RestoreFromSnapshotRequest& request) const;

        /**
         * 
         *
         * Queues the request into a thread executor and triggers associated callback when operation has finished.
         */
        virtual void RestoreFromSnapshotAsync(const Model::RestoreFromSnapshotRequest& request, const RestoreFromSnapshotResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context = nullptr) const;

        /**
         * 
         */
        virtual Model::SetConfigurationOutcome SetConfiguration(const Model::SetConfigurationRequest& request) const;

        /**
         * 
         *
         * returns a future to the operation so that it can be executed in parallel to other requests.
         */
        virtual Model::SetConfigurationOutcomeCallable SetConfigurationCallable(const Model::SetConfigurationRequest& request) const;

        /**
         * 
         *
         * Queues the request into a thread executor and triggers associated callback when operation has finished.
         */
        virtual void SetConfigurationAsync(const Model::SetConfigurationRequest& request, const SetConfigurationResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context = nullptr) const;


      void OverrideEndpoint(const Aws::String& endpoint);
    private:
      void init(const Aws::Client::ClientConfiguration& clientConfiguration);
        void AssociateDataShareConsumerAsyncHelper(const Model::AssociateDataShareConsumerRequest& request, const AssociateDataShareConsumerResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) const;
        void AuthorizeDataShareAsyncHelper(const Model::AuthorizeDataShareRequest& request, const AuthorizeDataShareResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) const;
        void CreateSnapshotAsyncHelper(const Model::CreateSnapshotRequest& request, const CreateSnapshotResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) const;
        void CreateUsageLimitAsyncHelper(const Model::CreateUsageLimitRequest& request, const CreateUsageLimitResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) const;
        void DeauthorizeDataShareAsyncHelper(const Model::DeauthorizeDataShareRequest& request, const DeauthorizeDataShareResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) const;
        void DeleteSnapshotAsyncHelper(const Model::DeleteSnapshotRequest& request, const DeleteSnapshotResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) const;
        void DeleteUsageLimitAsyncHelper(const Model::DeleteUsageLimitRequest& request, const DeleteUsageLimitResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) const;
        void DescribeConfigurationAsyncHelper(const Model::DescribeConfigurationRequest& request, const DescribeConfigurationResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) const;
        void DescribeDataSharesAsyncHelper(const Model::DescribeDataSharesRequest& request, const DescribeDataSharesResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) const;
        void DescribeDataSharesForConsumerAsyncHelper(const Model::DescribeDataSharesForConsumerRequest& request, const DescribeDataSharesForConsumerResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) const;
        void DescribeDataSharesForProducerAsyncHelper(const Model::DescribeDataSharesForProducerRequest& request, const DescribeDataSharesForProducerResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) const;
        void DescribeSnapshotsAsyncHelper(const Model::DescribeSnapshotsRequest& request, const DescribeSnapshotsResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) const;
        void DescribeUsageLimitAsyncHelper(const Model::DescribeUsageLimitRequest& request, const DescribeUsageLimitResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) const;
        void DisassociateDataShareConsumerAsyncHelper(const Model::DisassociateDataShareConsumerRequest& request, const DisassociateDataShareConsumerResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) const;
        void GetCredentialsAsyncHelper(const Model::GetCredentialsRequest& request, const GetCredentialsResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) const;
        void ModifyUsageLimitAsyncHelper(const Model::ModifyUsageLimitRequest& request, const ModifyUsageLimitResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) const;
        void RejectDataShareAsyncHelper(const Model::RejectDataShareRequest& request, const RejectDataShareResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) const;
        void RestoreFromSnapshotAsyncHelper(const Model::RestoreFromSnapshotRequest& request, const RestoreFromSnapshotResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) const;
        void SetConfigurationAsyncHelper(const Model::SetConfigurationRequest& request, const SetConfigurationResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) const;

      Aws::String m_uri;
      Aws::String m_configScheme;
      std::shared_ptr<Aws::Utils::Threading::Executor> m_executor;
  };

} // namespace RedshiftArcadiaCoralService
} // namespace Aws
