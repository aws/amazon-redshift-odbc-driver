#include "common.h"
#include <rsdesc.h>
#include <rsodbc.h>
#include <sql.h>

TEST(BIND_OFFSET_TEST_SUITE, test_value_loss) {
/*
The following is a snippet from sqltypes.h
It shows that in a 64-bit system, SQLLEN is 8 bytes(64 bits) in both Linux as
will as windows.

typedef long            SQLINTEGER;
typedef unsigned long   SQLUINTEGER;

#ifdef _WIN64
typedef INT64           SQLLEN;
typedef UINT64          SQLULEN;
typedef UINT64          SQLSETPOSIROW;
#else
#define SQLLEN          SQLINTEGER
#define SQLULEN         SQLUINTEGER
#define SQLSETPOSIROW   SQLUSMALLINT
#endif
*/

// Lets create an 8 byte offset with max value for a 32bit offset: 4294967295
// (2^32 - 1)
#ifdef WIN32
    long long offset = 4294967295; // INT64 variable
#else
    long offset = 4294967295; // 64 bits variable
#endif
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
    phdesc_.pDescHeader.plBindOffsetPtr = (SQLLEN *)offsetPtr;

    EXPECT_EQ(iBindOffset, iBindOffset2); // baseline
    iBindOffset =
        *(phdesc_.pDescHeader.plBindOffsetPtr); // integer offset rotates!!
    iBindOffset2 = *(phdesc_.pDescHeader.plBindOffsetPtr); // 64bit offset
    EXPECT_NE(iBindOffset, iBindOffset2);
    EXPECT_EQ(iBindOffset2, offset); // value is preserved
    EXPECT_LT(iBindOffset, 0);       // value is lost/rotated (-1)
}
