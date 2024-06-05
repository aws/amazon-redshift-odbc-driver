#include "common.h"
#include <rsdesc.h>
#include <rsodbc.h>
#include <sql.h>

// This unit test is for testing the default value when databaseMetadataCurrentDbOnly flag is not included in DSN.
TEST(DEFAULT_VALUE_TEST_SUITE, test_DatabaseMetadataCurrentDbOnly) {

    RS_CONNECT_PROPS_INFO obj;
    EXPECT_TRUE(obj.iDatabaseMetadataCurrentDbOnly == 1);

}
