#include "common.h"
#include <rsdesc.h>
#include <rsodbc.h>
#include <sql.h>
#include <rs_pq_type.h>

/*
 * Parameterized OID mapping tests covering useUnicode and boolAsChar.
 *
 * Character-type OIDs return different SQL types depending on useUnicode:
 *   useUnicode=1: SQL_WVARCHAR / SQL_WCHAR (wide)
 *   useUnicode=0: SQL_VARCHAR  / SQL_CHAR  (narrow)
 *
 * BOOLOID returns different SQL types depending on boolAsChar:
 *   boolAsChar=0: SQL_BIT
 *   boolAsChar=1: SQL_VARCHAR (or SQL_WVARCHAR if useUnicode=1)
 */

struct OidMappingParam {
    unsigned int oid;
    int          useUnicode;
    int          boolAsChar;
    short        expectedSqlType;
    const char  *name;
};

class OidMappingTest : public ::testing::TestWithParam<OidMappingParam> {};

TEST_P(OidMappingTest, MapsToExpectedSqlType) {
    const auto &p = GetParam();
    short rsSpecialType = 0;
    short result = mapPgTypeToSqlType(p.oid, &rsSpecialType, p.useUnicode, p.boolAsChar);
    EXPECT_EQ(result, p.expectedSqlType)
        << p.name << " (OID " << p.oid << ", useUnicode=" << p.useUnicode
        << ", boolAsChar=" << p.boolAsChar
        << ") returned " << result << ", expected " << p.expectedSqlType;
    if (p.oid == TIMETZOID) {
        EXPECT_EQ(rsSpecialType, TIMETZOID);
    } else if (p.oid == TIMESTAMPTZOID) {
        EXPECT_EQ(rsSpecialType, TIMESTAMPTZOID);
    } else if (p.oid == SUPER) {
        EXPECT_EQ(rsSpecialType, SUPER);
    } else if (p.oid == BOOLOID && p.boolAsChar) {
        EXPECT_EQ(rsSpecialType, BOOLOID);
    } else {
        EXPECT_EQ(rsSpecialType, 0);
    }
}

INSTANTIATE_TEST_SUITE_P(
    Unicode_CharacterOids,
    OidMappingTest,
    ::testing::Values(
        // Variable-length character OIDs — useUnicode=1 → SQL_WVARCHAR
        OidMappingParam{VARCHAROID,  1, 0, SQL_WVARCHAR, "VARCHAROID"},
        OidMappingParam{TEXTOID,     1, 0, SQL_WVARCHAR, "TEXTOID"},
        OidMappingParam{NAMEOID,     1, 0, SQL_WVARCHAR, "NAMEOID"},
        // Fixed-length character OIDs — useUnicode=1 → SQL_WCHAR
        OidMappingParam{CHAROID,     1, 0, SQL_WCHAR,    "CHAROID"},
        OidMappingParam{BPCHAROID,   1, 0, SQL_WCHAR,    "BPCHAROID"},

        // SUPER data type — useUnicode=1 → SQL_WLONGVARCHAR
        OidMappingParam{SUPER,       1, 0, SQL_WLONGVARCHAR,"SUPER"},

        // Other string OIDs — useUnicode=1 → SQL_WVARCHAR
        OidMappingParam{REFCURSOROID, 1, 0, SQL_WVARCHAR, "REFCURSOROID"},
        OidMappingParam{UNKNOWNOID,   1, 0, SQL_WVARCHAR, "UNKNOWNOID"}
    ),
    [](const ::testing::TestParamInfo<OidMappingParam> &info) {
        return std::string(info.param.name);
    });

INSTANTIATE_TEST_SUITE_P(
    NoUnicode_CharacterOids,
    OidMappingTest,
    ::testing::Values(
        // Variable-length character OIDs — useUnicode=0 → SQL_VARCHAR
        OidMappingParam{VARCHAROID,  0, 0, SQL_VARCHAR, "VARCHAROID"},
        OidMappingParam{TEXTOID,     0, 0, SQL_VARCHAR, "TEXTOID"},
        OidMappingParam{NAMEOID,     0, 0, SQL_VARCHAR, "NAMEOID"},

        // Fixed-length character OIDs — useUnicode=0 → SQL_CHAR
        OidMappingParam{CHAROID,     0, 0, SQL_CHAR,    "CHAROID"},
        OidMappingParam{BPCHAROID,   0, 0, SQL_CHAR,    "BPCHAROID"},

        // SUPER data type - useUnicode=0 → SQL_LONGVARCHAR
        OidMappingParam{SUPER,       0, 0, SQL_LONGVARCHAR,"SUPER"},

        // Other string OIDs — useUnicode=0 → SQL_VARCHAR
        OidMappingParam{REFCURSOROID, 0, 0, SQL_VARCHAR, "REFCURSOROID"},
        OidMappingParam{UNKNOWNOID,   0, 0, SQL_VARCHAR, "UNKNOWNOID"}
    ),
    [](const ::testing::TestParamInfo<OidMappingParam> &info) {
        return std::string(info.param.name);
    });

