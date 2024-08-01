################################################################################
# Automatically-generated file. Do not edit!
################################################################################

USER_OBJS :=

THIRD_PARTY_LIBS := ${ROOT_DIR}/src/logging/librslog.a $(AWS_SDK_LIB_DIR)/libaws-cpp-sdk-redshift.a $(AWS_SDK_LIB_DIR)/libaws-cpp-sdk-sso-oidc.a $(AWS_SDK_LIB_DIR)/libaws-cpp-sdk-redshift-serverless.a $(AWS_SDK_LIB_DIR)/libaws-cpp-sdk-sts.a $(AWS_SDK_LIB_DIR)/libaws-cpp-sdk-core.a $(AWS_SDK_LIB_DIR)/libaws-crt-cpp.a $(AWS_SDK_LIB_DIR)/libaws-c-s3.a $(AWS_SDK_LIB_DIR)/libaws-c-auth.a $(AWS_SDK_LIB_DIR)/libaws-c-event-stream.a $(AWS_SDK_LIB_DIR)/libaws-c-http.a $(AWS_SDK_LIB_DIR)/libaws-c-mqtt.a $(AWS_SDK_LIB_DIR)/libaws-c-io.a $(AWS_SDK_LIB_DIR)/libaws-c-cal.a $(AWS_SDK_LIB_DIR)/libaws-checksums.a $(AWS_SDK_LIB_DIR)/libaws-c-compression.a $(AWS_SDK_LIB_DIR)/libaws-c-common.a $(AWS_SDK_LIB_DIR)/libs2n.a $(AWS_SDK_LIB_DIR)/libaws-c-sdkutils.a $(CURL_LIB_DIR)/libcurl.a $(OPENSSL_LIB_DIR)/libssl.a $(OPENSSL_LIB_DIR)/libcrypto.a
ifdef DEPENDENCY_DIR
    THIRD_PARTY_LIBS += $(DEPENDENCY_DIR)/lib/libnghttp2.a $(DEPENDENCY_DIR)/lib/libcares.a $(DEPENDENCY_DIR)/lib/libkeyutils.a $(DEPENDENCY_DIR)/lib/libz.a
endif

LIBS := $(THIRD_PARTY_LIBS) -lpq -lpgport -lpthread -lkrb5 -lgssapi_krb5 -lz -ldl -lrt -lresolv

