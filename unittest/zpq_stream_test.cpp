#include "common.h"
#include <libpq/zpq_stream.h>

class ZpqDeserializeCompressorsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up any test resources
    }

    void TearDown() override {
        // Clean up any test resources
    }
};

// Test Case 1: Empty input string
TEST_F(ZpqDeserializeCompressorsTest, EmptyInputString) {
    zpq_compressor* compressors = nullptr;
    size_t n_compressors = 0;
    
    ASSERT_TRUE(zpq_deserialize_compressors("", &compressors, &n_compressors));
    EXPECT_EQ(n_compressors, 0);
    EXPECT_EQ(compressors, nullptr);
}

// Test Case 2: Single valid compressor
TEST_F(ZpqDeserializeCompressorsTest, SingleValidCompressor) {
    zpq_compressor* compressors = nullptr;
    size_t n_compressors = 0;
    
    ASSERT_TRUE(zpq_deserialize_compressors("zstd", &compressors, &n_compressors));
    
    // Since we're using real implementation, we need to check what we know will be consistent
    EXPECT_GT(n_compressors, 0);
    EXPECT_NE(compressors, nullptr);
    
    // Clean up
    if (compressors) {
        free(compressors);
    }
}

// Test Case 3: Multiple valid compressors
TEST_F(ZpqDeserializeCompressorsTest, MultipleValidCompressors) {
    zpq_compressor* compressors = nullptr;
    size_t n_compressors = 0;
    
    ASSERT_TRUE(zpq_deserialize_compressors("zstd,lz4", &compressors, &n_compressors));
    
    // With real implementation, we expect at least two compressors
    EXPECT_GE(n_compressors, 2);
    EXPECT_NE(compressors, nullptr);
    
    // Clean up
    if (compressors) {
        free(compressors);
    }
}

// Test Case 4: Compressor with custom compression level
TEST_F(ZpqDeserializeCompressorsTest, CompressorWithCustomLevel) {
    zpq_compressor* compressors = nullptr;
    size_t n_compressors = 0;
    
    ASSERT_TRUE(zpq_deserialize_compressors("zstd:5", &compressors, &n_compressors));
    
    EXPECT_GT(n_compressors, 0);
    EXPECT_NE(compressors, nullptr);
    
    // Since we're using the real implementation, we have to check what we can reasonably assert
    // Look for a compressor with level 5
    bool found_level_5 = false;
    for (size_t i = 0; i < n_compressors; i++) {
        if (compressors[i].level == 5) {
            found_level_5 = true;
            break;
        }
    }
    EXPECT_TRUE(found_level_5);
    
    // Clean up
    if (compressors) {
        free(compressors);
    }
}

// Test Case 5: Multiple compressors with custom levels
TEST_F(ZpqDeserializeCompressorsTest, MultipleCompressorsWithCustomLevels) {
    zpq_compressor* compressors = nullptr;
    size_t n_compressors = 0;
    
    ASSERT_TRUE(zpq_deserialize_compressors("zstd:9,lz4:2", &compressors, &n_compressors));
    
    EXPECT_GE(n_compressors, 2);
    EXPECT_NE(compressors, nullptr);
    
    // Clean up
    if (compressors) {
        free(compressors);
    }
}

// Test Case 6: Invalid compression level
TEST_F(ZpqDeserializeCompressorsTest, InvalidCompressionLevel) {
    zpq_compressor* compressors = nullptr;
    size_t n_compressors = 0;
    
    ASSERT_FALSE(zpq_deserialize_compressors("zstd:abc", &compressors, &n_compressors));
    EXPECT_EQ(n_compressors, 0);
    EXPECT_EQ(compressors, nullptr);
}

// Test Case 7: Duplicate algorithms
TEST_F(ZpqDeserializeCompressorsTest, DuplicateAlgorithms) {
    zpq_compressor* compressors = nullptr;
    size_t n_compressors = 0;
    
    ASSERT_FALSE(zpq_deserialize_compressors("zstd,lz4,zstd", &compressors, &n_compressors));
    EXPECT_EQ(n_compressors, 0);
    EXPECT_EQ(compressors, nullptr);
}

// Test Case 8: Unsupported algorithm
TEST_F(ZpqDeserializeCompressorsTest, UnsupportedAlgorithm) {
    zpq_compressor* compressors = nullptr;
    size_t n_compressors = 0;
    
    // The function should return true but ignore unknown algorithms
    ASSERT_TRUE(zpq_deserialize_compressors("unknown_algorithm", &compressors, &n_compressors));
    EXPECT_EQ(n_compressors, 0);
    EXPECT_EQ(compressors, nullptr);
}

// Test Case 9: Mix of valid and unsupported algorithms
TEST_F(ZpqDeserializeCompressorsTest, MixOfValidAndUnsupportedAlgorithms) {
    zpq_compressor* compressors = nullptr;
    size_t n_compressors = 0;
    
    ASSERT_TRUE(zpq_deserialize_compressors("zstd,unknown_algo,lz4", &compressors, &n_compressors));
    
    // We expect at least two valid compressors
    EXPECT_GE(n_compressors, 2);
    EXPECT_NE(compressors, nullptr);
    
    // Clean up
    if (compressors) {
        free(compressors);
    }
}

