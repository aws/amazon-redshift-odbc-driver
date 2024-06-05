#include "common.h"
#include <rsdesc.h>
#include <rsodbc.h>
#include <sql.h>

TEST(BIND_OFFSET_TEST_SUITE, test_value_loss) {

    long offset = 4294967295;
    SQLPOINTER offsetPtr = &offset;
    RS_ENV_INFO _phenv;
    RS_ENV_INFO *_phenv_ = &_phenv;
    RS_CONN_INFO _phdbc(_phenv_);
    RS_CONN_INFO *_phdbc_ = &_phdbc;
    RS_DESC_INFO phdesc_(_phdbc_, 0, 0);
    phdesc_.pDescHeader = RS_DESC_HEADER(0);
    phdesc_.pDescHeader.valid = 1;
    SQLHDESC phdesc = &phdesc_;

    RsDesc::RS_SQLSetDescField(phdesc, 0, SQL_DESC_BIND_OFFSET_PTR, offsetPtr,
                               0, TRUE);

    int iBindOffset = 0; // just to depict the buggy behavior
    SQLLEN iBindOffset2 = 0;
    EXPECT_EQ(iBindOffset, iBindOffset2);                  // baseline
    iBindOffset = *(phdesc_.pDescHeader.plBindOffsetPtr);  // integer offset
    iBindOffset2 = *(phdesc_.pDescHeader.plBindOffsetPtr); // 64bit offset
    EXPECT_NE(iBindOffset, iBindOffset2);
    EXPECT_EQ(iBindOffset2, offset); // value is preserved
    EXPECT_LT(iBindOffset, 0);       // value is lost/rotated (-1)
}
