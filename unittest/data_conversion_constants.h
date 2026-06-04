/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#pragma once

// Constants for buffer sizes
constexpr int CHAR_BUFFER_SIZE_T = 256;
constexpr int UTF16_BUFFER_SIZE_T = 512;

// Positive and zero
#define ZERO "0"
#define ONE "1"
#define TWO "2"
#define POS_127 "127"
#define POS_128 "128"
#define POS_255 "255"
#define POS_256 "256"
#define POS_12345 "12345"
#define POS_32767 "32767"
#define POS_32768 "32768"
#define POS_65535 "65535"
#define POS_65536 "65536"
#define POS_2147483647 "2147483647"
#define POS_2147483648 "2147483648"
#define POS_4294967295 "4294967295"
#define POS_4294967296 "4294967296"
#define POS_9_22E18 "9.22E+18"

// Negative values
#define NEG_ONE "-1"
#define NEG_128 "-128"
#define NEG_129 "-129"
#define NEG_32768 "-32768"
#define NEG_32769 "-32769"
#define NEG_2_15E9 "-2.15E+09"
#define NEG_9_22E18 "-9.22E+18"

// Min/Max big integers
#define MAX_BIGINT "9223372036854775807"
#define MIN_BIGINT "-9223372036854775808"

// Aliases for documentation clarity and symbolic access
#define MaxBInt MAX_BIGINT
#define MinBInt MIN_BIGINT

#define MaxInt POS_2147483647
#define MaxIntP1 POS_2147483648
#define MaxUInt POS_4294967295
#define MaxUIntP1 POS_4294967296

#define MaxSInt POS_32767
#define MaxSIntP1 POS_32768

#define MaxUSInt POS_65535
#define MaxUSIntP1 POS_65536

#define MaxTInt POS_127
#define MaxTIntP1 POS_128

#define MaxUTInt POS_255
#define MaxUTIntP1 POS_256

#define MinInt "-2147483648"
#define MinIntM1 "-2147483649"
#define MinSInt NEG_32768
#define MinSIntM1 NEG_32769
#define MinTInt NEG_128
#define MinTIntM1 NEG_129
#define MinusOne NEG_ONE

#define FLOAT_0_9 "0.9"
#define FLOAT_1_0 "1.0"
#define FLOAT_2_0 "2.0"

#define NULL_VAL "" // for testing empty strings / NULLs
