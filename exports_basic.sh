#!/bin/sh
export OPENSSL_LIB_DIR=$(pwd)/src/odbc/rsodbc/iam/linux/openssl/1.1.1/centos7/gcc5_5/release64/lib
export AWS_SDK_LIB_DIR=$(pwd)/src/odbc/rsodbc/iam/linux/aws-cpp-sdk/1.9.289/centos7/gcc5_5/release64/lib
export CURL_LIB_DIR=$(pwd)/src/odbc/rsodbc/iam/linux/libcURL/7.78.0_ssl1.1.1_zlib1.2.11_threaded_resolver/centos7/gcc5_5/release64/lib

export OPENSSL_INC_DIR=$(pwd)/src/odbc/rsodbc/iam/linux/openssl/1.1.1/centos7/gcc5_5/release64/include
export AWS_SDK_INC_DIR=$(pwd)/src/odbc/rsodbc/iam/linux/aws-cpp-sdk/1.9.289/include
export CURL_INC_DIR=$(pwd)/src/odbc/rsodbc/iam/linux/libcURL/7.78.0_ssl1.1.1_zlib1.2.11_threaded_resolver/centos7/gcc5_5/release64/include

