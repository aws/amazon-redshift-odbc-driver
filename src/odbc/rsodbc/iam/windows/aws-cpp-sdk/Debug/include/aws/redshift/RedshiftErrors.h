﻿/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once

#include <aws/core/client/AWSError.h>
#include <aws/core/client/CoreErrors.h>
#include <aws/redshift/Redshift_EXPORTS.h>

namespace Aws
{
namespace Redshift
{
enum class RedshiftErrors
{
  //From Core//
  //////////////////////////////////////////////////////////////////////////////////////////
  INCOMPLETE_SIGNATURE = 0,
  INTERNAL_FAILURE = 1,
  INVALID_ACTION = 2,
  INVALID_CLIENT_TOKEN_ID = 3,
  INVALID_PARAMETER_COMBINATION = 4,
  INVALID_QUERY_PARAMETER = 5,
  INVALID_PARAMETER_VALUE = 6,
  MISSING_ACTION = 7, // SDK should never allow
  MISSING_AUTHENTICATION_TOKEN = 8, // SDK should never allow
  MISSING_PARAMETER = 9, // SDK should never allow
  OPT_IN_REQUIRED = 10,
  REQUEST_EXPIRED = 11,
  SERVICE_UNAVAILABLE = 12,
  THROTTLING = 13,
  VALIDATION = 14,
  ACCESS_DENIED = 15,
  RESOURCE_NOT_FOUND = 16,
  UNRECOGNIZED_CLIENT = 17,
  MALFORMED_QUERY_STRING = 18,
  SLOW_DOWN = 19,
  REQUEST_TIME_TOO_SKEWED = 20,
  INVALID_SIGNATURE = 21,
  SIGNATURE_DOES_NOT_MATCH = 22,
  INVALID_ACCESS_KEY_ID = 23,
  REQUEST_TIMEOUT = 24,
  NETWORK_CONNECTION = 99,

  UNKNOWN = 100,
  ///////////////////////////////////////////////////////////////////////////////////////////

