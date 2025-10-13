#include "common.h"
#include "rsoptions.h"

// Test isStrConnectAttr function
class IsStrConnectAttrTest : public ::testing::Test {
};

TEST_F(IsStrConnectAttrTest, ReturnsTrue_ForStringAttributes) {
    // Test string-type connection attributes
    EXPECT_TRUE(RsOptions::isStrConnectAttr(SQL_ATTR_CURRENT_CATALOG));
    EXPECT_TRUE(RsOptions::isStrConnectAttr(SQL_ATTR_TRACEFILE));
    EXPECT_TRUE(RsOptions::isStrConnectAttr(SQL_ATTR_TRANSLATE_LIB));
}

TEST_F(IsStrConnectAttrTest, ReturnsFalse_ForIntegerAttributes) {
    // Test integer-type connection attributes
    EXPECT_FALSE(RsOptions::isStrConnectAttr(SQL_ATTR_ACCESS_MODE));
    EXPECT_FALSE(RsOptions::isStrConnectAttr(SQL_ATTR_AUTOCOMMIT));
    EXPECT_FALSE(RsOptions::isStrConnectAttr(SQL_ATTR_CONNECTION_TIMEOUT));
    EXPECT_FALSE(RsOptions::isStrConnectAttr(SQL_ATTR_LOGIN_TIMEOUT));
    EXPECT_FALSE(RsOptions::isStrConnectAttr(SQL_ATTR_PACKET_SIZE));
    EXPECT_FALSE(RsOptions::isStrConnectAttr(SQL_ATTR_TXN_ISOLATION));
    EXPECT_FALSE(RsOptions::isStrConnectAttr(SQL_ATTR_QUIET_MODE));
    EXPECT_FALSE(RsOptions::isStrConnectAttr(SQL_ATTR_TRANSLATE_OPTION));
}

TEST_F(IsStrConnectAttrTest, ReturnsFalse_ForInvalidAttributes) {
    // Test invalid/unknown attributes
    EXPECT_FALSE(RsOptions::isStrConnectAttr(-1));
    EXPECT_FALSE(RsOptions::isStrConnectAttr(0));
    EXPECT_FALSE(RsOptions::isStrConnectAttr(99999));
}
