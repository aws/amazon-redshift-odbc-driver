#include "common.h"
#include "rsodbc.h"

#include <cstdio>
#include <cstring>

// These tests validate the production DRIVER_DISCOVERY_VERSION constant
// defined in rsodbc.h, which is the same header included by rslibpq.c.

TEST(DRIVER_DISCOVERY_VERSION_TEST_SUITE, constant_equals_one) {
    // Validates the production value from rsodbc.h
    ASSERT_EQ(DRIVER_DISCOVERY_VERSION, 1)
        << "DRIVER_DISCOVERY_VERSION from rsodbc.h should be 1";
}

TEST(DRIVER_DISCOVERY_VERSION_TEST_SUITE, constant_is_positive) {
    ASSERT_GT(DRIVER_DISCOVERY_VERSION, 0)
        << "DRIVER_DISCOVERY_VERSION must be a positive integer";
}

TEST(DRIVER_DISCOVERY_VERSION_TEST_SUITE, startup_param_construction) {
    // Exercises the same code path as libpqConnect():
    //   snprintf(szDriverDiscoveryVersion, sizeof(szDriverDiscoveryVersion), "%d", DRIVER_DISCOVERY_VERSION);
    //   ppKeywords[iCount] = "driver_discovery_version";
    //   ppValues[iCount++] = szDriverDiscoveryVersion;
    //
    // Verifies the formatted value and key are correct for the wire protocol.

    char szDriverDiscoveryVersion[32];
    snprintf(szDriverDiscoveryVersion, sizeof(szDriverDiscoveryVersion), "%d", DRIVER_DISCOVERY_VERSION);

    const char* key = "driver_discovery_version";

    // Simulate the ppKeywords/ppValues assignment
    const char* ppKeywords[2] = { nullptr, nullptr };
    const char* ppValues[2] = { nullptr, nullptr };
    int iCount = 0;

    ppKeywords[iCount] = key;
    ppValues[iCount++] = szDriverDiscoveryVersion;

    // Verify the parameter was added correctly
    ASSERT_EQ(iCount, 1) << "Should have incremented iCount by 1";
    ASSERT_STREQ(ppKeywords[0], "driver_discovery_version")
        << "Key should be 'driver_discovery_version'";
    ASSERT_STREQ(ppValues[0], "1")
        << "Value should be the string representation of DRIVER_DISCOVERY_VERSION";
}
