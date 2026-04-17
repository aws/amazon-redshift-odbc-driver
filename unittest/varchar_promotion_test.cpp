/*-------------------------------------------------------------------------
 *
 * Unit tests for applyVarcharPromotion() in rslibpq.c
 *
 * Tests the varchar-to-longvarchar promotion logic without requiring
 * a database connection. Covers type promotion, size override,
 * UseUnicode interaction, disabled/boundary/negative cases.
 *
 * See: https://github.com/aws/amazon-redshift-odbc-driver/issues/23
 *
 *-------------------------------------------------------------------------
 */

#include "common.h"
#include <rsodbc.h>
#include <rsdesc.h>
#include <rsutil.h>
#include <sql.h>

/*
 * Helper to create a minimal RS_CONNECT_PROPS_INFO with
 * MaxVarcharSize and MaxLongVarcharSize set.
 */
static RS_CONNECT_PROPS_INFO makeConnProps(
    int maxVarcharSize,
    int maxLongVarcharSize)
{
    RS_CONNECT_PROPS_INFO props;
    props.iMaxVarcharSize = maxVarcharSize;
    props.iMaxLongVarcharSize = maxLongVarcharSize;
    return props;
}

/*
 * Helper to create a minimal RS_DESC_REC with type and size.
 */
static RS_DESC_REC makeDescRec(
    short hType,
    int iSize)
{
    RS_DESC_REC rec = {};
    rec.hType = hType;
    rec.iSize = iSize;
    strncpy(rec.szName, "test_col", sizeof(rec.szName));
    return rec;
}

// --- Type promotion tests ---

TEST(VarcharPromotion, VarcharPromotedToLongvarchar)
{
    auto props = makeConnProps(255, RS_MAX_VARCHAR_COLUMN_SIZE);
    auto rec = makeDescRec(SQL_VARCHAR, 65535);
    applyVarcharPromotion(&rec, &props);
    EXPECT_EQ(rec.hType, SQL_LONGVARCHAR);
}

TEST(VarcharPromotion, CharPromotedToLongvarchar)
{
    auto props = makeConnProps(255, RS_MAX_VARCHAR_COLUMN_SIZE);
    auto rec = makeDescRec(SQL_CHAR, 4096);
    applyVarcharPromotion(&rec, &props);
    EXPECT_EQ(rec.hType, SQL_LONGVARCHAR);
}

TEST(VarcharPromotion, WvarcharPromotedToWlongvarchar)
{
    auto props = makeConnProps(255, RS_MAX_VARCHAR_COLUMN_SIZE);
    auto rec = makeDescRec(SQL_WVARCHAR, 65535);
    applyVarcharPromotion(&rec, &props);
    EXPECT_EQ(rec.hType, SQL_WLONGVARCHAR);
}

TEST(VarcharPromotion, WcharPromotedToWlongvarchar)
{
    auto props = makeConnProps(255, RS_MAX_VARCHAR_COLUMN_SIZE);
    auto rec = makeDescRec(SQL_WCHAR, 4096);
    applyVarcharPromotion(&rec, &props);
    EXPECT_EQ(rec.hType, SQL_WLONGVARCHAR);
}

// --- No promotion cases ---

TEST(VarcharPromotion, SmallVarcharNotPromoted)
{
    auto props = makeConnProps(255, RS_MAX_VARCHAR_COLUMN_SIZE);
    auto rec = makeDescRec(SQL_VARCHAR, 200);
    applyVarcharPromotion(&rec, &props);
    EXPECT_EQ(rec.hType, SQL_VARCHAR);
    EXPECT_EQ(rec.iSize, 200);
}

TEST(VarcharPromotion, AtThresholdNotPromoted)
{
    auto props = makeConnProps(255, RS_MAX_VARCHAR_COLUMN_SIZE);
    auto rec = makeDescRec(SQL_VARCHAR, 255);
    applyVarcharPromotion(&rec, &props);
    EXPECT_EQ(rec.hType, SQL_VARCHAR);
    EXPECT_EQ(rec.iSize, 255);
}

TEST(VarcharPromotion, DisabledWithZero)
{
    auto props = makeConnProps(0, RS_MAX_VARCHAR_COLUMN_SIZE);
    auto rec = makeDescRec(SQL_VARCHAR, 65535);
    applyVarcharPromotion(&rec, &props);
    EXPECT_EQ(rec.hType, SQL_VARCHAR);
    EXPECT_EQ(rec.iSize, 65535);
}

TEST(VarcharPromotion, NonCharTypeUnaffected)
{
    auto props = makeConnProps(255, RS_MAX_VARCHAR_COLUMN_SIZE);
    auto rec = makeDescRec(SQL_INTEGER, 10);
    applyVarcharPromotion(&rec, &props);
    EXPECT_EQ(rec.hType, SQL_INTEGER);
    EXPECT_EQ(rec.iSize, 10);
}