  ACCESS_TO_CLUSTER_DENIED_FAULT= static_cast<int>(Aws::Client::CoreErrors::SERVICE_EXTENSION_START_RANGE) + 1,
  ACCESS_TO_SNAPSHOT_DENIED_FAULT,
  AUTHENTICATION_PROFILE_ALREADY_EXISTS_FAULT,
  AUTHENTICATION_PROFILE_NOT_FOUND_FAULT,
  AUTHENTICATION_PROFILE_QUOTA_EXCEEDED_FAULT,
  AUTHORIZATION_ALREADY_EXISTS_FAULT,
  AUTHORIZATION_NOT_FOUND_FAULT,
  AUTHORIZATION_QUOTA_EXCEEDED_FAULT,
  BATCH_DELETE_REQUEST_SIZE_EXCEEDED_FAULT,
  BATCH_MODIFY_CLUSTER_SNAPSHOTS_LIMIT_EXCEEDED_FAULT,
  BUCKET_NOT_FOUND_FAULT,
  CLUSTER_ALREADY_EXISTS_FAULT,
  CLUSTER_NOT_FOUND_FAULT,
  CLUSTER_ON_LATEST_REVISION_FAULT,
  CLUSTER_PARAMETER_GROUP_ALREADY_EXISTS_FAULT,
  CLUSTER_PARAMETER_GROUP_NOT_FOUND_FAULT,
  CLUSTER_PARAMETER_GROUP_QUOTA_EXCEEDED_FAULT,
  CLUSTER_QUOTA_EXCEEDED_FAULT,
  CLUSTER_SECURITY_GROUP_ALREADY_EXISTS_FAULT,
  CLUSTER_SECURITY_GROUP_NOT_FOUND_FAULT,
  CLUSTER_SECURITY_GROUP_QUOTA_EXCEEDED_FAULT,
  CLUSTER_SNAPSHOT_ALREADY_EXISTS_FAULT,
  CLUSTER_SNAPSHOT_NOT_FOUND_FAULT,
  CLUSTER_SNAPSHOT_QUOTA_EXCEEDED_FAULT,
  CLUSTER_SUBNET_GROUP_ALREADY_EXISTS_FAULT,
  CLUSTER_SUBNET_GROUP_NOT_FOUND_FAULT,
  CLUSTER_SUBNET_GROUP_QUOTA_EXCEEDED_FAULT,
  CLUSTER_SUBNET_QUOTA_EXCEEDED_FAULT,
  CONFLICT_POLICY_UPDATE_FAULT,
  COPY_TO_REGION_DISABLED_FAULT,
  CUSTOM_CNAME_ASSOCIATION_FAULT,
  CUSTOM_DOMAIN_ASSOCIATION_NOT_FOUND_FAULT,
  DEPENDENT_SERVICE_ACCESS_DENIED_FAULT,
  DEPENDENT_SERVICE_REQUEST_THROTTLING_FAULT,
  DEPENDENT_SERVICE_UNAVAILABLE_FAULT,
  ENDPOINTS_PER_AUTHORIZATION_LIMIT_EXCEEDED_FAULT,
  ENDPOINTS_PER_CLUSTER_LIMIT_EXCEEDED_FAULT,
  ENDPOINT_ALREADY_EXISTS_FAULT,
  ENDPOINT_AUTHORIZATIONS_PER_CLUSTER_LIMIT_EXCEEDED_FAULT,
  ENDPOINT_AUTHORIZATION_ALREADY_EXISTS_FAULT,
  ENDPOINT_AUTHORIZATION_NOT_FOUND_FAULT,
  ENDPOINT_NOT_FOUND_FAULT,
  EVENT_SUBSCRIPTION_QUOTA_EXCEEDED_FAULT,
  HSM_CLIENT_CERTIFICATE_ALREADY_EXISTS_FAULT,
  HSM_CLIENT_CERTIFICATE_NOT_FOUND_FAULT,
  HSM_CLIENT_CERTIFICATE_QUOTA_EXCEEDED_FAULT,
  HSM_CONFIGURATION_ALREADY_EXISTS_FAULT,
  HSM_CONFIGURATION_NOT_FOUND_FAULT,
  HSM_CONFIGURATION_QUOTA_EXCEEDED_FAULT,
  INCOMPATIBLE_ORDERABLE_OPTIONS,
  INSUFFICIENT_CLUSTER_CAPACITY_FAULT,
  INSUFFICIENT_S3_BUCKET_POLICY_FAULT,
  INTEGRATION_NOT_FOUND_FAULT,
  INVALID_AUTHENTICATION_PROFILE_REQUEST_FAULT,
  INVALID_AUTHORIZATION_STATE_FAULT,
  INVALID_CLUSTER_PARAMETER_GROUP_STATE_FAULT,
  INVALID_CLUSTER_SECURITY_GROUP_STATE_FAULT,
  INVALID_CLUSTER_SNAPSHOT_SCHEDULE_STATE_FAULT,
  INVALID_CLUSTER_SNAPSHOT_STATE_FAULT,
  INVALID_CLUSTER_STATE_FAULT,
  INVALID_CLUSTER_SUBNET_GROUP_STATE_FAULT,
  INVALID_CLUSTER_SUBNET_STATE_FAULT,
  INVALID_CLUSTER_TRACK_FAULT,
  INVALID_DATA_SHARE_FAULT,
  INVALID_ELASTIC_IP_FAULT,
  INVALID_ENDPOINT_STATE_FAULT,
  INVALID_HSM_CLIENT_CERTIFICATE_STATE_FAULT,
  INVALID_HSM_CONFIGURATION_STATE_FAULT,
  INVALID_NAMESPACE_FAULT,
  INVALID_POLICY_FAULT,
  INVALID_RESERVED_NODE_STATE_FAULT,
  INVALID_RESTORE_FAULT,
  INVALID_RETENTION_PERIOD_FAULT,
  INVALID_S3_BUCKET_NAME_FAULT,
  INVALID_S3_KEY_PREFIX_FAULT,
  INVALID_SCHEDULED_ACTION_FAULT,
  INVALID_SCHEDULE_FAULT,
  INVALID_SNAPSHOT_COPY_GRANT_STATE_FAULT,
  INVALID_SUBNET,
  INVALID_SUBSCRIPTION_STATE_FAULT,
  INVALID_TABLE_RESTORE_ARGUMENT_FAULT,
  INVALID_TAG_FAULT,
  INVALID_USAGE_LIMIT_FAULT,
  INVALID_V_P_C_NETWORK_STATE_FAULT,
  IN_PROGRESS_TABLE_RESTORE_QUOTA_EXCEEDED_FAULT,
  IPV6_CIDR_BLOCK_NOT_FOUND_FAULT,
  LIMIT_EXCEEDED_FAULT,
  NUMBER_OF_NODES_PER_CLUSTER_LIMIT_EXCEEDED_FAULT,
  NUMBER_OF_NODES_QUOTA_EXCEEDED_FAULT,
  PARTNER_NOT_FOUND_FAULT,
  REDSHIFT_IDC_APPLICATION_ALREADY_EXISTS_FAULT,
  REDSHIFT_IDC_APPLICATION_NOT_EXISTS_FAULT,
  REDSHIFT_IDC_APPLICATION_QUOTA_EXCEEDED_FAULT,
  RESERVED_NODE_ALREADY_EXISTS_FAULT,
  RESERVED_NODE_ALREADY_MIGRATED_FAULT,
  RESERVED_NODE_EXCHANGE_NOT_FOUND_FAULT,
  RESERVED_NODE_NOT_FOUND_FAULT,
  RESERVED_NODE_OFFERING_NOT_FOUND_FAULT,
  RESERVED_NODE_QUOTA_EXCEEDED_FAULT,
  RESIZE_NOT_FOUND_FAULT,
  RESOURCE_NOT_FOUND_FAULT,
  SCHEDULED_ACTION_ALREADY_EXISTS_FAULT,
  SCHEDULED_ACTION_NOT_FOUND_FAULT,
  SCHEDULED_ACTION_QUOTA_EXCEEDED_FAULT,
  SCHEDULED_ACTION_TYPE_UNSUPPORTED_FAULT,
  SCHEDULE_DEFINITION_TYPE_UNSUPPORTED_FAULT,
  SNAPSHOT_COPY_ALREADY_DISABLED_FAULT,
  SNAPSHOT_COPY_ALREADY_ENABLED_FAULT,
  SNAPSHOT_COPY_DISABLED_FAULT,
  SNAPSHOT_COPY_GRANT_ALREADY_EXISTS_FAULT,
  SNAPSHOT_COPY_GRANT_NOT_FOUND_FAULT,
  SNAPSHOT_COPY_GRANT_QUOTA_EXCEEDED_FAULT,
  SNAPSHOT_SCHEDULE_ALREADY_EXISTS_FAULT,
  SNAPSHOT_SCHEDULE_NOT_FOUND_FAULT,
  SNAPSHOT_SCHEDULE_QUOTA_EXCEEDED_FAULT,
  SNAPSHOT_SCHEDULE_UPDATE_IN_PROGRESS_FAULT,
  SOURCE_NOT_FOUND_FAULT,
  SUBNET_ALREADY_IN_USE,
  SUBSCRIPTION_ALREADY_EXIST_FAULT,
  SUBSCRIPTION_CATEGORY_NOT_FOUND_FAULT,
  SUBSCRIPTION_EVENT_ID_NOT_FOUND_FAULT,
  SUBSCRIPTION_NOT_FOUND_FAULT,
  SUBSCRIPTION_SEVERITY_NOT_FOUND_FAULT,
  S_N_S_INVALID_TOPIC_FAULT,
  S_N_S_NO_AUTHORIZATION_FAULT,
  S_N_S_TOPIC_ARN_NOT_FOUND_FAULT,
  TABLE_LIMIT_EXCEEDED_FAULT,
  TABLE_RESTORE_NOT_FOUND_FAULT,
  TAG_LIMIT_EXCEEDED_FAULT,
  UNAUTHORIZED_OPERATION,
  UNAUTHORIZED_PARTNER_INTEGRATION_FAULT,
  UNKNOWN_SNAPSHOT_COPY_REGION_FAULT,
  UNSUPPORTED_OPERATION_FAULT,
  UNSUPPORTED_OPTION_FAULT,
  USAGE_LIMIT_ALREADY_EXISTS_FAULT,
  USAGE_LIMIT_NOT_FOUND_FAULT
};

class AWS_REDSHIFT_API RedshiftError : public Aws::Client::AWSError<RedshiftErrors>
{
public:
  RedshiftError() {}
  RedshiftError(const Aws::Client::AWSError<Aws::Client::CoreErrors>& rhs) : Aws::Client::AWSError<RedshiftErrors>(rhs) {}
  RedshiftError(Aws::Client::AWSError<Aws::Client::CoreErrors>&& rhs) : Aws::Client::AWSError<RedshiftErrors>(rhs) {}
  RedshiftError(const Aws::Client::AWSError<RedshiftErrors>& rhs) : Aws::Client::AWSError<RedshiftErrors>(rhs) {}
  RedshiftError(Aws::Client::AWSError<RedshiftErrors>&& rhs) : Aws::Client::AWSError<RedshiftErrors>(rhs) {}

  template <typename T>
  T GetModeledError();
};

namespace RedshiftErrorMapper
{
  AWS_REDSHIFT_API Aws::Client::AWSError<Aws::Client::CoreErrors> GetErrorForName(const char* errorName);
}

} // namespace Redshift
} // namespace Aws
