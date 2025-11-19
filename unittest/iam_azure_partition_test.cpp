#include "iam/core/IAMUtils.h"
#include <gtest/gtest.h>

using namespace Redshift::IamSupport;

#define COMMERCIAL_REGION "https://login.microsoftonline.com"
#define GOV_REGION "https://login.microsoftonline.us"
#define CHINA_REGION "https://login.chinacloudapi.cn"

class AzurePartitionTest : public ::testing::Test {
protected:
    void expectValidPartition(const rs_string& partition, const rs_string& expectedHost) {
        rs_string result;
        IAMUtils::GetMicrosoftIdpHost(partition, result);
        EXPECT_EQ(result, expectedHost);
    }

    void expectInvalidPartitionError(const rs_string& partition) {
        bool exceptionThrown = false;
        rs_string errorMessage;
        
        try {
            rs_string result;
            IAMUtils::GetMicrosoftIdpHost(partition, result);
            FAIL() << "Expected exception not thrown for partition: " << partition;
        } catch (RsErrorException& e) {
            exceptionThrown = true;
            errorMessage = e.getErrorMessage();
        }
        
        EXPECT_TRUE(exceptionThrown);
        if (exceptionThrown) {
            EXPECT_TRUE(errorMessage.find("Invalid IdP partition") != rs_string::npos);
        }
    }
};

// Empty partition test
TEST_F(AzurePartitionTest, EmptyPartitionDefaultsToCommercial) {
    expectValidPartition("", COMMERCIAL_REGION);
    expectValidPartition("    ", COMMERCIAL_REGION);
    expectValidPartition("  \t  ", COMMERCIAL_REGION);
    expectValidPartition("   ", COMMERCIAL_REGION); // Empty after trim
}

// Explicit commercial partition tests
TEST_F(AzurePartitionTest, ExplicitCommercialPartitionCaseInsensitive) {
    expectValidPartition("commercial", COMMERCIAL_REGION);
    expectValidPartition("COMMERCIAL", COMMERCIAL_REGION);
    expectValidPartition("Commercial   ", COMMERCIAL_REGION);
    expectValidPartition("  commercial", COMMERCIAL_REGION);
    expectValidPartition("  commercial  ", COMMERCIAL_REGION);
}

// US Government partition tests
TEST_F(AzurePartitionTest, UsGovPartitionCaseInsensitive) {
    expectValidPartition("us-gov", GOV_REGION);
    expectValidPartition("US-GOV", GOV_REGION);
    expectValidPartition("Us-gov   ", GOV_REGION);
    expectValidPartition("  us-gov", GOV_REGION);
    expectValidPartition("  us-gov  ", GOV_REGION);
}

// China partition tests
TEST_F(AzurePartitionTest, ChinaPartitionCaseInsensitive) {
    expectValidPartition("cn", CHINA_REGION);
    expectValidPartition("CN", CHINA_REGION);
    expectValidPartition("Cn   ", CHINA_REGION);
    expectValidPartition("  cn  ", CHINA_REGION);
    expectValidPartition("\t cn \t", CHINA_REGION);
}

// Invalid partition test
TEST_F(AzurePartitionTest, InvalidPartitionThrowsError) {
    expectInvalidPartitionError("random_partition");
}