// --- Size override tests ---

TEST(VarcharPromotion, SizeSetToMaxLongVarcharSize)
{
    auto props = makeConnProps(255, RS_MAX_VARCHAR_COLUMN_SIZE);
    auto rec = makeDescRec(SQL_VARCHAR, 65535);
    applyVarcharPromotion(&rec, &props);
    EXPECT_EQ(rec.iSize, RS_MAX_VARCHAR_COLUMN_SIZE);
}

TEST(VarcharPromotion, SmallPromotedGetsOverriddenSize)
{
    auto props = makeConnProps(255, RS_MAX_VARCHAR_COLUMN_SIZE);
    auto rec = makeDescRec(SQL_VARCHAR, 256);
    applyVarcharPromotion(&rec, &props);
    EXPECT_EQ(rec.hType, SQL_LONGVARCHAR);
    EXPECT_EQ(rec.iSize, RS_MAX_VARCHAR_COLUMN_SIZE);
}

TEST(VarcharPromotion, SizeOverrideDisabledWithZero)
{
    auto props = makeConnProps(255, 0);
    auto rec = makeDescRec(SQL_VARCHAR, 8001);
    applyVarcharPromotion(&rec, &props);
    EXPECT_EQ(rec.hType, SQL_LONGVARCHAR);
    EXPECT_EQ(rec.iSize, 8001);
}

TEST(VarcharPromotion, CustomMaxLongVarcharSize)
{
    auto props = makeConnProps(255, 32769);
    auto rec = makeDescRec(SQL_VARCHAR, 65535);
    applyVarcharPromotion(&rec, &props);
    EXPECT_EQ(rec.hType, SQL_LONGVARCHAR);
    EXPECT_EQ(rec.iSize, 32769);
}

// --- Custom threshold tests ---

TEST(VarcharPromotion, CustomThreshold8000)
{
    auto props = makeConnProps(8000, RS_MAX_VARCHAR_COLUMN_SIZE);
    auto rec = makeDescRec(SQL_VARCHAR, 8001);
    applyVarcharPromotion(&rec, &props);
    EXPECT_EQ(rec.hType, SQL_LONGVARCHAR);
    EXPECT_EQ(rec.iSize, RS_MAX_VARCHAR_COLUMN_SIZE);
}

TEST(VarcharPromotion, BelowCustomThresholdNotPromoted)
{
    auto props = makeConnProps(8000, RS_MAX_VARCHAR_COLUMN_SIZE);
    auto rec = makeDescRec(SQL_VARCHAR, 7999);
    applyVarcharPromotion(&rec, &props);
    EXPECT_EQ(rec.hType, SQL_VARCHAR);
    EXPECT_EQ(rec.iSize, 7999);
}

// --- Wide type + size override ---

TEST(VarcharPromotion, WvarcharSizeOverride)
{
    auto props = makeConnProps(255, RS_MAX_VARCHAR_COLUMN_SIZE);
    auto rec = makeDescRec(SQL_WVARCHAR, 65535);
    applyVarcharPromotion(&rec, &props);
    EXPECT_EQ(rec.hType, SQL_WLONGVARCHAR);
    EXPECT_EQ(rec.iSize, RS_MAX_VARCHAR_COLUMN_SIZE);
}

// --- Negative MaxVarcharSize (should not promote) ---

TEST(VarcharPromotion, NegativeMaxVarcharSizeNoPromotion)
{
    auto props = makeConnProps(-1, RS_MAX_VARCHAR_COLUMN_SIZE);
    auto rec = makeDescRec(SQL_VARCHAR, 65535);
    applyVarcharPromotion(&rec, &props);
    EXPECT_EQ(rec.hType, SQL_VARCHAR);
    EXPECT_EQ(rec.iSize, 65535);
}


// --- hConciseType tests ---

TEST(VarcharPromotion, ConciseTypeUpdatedOnPromotion)
{
    auto props = makeConnProps(255, RS_MAX_VARCHAR_COLUMN_SIZE);
    auto rec = makeDescRec(SQL_VARCHAR, 65535);
    applyVarcharPromotion(&rec, &props);
    EXPECT_EQ(rec.hType, SQL_LONGVARCHAR);
    EXPECT_EQ(rec.hConciseType, SQL_LONGVARCHAR);
}

TEST(VarcharPromotion, ConciseTypeUpdatedWide)
{
    auto props = makeConnProps(255, RS_MAX_VARCHAR_COLUMN_SIZE);
    auto rec = makeDescRec(SQL_WVARCHAR, 65535);
    applyVarcharPromotion(&rec, &props);
    EXPECT_EQ(rec.hType, SQL_WLONGVARCHAR);
    EXPECT_EQ(rec.hConciseType, SQL_WLONGVARCHAR);
}

