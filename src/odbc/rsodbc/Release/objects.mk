################################################################################
# Automatically-generated file. Do not edit!
################################################################################

USER_OBJS :=

#LIBS := -lpq -lpgport -lpthread -lssl -lcrypto -lkrb5 -lgssapi_krb5 -lz -ldl -lrt
LIBS := -lpq -lpgport -lpthread -lkrb5 -lgssapi_krb5 -lz -ldl -lrt

AWS_SDK_LIB_PATH := ../iam/linux/aws-cpp-sdk/1.9.136/centos7/gcc5_5/release64/lib
LIBCURL_PATH := ../iam/linux/libcURL/7.78.0_ssl1.1.1_zlib1.2.11_threaded_resolver/centos7/gcc5_5/release64/lib
OPENSSL_LIB_PATH := ../iam/linux/openssl/1.1.1/centos7/gcc5_5/release64/lib

THIRD_PARTY_LIBS := $(AWS_SDK_LIB_PATH)/libaws-cpp-sdk-redshift.a $(AWS_SDK_LIB_PATH)/libaws-cpp-sdk-redshiftarcadiacoral.a $(AWS_SDK_LIB_PATH)/libaws-cpp-sdk-sts.a $(AWS_SDK_LIB_PATH)/libaws-cpp-sdk-core.a $(AWS_SDK_LIB_PATH)/libaws-crt-cpp.a $(AWS_SDK_LIB_PATH)/libaws-c-s3.a $(AWS_SDK_LIB_PATH)/libaws-c-auth.a $(AWS_SDK_LIB_PATH)/libaws-c-event-stream.a $(AWS_SDK_LIB_PATH)/libaws-c-http.a $(AWS_SDK_LIB_PATH)/libaws-c-mqtt.a $(AWS_SDK_LIB_PATH)/libaws-c-io.a $(AWS_SDK_LIB_PATH)/libaws-c-cal.a $(AWS_SDK_LIB_PATH)/libaws-checksums.a $(AWS_SDK_LIB_PATH)/libaws-c-compression.a $(AWS_SDK_LIB_PATH)/libaws-c-common.a $(AWS_SDK_LIB_PATH)/libs2n.a ../iam/linux/libcURL/7.78.0_ssl1.1.1_zlib1.2.11_threaded_resolver/centos7/gcc5_5/release64/lib/libcurl.a $(OPENSSL_LIB_PATH)/libssl.a $(OPENSSL_LIB_PATH)/libcrypto.a