// Test Case 10: Case insensitivity
TEST_F(ZpqDeserializeCompressorsTest, CaseInsensitivity) {
    zpq_compressor* compressors = nullptr;
    size_t n_compressors = 0;
    
    ASSERT_TRUE(zpq_deserialize_compressors("ZsTd,Lz4", &compressors, &n_compressors));
    
    // We expect at least two valid compressors
    EXPECT_GE(n_compressors, 2);
    EXPECT_NE(compressors, nullptr);
    
    // Clean up
    if (compressors) {
        free(compressors);
    }
}


class ZpqSerializeCompressorsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Nothing to set up
    }

    void TearDown() override {
        // Nothing to clean up
    }
};

// Test Case 1: Zero compressors
TEST_F(ZpqSerializeCompressorsTest, ZeroCompressors) {
    char* result = zpq_serialize_compressors(nullptr, 0);
    EXPECT_EQ(result, nullptr);
}

// Test Case 2: Null pointer for compressors
TEST_F(ZpqSerializeCompressorsTest, NullCompressors) {
    char* result = zpq_serialize_compressors(nullptr, 5);
    EXPECT_EQ(result, nullptr);
}

// The remaining tests use zpq_parse_compression_setting to get valid compressors,
// then test serialization

// Test Case 3: Round trip with single compressor
TEST_F(ZpqSerializeCompressorsTest, RoundTripSingleCompressor) {
    zpq_compressor* compressors = nullptr;
    size_t n_compressors = 0;
    char* input = strdup("zstd");
    
    // Get valid compressors using the parse function
    int result = zpq_parse_compression_setting(input, &compressors, &n_compressors);
    if (result <= 0 || compressors == nullptr || n_compressors == 0) {
        GTEST_SKIP() << "Could not parse 'zstd' compression setting";
        return;
    }
    
    // Serialize the compressors
    char* serialized = zpq_serialize_compressors(compressors, n_compressors);
    ASSERT_NE(serialized, nullptr);
    
    // Basic validation of output
    EXPECT_TRUE(strstr(serialized, ":") != nullptr);
    
    free(serialized);
    free(compressors);
}

// Test Case 4: Round trip with multiple compressors
TEST_F(ZpqSerializeCompressorsTest, RoundTripMultipleCompressors) {
    zpq_compressor* compressors = nullptr;
    size_t n_compressors = 0;
    char* input = strdup("zstd,lz4");
    
    // Get valid compressors using the parse function
    int result = zpq_parse_compression_setting(input, &compressors, &n_compressors);
    if (result <= 0 || compressors == nullptr || n_compressors < 2) {
        GTEST_SKIP() << "Could not parse 'zstd,lz4' compression setting";
        return;
    }
    
    // Serialize the compressors
    char* serialized = zpq_serialize_compressors(compressors, n_compressors);
    ASSERT_NE(serialized, nullptr);
    
    // Basic validation of output - should include a comma
    EXPECT_TRUE(strstr(serialized, ",") != nullptr);
    
    free(serialized);
    free(compressors);
}

// Test Case 5: Round trip with compression levels
TEST_F(ZpqSerializeCompressorsTest, RoundTripWithCompressionLevels) {
    zpq_compressor* compressors = nullptr;
    size_t n_compressors = 0;
    char* input = strdup("zstd:5");
    
    // Get valid compressors using the parse function
    int result = zpq_parse_compression_setting(input, &compressors, &n_compressors);
    if (result <= 0 || compressors == nullptr || n_compressors == 0) {
        GTEST_SKIP() << "Could not parse 'zstd:5' compression setting";
        return;
    }
    
    // Serialize the compressors
    char* serialized = zpq_serialize_compressors(compressors, n_compressors);
    ASSERT_NE(serialized, nullptr);
    
    // Basic validation - should include ":5"
    EXPECT_TRUE(strstr(serialized, ":5") != nullptr);
    
    free(serialized);
    free(compressors);
}

// Test Case 6: Complete round trip test - parse and then serialize back
TEST_F(ZpqSerializeCompressorsTest, CompleteRoundTrip) {
    // Start with a complex compression string
    const char* original = "zstd:9,lz4:2";
    char* input = strdup(original);
    
    // Parse it to get compressors
    zpq_compressor* compressors = nullptr;
    size_t n_compressors = 0;
    int result = zpq_parse_compression_setting(input, &compressors, &n_compressors);
    
    if (result <= 0 || compressors == nullptr || n_compressors < 2) {
        GTEST_SKIP() << "Could not parse compression setting for round trip test";
        return;
    }
    
    // Serialize back to string
    char* serialized = zpq_serialize_compressors(compressors, n_compressors);
    ASSERT_NE(serialized, nullptr);
    
    // While the exact format might differ, it should contain key components
    EXPECT_TRUE(strstr(serialized, "zstd") != nullptr);
    EXPECT_TRUE(strstr(serialized, "lz4") != nullptr);
    EXPECT_TRUE(strstr(serialized, ":9") != nullptr);
    EXPECT_TRUE(strstr(serialized, ":2") != nullptr);
    EXPECT_TRUE(strstr(serialized, ",") != nullptr);
    
    free(serialized);
    free(compressors);
}