TEST(VarcharPromotion, ConciseTypeUnchangedWhenNotPromoted)
{
    auto props = makeConnProps(255, RS_MAX_VARCHAR_COLUMN_SIZE);
    auto rec = makeDescRec(SQL_VARCHAR, 200);
    rec.hConciseType = 0;
    applyVarcharPromotion(&rec, &props);
    EXPECT_EQ(rec.hType, SQL_VARCHAR);
    EXPECT_EQ(rec.hConciseType, 0);
}


// --- Utility function tests for LONGVARCHAR/WLONGVARCHAR ---

TEST(VarcharPromotion, GetParamSizeLongvarchar)
{
    EXPECT_EQ(getParamSize(SQL_LONGVARCHAR),
              RS_MAX_VARCHAR_COLUMN_SIZE);
}

TEST(VarcharPromotion, GetParamSizeWlongvarchar)
{
    EXPECT_EQ(getParamSize(SQL_WLONGVARCHAR),
              RS_MAX_VARCHAR_COLUMN_SIZE);
}

TEST(VarcharPromotion, GetDisplaySizeLongvarchar)
{
    int size = getDisplaySize(
        SQL_LONGVARCHAR, 65535, 0);
    EXPECT_GT(size, 0);
}

TEST(VarcharPromotion, GetDisplaySizeWlongvarchar)
{
    int size = getDisplaySize(
        SQL_WLONGVARCHAR, 65535, 0);
    EXPECT_GT(size, 0);
}

TEST(VarcharPromotion, GetTypeNameLongvarchar)
{
    char buf[128] = {};
    getTypeName(SQL_LONGVARCHAR, buf,
        sizeof(buf), 0);
    EXPECT_GT(strlen(buf), 0u);
}

TEST(VarcharPromotion, GetTypeNameWlongvarchar)
{
    char buf[128] = {};
    getTypeName(SQL_WLONGVARCHAR, buf,
        sizeof(buf), 0);
    EXPECT_GT(strlen(buf), 0u);
}

TEST(VarcharPromotion, DefaultCTypeLongvarchar)
{
    int err = 0;
    short cType = getDefaultCTypeFromSQLType(
        SQL_LONGVARCHAR, &err);
    EXPECT_EQ(cType, SQL_C_CHAR);
    EXPECT_EQ(err, 0);
}

TEST(VarcharPromotion, DefaultCTypeWlongvarchar)
{
    int err = 0;
    short cType = getDefaultCTypeFromSQLType(
        SQL_WLONGVARCHAR, &err);
    EXPECT_EQ(cType, SQL_C_WCHAR);
    EXPECT_EQ(err, 0);
}


// --- SUPER vs promoted varchar type name tests ---

TEST(VarcharPromotion, TypeNameSuperLongvarchar)
{
    char buf[128] = {};
    getTypeName(SQL_LONGVARCHAR, buf,
        sizeof(buf), SUPER);
    EXPECT_STREQ(buf, "SUPER");
}

TEST(VarcharPromotion, TypeNameSuperWlongvarchar)
{
    char buf[128] = {};
    getTypeName(SQL_WLONGVARCHAR, buf,
        sizeof(buf), SUPER);
    EXPECT_STREQ(buf, "SUPER");
}

TEST(VarcharPromotion, TypeNamePromotedLongvarchar)
{
    char buf[128] = {};
    getTypeName(SQL_LONGVARCHAR, buf,
        sizeof(buf), 0);
    EXPECT_STREQ(buf, "CHARACTER VARYING");
}

TEST(VarcharPromotion, TypeNamePromotedWlongvarchar)
{
    char buf[128] = {};
    getTypeName(SQL_WLONGVARCHAR, buf,
        sizeof(buf), 0);
    EXPECT_STREQ(buf, "CHARACTER VARYING");
}


// --- SUPER pass-through: already LONGVARCHAR, not reprocessed ---

TEST(VarcharPromotion, AlreadyLongvarcharNotReprocessed)
{
    auto props = makeConnProps(255, RS_MAX_VARCHAR_COLUMN_SIZE);
    auto rec = makeDescRec(SQL_LONGVARCHAR, 4194304); // SUPER size
    applyVarcharPromotion(&rec, &props);
    EXPECT_EQ(rec.hType, SQL_LONGVARCHAR);
    EXPECT_EQ(rec.iSize, 4194304); // size must NOT be overridden
}

TEST(VarcharPromotion, AlreadyWlongvarcharNotReprocessed)
{
    auto props = makeConnProps(255, RS_MAX_VARCHAR_COLUMN_SIZE);
    auto rec = makeDescRec(SQL_WLONGVARCHAR, 4194304); // SUPER size
    applyVarcharPromotion(&rec, &props);
    EXPECT_EQ(rec.hType, SQL_WLONGVARCHAR);
    EXPECT_EQ(rec.iSize, 4194304); // size must NOT be overridden
}
