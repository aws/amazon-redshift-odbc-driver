#ifndef AWS_SDKUTILS_SDKUTILS_H
#define AWS_SDKUTILS_SDKUTILS_H
/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#include <aws/common/common.h>
#include <aws/common/logging.h>

#include <aws/sdkutils/exports.h>

struct aws_allocator;

#define AWS_C_SDKUTILS_PACKAGE_ID 15

enum aws_sdkutils_errors {
    AWS_ERROR_SDKUTILS_GENERAL = AWS_ERROR_ENUM_BEGIN_RANGE(AWS_C_SDKUTILS_PACKAGE_ID),
    AWS_ERROR_SDKUTILS_PARSE_FATAL,
    AWS_ERROR_SDKUTILS_PARSE_RECOVERABLE,

    AWS_ERROR_SDKUTILS_END_RANGE = AWS_ERROR_ENUM_END_RANGE(AWS_C_SDKUTILS_PACKAGE_ID)
};

enum aws_sdkutils_log_subject {
    AWS_LS_SDKUTILS_GENERAL = AWS_LOG_SUBJECT_BEGIN_RANGE(AWS_C_SDKUTILS_PACKAGE_ID),
    AWS_LS_SDKUTILS_PROFILE,

    AWS_LS_SDKUTILS_LAST = AWS_LOG_SUBJECT_END_RANGE(AWS_C_SDKUTILS_PACKAGE_ID)
};

AWS_EXTERN_C_BEGIN

AWS_SDKUTILS_API void aws_sdkutils_library_init(struct aws_allocator *allocator);
AWS_SDKUTILS_API void aws_sdkutils_library_clean_up(void);

AWS_EXTERN_C_END

#endif /* AWS_SDKUTILS_SDKUTILS_H */