INSTANTIATE_TEST_SUITE_P(
    NonCharacterOids,
    OidMappingTest,
    ::testing::Values(
        // These return the same type regardless of useUnicode — test with both values
        // useUnicode=1, boolAsChar=0
        OidMappingParam{BOOLOID,        1, 0, SQL_BIT,                    "BOOLOID_u1"},
        OidMappingParam{INT2OID,        1, 0, SQL_SMALLINT,               "INT2OID_u1"},
        OidMappingParam{INT4OID,        1, 0, SQL_INTEGER,                "INT4OID_u1"},
        OidMappingParam{INT8OID,        1, 0, SQL_BIGINT,                 "INT8OID_u1"},
        OidMappingParam{FLOAT4OID,      1, 0, SQL_REAL,                   "FLOAT4OID_u1"},
        OidMappingParam{FLOAT8OID,      1, 0, SQL_DOUBLE,                 "FLOAT8OID_u1"},
        OidMappingParam{NUMERICOID,     1, 0, SQL_NUMERIC,                "NUMERICOID_u1"},
        OidMappingParam{DATEOID,        1, 0, SQL_TYPE_DATE,              "DATEOID_u1"},
        OidMappingParam{TIMEOID,        1, 0, SQL_TYPE_TIME,              "TIMEOID_u1"},
        OidMappingParam{TIMETZOID,      1, 0, SQL_TYPE_TIME,              "TIMETZOID_u1"},
        OidMappingParam{TIMESTAMPOID,   1, 0, SQL_TYPE_TIMESTAMP,         "TIMESTAMPOID_u1"},
        OidMappingParam{TIMESTAMPTZOID, 1, 0, SQL_TYPE_TIMESTAMP,         "TIMESTAMPTZOID_u1"},
        OidMappingParam{INTERVALY2MOID, 1, 0, SQL_INTERVAL_YEAR_TO_MONTH, "INTERVALY2MOID_u1"},
        OidMappingParam{INTERVALD2SOID, 1, 0, SQL_INTERVAL_DAY_TO_SECOND, "INTERVALD2SOID_u1"},

        // useUnicode=0, boolAsChar=0 — same expected results
        OidMappingParam{BOOLOID,        0, 0, SQL_BIT,                    "BOOLOID_u0"},
        OidMappingParam{INT2OID,        0, 0, SQL_SMALLINT,               "INT2OID_u0"},
        OidMappingParam{INT4OID,        0, 0, SQL_INTEGER,                "INT4OID_u0"},
        OidMappingParam{INT8OID,        0, 0, SQL_BIGINT,                 "INT8OID_u0"},
        OidMappingParam{FLOAT4OID,      0, 0, SQL_REAL,                   "FLOAT4OID_u0"},
        OidMappingParam{FLOAT8OID,      0, 0, SQL_DOUBLE,                 "FLOAT8OID_u0"},
        OidMappingParam{NUMERICOID,     0, 0, SQL_NUMERIC,                "NUMERICOID_u0"},
        OidMappingParam{DATEOID,        0, 0, SQL_TYPE_DATE,              "DATEOID_u0"},
        OidMappingParam{TIMEOID,        0, 0, SQL_TYPE_TIME,              "TIMEOID_u0"},
        OidMappingParam{TIMETZOID,      0, 0, SQL_TYPE_TIME,              "TIMETZOID_u0"},
        OidMappingParam{TIMESTAMPOID,   0, 0, SQL_TYPE_TIMESTAMP,         "TIMESTAMPOID_u0"},
        OidMappingParam{TIMESTAMPTZOID, 0, 0, SQL_TYPE_TIMESTAMP,         "TIMESTAMPTZOID_u0"},
        OidMappingParam{INTERVALY2MOID, 0, 0, SQL_INTERVAL_YEAR_TO_MONTH, "INTERVALY2MOID_u0"},
        OidMappingParam{INTERVALD2SOID, 0, 0, SQL_INTERVAL_DAY_TO_SECOND, "INTERVALD2SOID_u0"}
    ),
    [](const ::testing::TestParamInfo<OidMappingParam> &info) {
        return std::string(info.param.name);
    });

INSTANTIATE_TEST_SUITE_P(
    BoolsAsChar,
    OidMappingTest,
    ::testing::Values(
        // boolAsChar=1, useUnicode=0 → BOOLOID maps to SQL_VARCHAR
        OidMappingParam{BOOLOID, 0, 1, SQL_VARCHAR,  "BOOLOID_BoolsAsChar_noUnicode"},
        // boolAsChar=1, useUnicode=1 → BOOLOID maps to SQL_WVARCHAR
        OidMappingParam{BOOLOID, 1, 1, SQL_WVARCHAR, "BOOLOID_BoolsAsChar_Unicode"}
    ),
    [](const ::testing::TestParamInfo<OidMappingParam> &info) {
        return std::string(info.param.name);
    });
