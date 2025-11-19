#include <cstdlib>
#include <map>
#include <vector>

#include <gtest/gtest.h>

#include "common.h"
#include "rsdesc.h"
#include "rsodbc.h"
#include <sql.h>
#include "rsMetadataAPIHelper.h"

#define expectedSQLTablesColNum 10
char* expectedSQLTablesCol[expectedSQLTablesColNum] = {"TABLE_CAT", "TABLE_SCHEM", "TABLE_NAME", "TABLE_TYPE", "REMARKS", "OWNER", 
    "LAST_ALTERED_TIME", "LAST_MODIFIED_TIME", "DIST_STYLE", "TABLE_SUBTYPE"};
int expectedSQLTablesColDataType[expectedSQLTablesColNum] = {VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID,
    VARCHAROID, TIMESTAMPOID, TIMESTAMPOID, VARCHAROID, VARCHAROID};

#define expectedSQLColumnsColNum 23
char* expectedSQLColumnsCol[expectedSQLColumnsColNum] = {"TABLE_CAT", "TABLE_SCHEM", "TABLE_NAME", "COLUMN_NAME", "DATA_TYPE",
                            "TYPE_NAME", "COLUMN_SIZE", "BUFFER_LENGTH", "DECIMAL_DIGITS", "NUM_PREC_RADIX",
                            "NULLABLE", "REMARKS", "COLUMN_DEF", "SQL_DATA_TYPE", "SQL_DATETIME_SUB",
                            "CHAR_OCTET_LENGTH", "ORDINAL_POSITION", "IS_NULLABLE", "SORT_KEY_TYPE", "SORT_KEY",
                            "DIST_KEY", "ENCODING", "COLLATION"};
int expectedSQLColumnsDatatype[expectedSQLColumnsColNum] = {VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID, INT2OID,
                                                    VARCHAROID,INT4OID,INT4OID,INT2OID,INT2OID,
                                                    INT2OID,VARCHAROID,VARCHAROID,INT2OID,INT2OID,
                                                    INT4OID,INT4OID,VARCHAROID,
                                                    VARCHAROID,VARCHAROID,VARCHAROID,VARCHAROID,VARCHAROID};

#define expectedSQLPrimaryKeysColNum 6
char *expectedSQLPrimaryKeysCol[expectedSQLPrimaryKeysColNum] = {
    "TABLE_CAT",   "TABLE_SCHEM", "TABLE_NAME",
    "COLUMN_NAME", "KEY_SEQ",     "PK_NAME"};
int expectedSQLPrimaryKeysColDataType[expectedSQLPrimaryKeysColNum] = {
    VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID, INT2OID, VARCHAROID};

#define expectedSQLForeignKeysColNum 14
char *expectedSQLForeignKeysCol[expectedSQLForeignKeysColNum] = {
    "PKTABLE_CAT", "PKTABLE_SCHEM", "PKTABLE_NAME", "PKCOLUMN_NAME",
    "FKTABLE_CAT", "FKTABLE_SCHEM", "FKTABLE_NAME", "FKCOLUMN_NAME",
    "KEY_SEQ",     "UPDATE_RULE",   "DELETE_RULE",  "FK_NAME",
    "PK_NAME",     "DEFERRABILITY"};
int expectedSQLForeignKeysColDataType[expectedSQLForeignKeysColNum] = {
    VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID,
    VARCHAROID, VARCHAROID, VARCHAROID, INT2OID,    INT2OID,
    INT2OID,    VARCHAROID, VARCHAROID, INT2OID};

#define expectedSQLSpecialColumnsColNum 8
char *expectedSQLSpecialColumnsCol[expectedSQLSpecialColumnsColNum] = {
    "SCOPE",       "COLUMN_NAME",   "DATA_TYPE",      "TYPE_NAME",
    "COLUMN_SIZE", "BUFFER_LENGTH", "DECIMAL_DIGITS", "PSEUDO_COLUMN"};
int expectedSQLSpecialColumnsColDataType[expectedSQLSpecialColumnsColNum] = {
    INT2OID, VARCHAROID, INT2OID, VARCHAROID,
    INT4OID, INT4OID,    INT2OID, INT2OID};

#define expectedSQLColumnPrivilegesColNum 8
char *expectedSQLColumnPrivilegesCol[expectedSQLColumnPrivilegesColNum] = {
    "TABLE_CAT", "TABLE_SCHEM", "TABLE_NAME", "COLUMN_NAME",
    "GRANTOR",   "GRANTEE",     "PRIVILEGE",  "IS_GRANTABLE"};
int expectedSQLColumnPrivilegesColDataType[expectedSQLColumnPrivilegesColNum] =
    {VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID,
     VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID};

#define expectedSQLTablePrivilegesColNum 7
char *expectedSQLTablePrivilegesCol[expectedSQLTablePrivilegesColNum] = {
    "TABLE_CAT", "TABLE_SCHEM", "TABLE_NAME",  "GRANTOR",
    "GRANTEE",   "PRIVILEGE",   "IS_GRANTABLE"};
int expectedSQLTablePrivilegesColDataType[expectedSQLTablePrivilegesColNum] = {
    VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID,
    VARCHAROID, VARCHAROID, VARCHAROID};

#define expectedSQLProceduresColNum 8
char *expectedSQLProceduresCol[expectedSQLProceduresColNum] = {
    "PROCEDURE_CAT",    "PROCEDURE_SCHEM",   "PROCEDURE_NAME",
    "NUM_INPUT_PARAMS", "NUM_OUTPUT_PARAMS", "NUM_RESULT_SETS",
    "REMARKS",          "PROCEDURE_TYPE"};
int expectedSQLProceduresColDataType[expectedSQLProceduresColNum] = {
    VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID,
    VARCHAROID, VARCHAROID, VARCHAROID, INT2OID};

#define expectedSQLProcedureColumnsColNum 19
char *expectedSQLProcedureColumnsCol[expectedSQLProcedureColumnsColNum] = {
    "PROCEDURE_CAT",    "PROCEDURE_SCHEM",   "PROCEDURE_NAME",
    "COLUMN_NAME",      "COLUMN_TYPE",       "DATA_TYPE",
    "TYPE_NAME",        "COLUMN_SIZE",       "BUFFER_LENGTH",
    "DECIMAL_DIGITS",   "NUM_PREC_RADIX",    "NULLABLE",
    "REMARKS",          "COLUMN_DEF",        "SQL_DATA_TYPE",
    "SQL_DATETIME_SUB", "CHAR_OCTET_LENGTH", "ORDINAL_POSITION",
    "IS_NULLABLE"};
int expectedSQLProcedureColumnsColDataType[expectedSQLProcedureColumnsColNum] =
    {VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID, INT2OID,
     INT4OID,    VARCHAROID, INT4OID,    INT4OID,    INT2OID,
     INT2OID,    INT2OID,    VARCHAROID, VARCHAROID, INT4OID,
     INT4OID,    INT4OID,    INT4OID,    VARCHAROID};

#define character_maximum_length 512
#define numeric_precision 5
#define numeric_scale 5


#define SQLTYPE 0
#define SQLDATATYPE 1
#define SQLDATESUB 2
#define COLUMNSIZE 3
#define DECIMALDIGIT 4

struct DATA_TYPE_RES{
  short sqlType;
  short sqlTypeODBC2;
  short sqlDataType;
  short sqlDateSub;
  short colSize;
  short decimalDigit;
  int bufferLen;
};


// Unit test for column number constant
TEST(MetadataApiTest, SqlTablesColumnCount) {
    ASSERT_EQ(kTablesColNum, expectedSQLTablesColNum);
}
TEST(MetadataApiTest, SqlColumnsColumnCount) {
    ASSERT_EQ(kColumnsColNum, expectedSQLColumnsColNum);
}
TEST(MetadataApiTest, SqPrimaryKeysColumnCount) {
    ASSERT_EQ(kPrimaryKeysColNum, expectedSQLPrimaryKeysColNum);
}
TEST(MetadataApiTest, SqlForeignKeysColumnCount) {
    ASSERT_EQ(kForeignKeysColNum, expectedSQLForeignKeysColNum);
}
TEST(MetadataApiTest, SqlSpecialColumnsColumnCount) {
    ASSERT_EQ(kSpecialColumnsColNum, expectedSQLSpecialColumnsColNum);
}
TEST(MetadataApiTest, SqlColumnPrivilegesColumnCount) {
    ASSERT_EQ(kColumnPrivilegesColNum, expectedSQLColumnPrivilegesColNum);
}
TEST(MetadataApiTest, SqlTablePrivilegesColumnCount) {
    ASSERT_EQ(kTablePrivilegesColNum, expectedSQLTablePrivilegesColNum);
}
TEST(MetadataApiTest, SqlProceduresColumnCount) {
    ASSERT_EQ(kProceduresColNum, expectedSQLProceduresColNum);
}
TEST(MetadataApiTest, SqlProcedureColumnsColumnCount) {
    ASSERT_EQ(kProcedureColumnsColNum, expectedSQLProcedureColumnsColNum);
}

class MetadataApiTestBase : public ::testing::TestWithParam<std::tuple<short, bool>> {
protected:
    void SetUp() override {
        std::tie(column_index, odbcVersion) = GetParam();

        // Set up the statement with appropriate ODBC version
        pStmt = new RS_STMT_INFO(new RS_CONN_INFO(new RS_ENV_INFO()));
        pStmt->phdbc->phenv->pEnvAttr->iOdbcVersion = odbcVersion;

        // Initialize column names based on ODBC version
        RsMetadataAPIHelper::initializeColumnNames(pStmt);
    }

    void TearDown() override {
        if (pStmt) {
            if (pStmt->phdbc) {
                if (pStmt->phdbc->phenv) {
                    delete pStmt->phdbc->phenv;
                }
                delete pStmt->phdbc;
            }
            delete pStmt;
        }
    }

    // Helper method to verify column name and data type
    void verifyColumnAttributes(const char* apiName,
                              const char* expectedColName,
                              const char* (*getColNameFunc)(int),
                              const int* colDataTypes,
                              const int* expectedDataTypes) {
        // Verify column name
        EXPECT_STREQ(getColNameFunc(column_index), expectedColName)
            << "metadata API " << apiName << " Column index: " << column_index
            << ", has wrong column name: " << getColNameFunc(column_index)
            << " (Expect: " << expectedColName << ") for ODBC"
            << odbcVersion;

        // Verify data type
        short RsSpecialType;
        short sqlType = mapPgTypeToSqlType(colDataTypes[column_index], &RsSpecialType);
        short expectedSqlType = mapPgTypeToSqlType(expectedDataTypes[column_index], &RsSpecialType);

        EXPECT_EQ(colDataTypes[column_index], expectedDataTypes[column_index])
            << "metadata API " << apiName << " Column index: " << column_index
            << ", has wrong column data type: " << sqlTypeNameMap(sqlType)
            << " (Expect: " << sqlTypeNameMap(expectedSqlType) << ")";
    }

    short column_index;
    int odbcVersion;
    RS_STMT_INFO* pStmt;
};

// SQLTables Column name constant unit test
class SqlTablesTest : public MetadataApiTestBase {
protected:
    const char* getExpectedColumnName() {
        if (odbcVersion == SQL_OV_ODBC2) {
            switch (column_index) {
                case kSQLTables_TABLE_CATALOG_COL_NUM:
                    return RsMetadataAPIHelper::getOdbc2ColumnName(0);
                case kSQLTables_TABLE_SCHEM_COL_NUM:
                    return RsMetadataAPIHelper::getOdbc2ColumnName(1);
                default:
                    return expectedSQLTablesCol[column_index];
            }
        }
        return expectedSQLTablesCol[column_index];
    }
};

TEST_P(SqlTablesTest, VerifyColumnAttributes) {
    verifyColumnAttributes(
        "SQLTables",
        getExpectedColumnName(),
        RsMetadataAPIHelper::getSqlTablesColName,
        RsMetadataAPIHelper::kTablesColDatatype,
        expectedSQLTablesColDataType
    );
}

INSTANTIATE_TEST_SUITE_P(METADATA_API_HELPER_TEST_SUITE, 
    SqlTablesTest,
    ::testing::Combine(
        ::testing::Values(
            kSQLTables_TABLE_CATALOG_COL_NUM,
            kSQLTables_TABLE_SCHEM_COL_NUM,
            kSQLTables_TABLE_NAME_COL_NUM,
            kSQLTables_TABLE_TYPE_COL_NUM,
            kSQLTables_REMARKS_COL_NUM
        ),
        ::testing::Values(SQL_OV_ODBC2, SQL_OV_ODBC3, 999)
    )
);


// SQLColumns Column name constant unit test
class SqlColumnsTest : public MetadataApiTestBase {
protected:
    const char* getExpectedColumnName() {
        if (odbcVersion == SQL_OV_ODBC2) {
            static const std::map<short, int> odbc2ColumnMap = {
                {kSQLColumns_TABLE_CAT_COL_NUM, 0},
                {kSQLColumns_TABLE_SCHEM_COL_NUM, 1},
                {kSQLColumns_COLUMN_SIZE_COL_NUM, 2},
                {kSQLColumns_BUFFER_LENGTH_COL_NUM, 3},
                {kSQLColumns_DECIMAL_DIGITS_COL_NUM, 4},
                {kSQLColumns_NUM_PREC_RADIX_COL_NUM, 5}
            };

            auto it = odbc2ColumnMap.find(column_index);
            if (it != odbc2ColumnMap.end()) {
                return RsMetadataAPIHelper::getOdbc2ColumnName(it->second);
            }
        }
        return expectedSQLColumnsCol[column_index];
    }
};

TEST_P(SqlColumnsTest, VerifyColumnAttributes) {
    verifyColumnAttributes(
        "SQLColumns",
        getExpectedColumnName(),
        RsMetadataAPIHelper::getSqlColumnsColName,
        RsMetadataAPIHelper::kColumnsColDatatype,
        expectedSQLColumnsDatatype
    );
}

INSTANTIATE_TEST_SUITE_P(METADATA_API_HELPER_TEST_SUITE, 
    SqlColumnsTest,
    ::testing::Combine(
        ::testing::Values(
            kSQLColumns_TABLE_CAT_COL_NUM,
            kSQLColumns_TABLE_SCHEM_COL_NUM,
            kSQLColumns_TABLE_NAME_COL_NUM,
            kSQLColumns_COLUMN_NAME_COL_NUM,
            kSQLColumns_DATA_TYPE_COL_NUM,
            kSQLColumns_TYPE_NAME_COL_NUM,
            kSQLColumns_COLUMN_SIZE_COL_NUM,
            kSQLColumns_BUFFER_LENGTH_COL_NUM,
            kSQLColumns_DECIMAL_DIGITS_COL_NUM,
            kSQLColumns_NUM_PREC_RADIX_COL_NUM,
            kSQLColumns_NULLABLE_COL_NUM,
            kSQLColumns_REMARKS_COL_NUM,
            kSQLColumns_COLUMN_DEF_COL_NUM,
            kSQLColumns_SQL_DATA_TYPE_COL_NUM,
            kSQLColumns_SQL_DATETIME_SUB_COL_NUM,
            kSQLColumns_CHAR_OCTET_LENGTH_COL_NUM,
            kSQLColumns_ORDINAL_POSITION_COL_NUM,
            kSQLColumns_IS_NULLABLE_COL_NUM
        ),
        ::testing::Values(SQL_OV_ODBC2, SQL_OV_ODBC3, 999)
    )
);

// SQLPrimaryKeys Column name constant unit test
class SqlPrimaryKeysTest : public MetadataApiTestBase {
protected:
    const char* getExpectedColumnName() {
        if (odbcVersion == SQL_OV_ODBC2) {
            switch (column_index) {
                case kSQLPrimaryKeys_TABLE_CAT_COL_NUM:
                    return RsMetadataAPIHelper::getOdbc2ColumnName(0);
                case kSQLPrimaryKeys_TABLE_SCHEM_COL_NUM:
                    return RsMetadataAPIHelper::getOdbc2ColumnName(1);
                default:
                    return expectedSQLPrimaryKeysCol[column_index];
            }
        }
        return expectedSQLPrimaryKeysCol[column_index];
    }
};

TEST_P(SqlPrimaryKeysTest, VerifyColumnAttributes) {
    verifyColumnAttributes(
        "SQLPrimaryKeys",
        getExpectedColumnName(),
        RsMetadataAPIHelper::getSqlPrimaryKeysColName,
        RsMetadataAPIHelper::kPrimaryKeysColDatatype,
        expectedSQLPrimaryKeysColDataType
    );
}

INSTANTIATE_TEST_SUITE_P(METADATA_API_HELPER_TEST_SUITE,
    SqlPrimaryKeysTest,
    ::testing::Combine(
        ::testing::Values(
            kSQLPrimaryKeys_TABLE_CAT_COL_NUM,
            kSQLPrimaryKeys_TABLE_SCHEM_COL_NUM,
            kSQLPrimaryKeys_TABLE_NAME_COL_NUM,
            kSQLPrimaryKeys_COLUMN_NAME_COL_NUM,
            kSQLPrimaryKeys_KEY_SEQ_COL_NUM,
            kSQLPrimaryKeys_PK_NAME_COL_NUM
        ),
        ::testing::Values(SQL_OV_ODBC2, SQL_OV_ODBC3, 999)
    )
);

// SQLForeignKeys Column name constant unit test
class SqlForeignKeysTest : public MetadataApiTestBase {
protected:
    const char* getExpectedColumnName() {
        if (odbcVersion == SQL_OV_ODBC2) {
            switch (column_index) {
                case kSQLForeignKeys_PKTABLE_CAT_COL_NUM:
                case kSQLForeignKeys_FKTABLE_CAT_COL_NUM:
                    return RsMetadataAPIHelper::getOdbc2ColumnName(0);
                case kSQLForeignKeys_PKTABLE_SCHEM_COL_NUM:
                case kSQLForeignKeys_FKTABLE_SCHEM_COL_NUM:
                    return RsMetadataAPIHelper::getOdbc2ColumnName(1);
                default:
                    return expectedSQLForeignKeysCol[column_index];
            }
        }
        return expectedSQLForeignKeysCol[column_index];
    }
};

TEST_P(SqlForeignKeysTest, VerifyColumnAttributes) {
    verifyColumnAttributes(
        "SQLForeignKeys",
        getExpectedColumnName(),
        RsMetadataAPIHelper::getSqlForeignKeysColName,
        RsMetadataAPIHelper::kForeignKeysColDatatype,
        expectedSQLForeignKeysColDataType
    );
}

INSTANTIATE_TEST_SUITE_P(METADATA_API_HELPER_TEST_SUITE,
    SqlForeignKeysTest,
    ::testing::Combine(
        ::testing::Values(
            kSQLForeignKeys_PKTABLE_CAT_COL_NUM,
            kSQLForeignKeys_PKTABLE_SCHEM_COL_NUM,
            kSQLForeignKeys_PKTABLE_NAME_COL_NUM,
            kSQLForeignKeys_PKCOLUMN_NAME_COL_NUM,
            kSQLForeignKeys_FKTABLE_CAT_COL_NUM,
            kSQLForeignKeys_FKTABLE_SCHEM_COL_NUM,
            kSQLForeignKeys_FKTABLE_NAME_COL_NUM,
            kSQLForeignKeys_FKCOLUMN_NAME_COL_NUM,
            kSQLForeignKeys_KEY_SEQ_COL_NUM,
            kSQLForeignKeys_UPDATE_RULE_COL_NUM,
            kSQLForeignKeys_DELETE_RULE_COL_NUM,
            kSQLForeignKeys_FK_NAME_COL_NUM,
            kSQLForeignKeys_PK_NAME_COL_NUM,
            kSQLForeignKeys_DEFERRABILITY_COL_NUM
        ),
        ::testing::Values(SQL_OV_ODBC2, SQL_OV_ODBC3, 999)
    )
);

// SQLSpecialColumns Column name constant unit test
class SqlSpecialColumnsTest : public MetadataApiTestBase {
protected:
    const char* getExpectedColumnName() {
        if (odbcVersion == SQL_OV_ODBC2) {
            switch (column_index) {
                case kSQLColumnPrivileges_TABLE_CAT_COL_NUM:
                    return RsMetadataAPIHelper::getOdbc2ColumnName(0);
                case kSQLColumnPrivileges_TABLE_SCHEM_COL_NUM:
                    return RsMetadataAPIHelper::getOdbc2ColumnName(1);
                default:
                    return expectedSQLSpecialColumnsCol[column_index];
            }
        }
        return expectedSQLSpecialColumnsCol[column_index];
    }
};

TEST_P(SqlSpecialColumnsTest, VerifyColumnAttributes) {
    verifyColumnAttributes(
        "SQLSpecialColumns",
        getExpectedColumnName(),
        RsMetadataAPIHelper::getSqlSpecialColumnsColName,
        RsMetadataAPIHelper::kSpecialColumnsColDatatype,
        expectedSQLSpecialColumnsColDataType
    );
}

INSTANTIATE_TEST_SUITE_P(METADATA_API_HELPER_TEST_SUITE,
    SqlSpecialColumnsTest,
    ::testing::Combine(
        ::testing::Values(
            kSQLSpecialColumns_SCOPE ,
            kSQLSpecialColumns_COLUMN_NAME,
            kSQLSpecialColumns_DATA_TYPE,
            kSQLSpecialColumns_TYPE_NAME,
            kSQLSpecialColumns_COLUMN_SIZE,
            kSQLSpecialColumns_BUFFER_LENGTH,
            kSQLSpecialColumns_DECIMAL_DIGITS,
            kSQLSpecialColumns_PSEUDO_COLUMN
        ),
        ::testing::Values(SQL_OV_ODBC2, SQL_OV_ODBC3, 999)
    )
);

// SQLColumnPrivileges Column name constant unit test
class SqlColumnPrivilegesTest : public MetadataApiTestBase {
protected:
    const char* getExpectedColumnName() {
        if (odbcVersion == SQL_OV_ODBC2) {
            switch (column_index) {
                case kSQLColumnPrivileges_TABLE_CAT_COL_NUM:
                    return RsMetadataAPIHelper::getOdbc2ColumnName(0);
                case kSQLColumnPrivileges_TABLE_SCHEM_COL_NUM:
                    return RsMetadataAPIHelper::getOdbc2ColumnName(1);
                default:
                    return expectedSQLColumnPrivilegesCol[column_index];
            }
        }
        return expectedSQLColumnPrivilegesCol[column_index];
    }
};

TEST_P(SqlColumnPrivilegesTest, VerifyColumnAttributes) {
    verifyColumnAttributes(
        "SQLColumnPrivileges",
        getExpectedColumnName(),
        RsMetadataAPIHelper::getSqlColumnPrivilegesColName,
        RsMetadataAPIHelper::kColumnPrivilegesColDatatype,
        expectedSQLColumnPrivilegesColDataType
    );
}

INSTANTIATE_TEST_SUITE_P(METADATA_API_HELPER_TEST_SUITE,
    SqlColumnPrivilegesTest,
    ::testing::Combine(
        ::testing::Values(
            kSQLColumnPrivileges_TABLE_CAT_COL_NUM,
            kSQLColumnPrivileges_TABLE_SCHEM_COL_NUM,
            kSQLColumnPrivileges_TABLE_NAME_COL_NUM,
            kSQLColumnPrivileges_COLUMN_NAME_COL_NUM,
            kSQLColumnPrivileges_GRANTOR_COL_NUM,
            kSQLColumnPrivileges_GRANTEE_COL_NUM,
            kSQLColumnPrivileges_PRIVILEGE_COL_NUM,
            kSQLColumnPrivileges_IS_GRANTABLE_COL_NUM
        ),
        ::testing::Values(SQL_OV_ODBC2, SQL_OV_ODBC3, 999)
    )
);

// SQLTablePrivileges Column name constant unit test
class SqlTablePrivilegesTest : public MetadataApiTestBase {
protected:
    const char* getExpectedColumnName() {
        if (odbcVersion == SQL_OV_ODBC2) {
            switch (column_index) {
                case kSQLTablePrivileges_TABLE_CAT_COL_NUM:
                    return RsMetadataAPIHelper::getOdbc2ColumnName(0);
                case kSQLTablePrivileges_TABLE_SCHEM_COL_NUM:
                    return RsMetadataAPIHelper::getOdbc2ColumnName(1);
                default:
                    return expectedSQLTablePrivilegesCol[column_index];
            }
        }
        return expectedSQLTablePrivilegesCol[column_index];
    }
};

TEST_P(SqlTablePrivilegesTest, VerifyColumnAttributes) {
    verifyColumnAttributes(
        "SQLTablePrivileges",
        getExpectedColumnName(),
        RsMetadataAPIHelper::getSqlTablePrivilegesColName,
        RsMetadataAPIHelper::kTablePrivilegesColDatatype,
        expectedSQLTablePrivilegesColDataType
    );
}

INSTANTIATE_TEST_SUITE_P(METADATA_API_HELPER_TEST_SUITE,
    SqlTablePrivilegesTest,
    ::testing::Combine(
        ::testing::Values(
            kSQLTablePrivileges_TABLE_CAT_COL_NUM,
            kSQLTablePrivileges_TABLE_SCHEM_COL_NUM,
            kSQLTablePrivileges_TABLE_NAME_COL_NUM,
            kSQLTablePrivileges_GRANTOR_COL_NUM,
            kSQLTablePrivileges_GRANTEE_COL_NUM,
            kSQLTablePrivileges_PRIVILEGE_COL_NUM,
            kSQLTablePrivileges_IS_GRANTABLE_COL_NUM
        ),
        ::testing::Values(SQL_OV_ODBC2, SQL_OV_ODBC3, 999)
    )
);

// SQLProcedures Column name constant unit test
class SqlProceduresTest : public MetadataApiTestBase {
protected:
    const char* getExpectedColumnName() {
        if (odbcVersion == SQL_OV_ODBC2) {
            switch (column_index) {
                case kSQLProcedures_PROCEDURE_CAT_COL_NUM:
                    return RsMetadataAPIHelper::getOdbc2ColumnName(0);
                case kSQLProcedures_PROCEDURE_SCHEM_COL_NUM:
                    return RsMetadataAPIHelper::getOdbc2ColumnName(1);
                default:
                    return expectedSQLProceduresCol[column_index];
            }
        }
        return expectedSQLProceduresCol[column_index];
    }
};

TEST_P(SqlProceduresTest, VerifyColumnAttributes) {
    verifyColumnAttributes(
        "SQLProcedures",
        getExpectedColumnName(),
        RsMetadataAPIHelper::getSqlProceduresColName,
        RsMetadataAPIHelper::kProceduresColDatatype,
        expectedSQLProceduresColDataType
    );
}

INSTANTIATE_TEST_SUITE_P(METADATA_API_HELPER_TEST_SUITE,
    SqlProceduresTest,
    ::testing::Combine(
        ::testing::Values(
            kSQLProcedures_PROCEDURE_CAT_COL_NUM,
            kSQLProcedures_PROCEDURE_SCHEM_COL_NUM,
            kSQLProcedures_PROCEDURE_NAME_COL_NUM,
            kSQLProcedures_NUM_INPUT_PARAMS_COL_NUM,
            kSQLProcedures_NUM_OUTPUT_PARAMS_COL_NUM,
            kSQLProcedures_NUM_RESULT_SETS_COL_NUM,
            kSQLProcedures_REMARKS_COL_NUM,
            kSQLProcedures_PROCEDURE_TYPE_COL_NUM
        ),
        ::testing::Values(SQL_OV_ODBC2, SQL_OV_ODBC3, 999)
    )
);

// SQLProcedureColumns Column name constant unit test
class SqlProcedureColumnsTest : public MetadataApiTestBase {
protected:
    const char* getExpectedColumnName() {
        if (odbcVersion == SQL_OV_ODBC2) {
            static const std::map<short, int> odbc2ColumnMap = {
                {kSQLProcedureColumns_PROCEDURE_CAT_COL_NUM, 0},
                {kSQLProcedureColumns_PROCEDURE_SCHEM_COL_NUM, 1},
                {kSQLProcedureColumns_COLUMN_SIZE_COL_NUM, 2},
                {kSQLProcedureColumns_BUFFER_LENGTH_COL_NUM, 3},
                {kSQLProcedureColumns_DECIMAL_DIGITS_COL_NUM, 4},
                {kSQLProcedureColumns_NUM_PREC_RADIX_COL_NUM, 5}
            };

            auto it = odbc2ColumnMap.find(column_index);
            if (it != odbc2ColumnMap.end()) {
                return RsMetadataAPIHelper::getOdbc2ColumnName(it->second);
            }
        }
        return expectedSQLProcedureColumnsCol[column_index];
    }
};

TEST_P(SqlProcedureColumnsTest, VerifyColumnAttributes) {
    verifyColumnAttributes(
        "SQLProcedureColumns",
        getExpectedColumnName(),
        RsMetadataAPIHelper::getSqlProcedureColumnsColName,
        RsMetadataAPIHelper::kProcedureColumnsColDatatype,
        expectedSQLProcedureColumnsColDataType
    );
}

INSTANTIATE_TEST_SUITE_P(METADATA_API_HELPER_TEST_SUITE,
    SqlProcedureColumnsTest,
    ::testing::Combine(
        ::testing::Values(
            kSQLProcedureColumns_PROCEDURE_CAT_COL_NUM,
            kSQLProcedureColumns_PROCEDURE_SCHEM_COL_NUM,
            kSQLProcedureColumns_PROCEDURE_NAME_COL_NUM,
            kSQLProcedureColumns_COLUMN_NAME_COL_NUM,
            kSQLProcedureColumns_COLUMN_TYPE_COL_NUM,
            kSQLProcedureColumns_DATA_TYPE_COL_NUM,
            kSQLProcedureColumns_TYPE_NAME_COL_NUM,
            kSQLProcedureColumns_COLUMN_SIZE_COL_NUM,
            kSQLProcedureColumns_BUFFER_LENGTH_COL_NUM,
            kSQLProcedureColumns_DECIMAL_DIGITS_COL_NUM,
            kSQLProcedureColumns_NUM_PREC_RADIX_COL_NUM,
            kSQLProcedureColumns_NULLABLE_COL_NUM,
            kSQLProcedureColumns_REMARKS_COL_NUM,
            kSQLProcedureColumns_COLUMN_DEF_COL_NUM,
            kSQLProcedureColumns_SQL_DATA_TYPE_COL_NUM,
            kSQLProcedureColumns_SQL_DATETIME_SUB_COL_NUM,
            kSQLProcedureColumns_CHAR_OCTET_LENGTH_COL_NUM,
            kSQLProcedureColumns_ORDINAL_POSITION_COL_NUM,
            kSQLProcedureColumns_IS_NULLABLE_COL_NUM
        ),
        ::testing::Values(SQL_OV_ODBC2, SQL_OV_ODBC3, 999)
    )
);


// Unit test for helper function getNullable
TEST(METADATA_API_HELPER_TEST_SUITE, test_getnullable) {
    ASSERT_EQ(RsMetadataAPIHelper::getNullable("YES"), 1);
    ASSERT_EQ(RsMetadataAPIHelper::getNullable("NO"), 0);
    ASSERT_EQ(RsMetadataAPIHelper::getNullable("ELSE"), 2);
}

// Unit test for helper function getRSType
class TestGetRsType :public ::testing::TestWithParam<std::tuple<std::string, bool>> {
    protected:
        static std::map<std::string, std::string> dataType2RsTypeMap;
        static std::map<std::string, DATA_TYPE_RES> dataTypeMap;
        static void SetUpTestSuite();

        void SetUp() override {
            std::tie(dataType, isODBC_SpecV2) = GetParam();
        }
        
        std::string dataType;
        bool isODBC_SpecV2;
};

std::map<std::string, std::string> TestGetRsType::dataType2RsTypeMap;
std::map<std::string, DATA_TYPE_RES> TestGetRsType::dataTypeMap;


void TestGetRsType::SetUpTestSuite() {
    auto dataTypeName = RsMetadataAPIHelper::getDataTypeNameStruct();
    auto redshiftTypeName = RsMetadataAPIHelper::getRedshiftTypeNameStruct();

    dataType2RsTypeMap = {
        {dataTypeName.ksmallint, redshiftTypeName.kint2},
        {dataTypeName.kinteger, redshiftTypeName.kint4},
        {dataTypeName.kbigint, redshiftTypeName.kint8},
        {dataTypeName.knumeric, redshiftTypeName.knumeric},
        {dataTypeName.kreal, redshiftTypeName.kfloat4},
        {dataTypeName.kdouble_precision, redshiftTypeName.kfloat8},
        {dataTypeName.kboolean, redshiftTypeName.kbool},
        {dataTypeName.kcharacter, redshiftTypeName.kchar},
        {dataTypeName.kcharacter_varying, redshiftTypeName.kvarchar},
        {dataTypeName.kdate, redshiftTypeName.kdate},
        {dataTypeName.ktime_without_time_zone, redshiftTypeName.ktime},
        {dataTypeName.ktime, redshiftTypeName.ktime},
        {dataTypeName.ktime_with_time_zone, redshiftTypeName.ktimetz},
        {dataTypeName.ktimetz, redshiftTypeName.ktimetz},
        {dataTypeName.ktimestamp_without_time_zone, redshiftTypeName.ktimestamp},
        {dataTypeName.ktimestamp, redshiftTypeName.ktimestamp},
        {dataTypeName.ktimestamp_with_time_zone, redshiftTypeName.ktimestamptz},
        {dataTypeName.ktimestamptz, redshiftTypeName.ktimestamptz},
        {dataTypeName.kinterval_year_to_month, redshiftTypeName.kintervaly2m},
        {dataTypeName.kinterval_day_to_second, redshiftTypeName.kintervald2s},
        {dataTypeName.ksuper, redshiftTypeName.ksuper},
        {dataTypeName.kgeometry, redshiftTypeName.kgeometry},
        {dataTypeName.kgeography, redshiftTypeName.kgeography},
        {dataTypeName.kstring, dataTypeName.kstring},
        {dataTypeName.kbinary, dataTypeName.kbinary},
        {dataTypeName.karray, dataTypeName.karray},
        {dataTypeName.kmap, dataTypeName.kmap},
        {dataTypeName.kstruct, dataTypeName.kstruct}
    };

    dataTypeMap = {
        {dataTypeName.ksmallint, {SQL_SMALLINT, SQL_SMALLINT, SQL_SMALLINT, kNotApplicable, 5, kNotApplicable, sizeof(short)}},
        {dataTypeName.kinteger, {SQL_INTEGER, SQL_INTEGER, SQL_INTEGER, kNotApplicable, 10, kNotApplicable, sizeof(int)}},
        {dataTypeName.kbigint, {SQL_BIGINT, SQL_BIGINT, SQL_BIGINT, kNotApplicable, 19, kNotApplicable, sizeof(long long)}},
        {dataTypeName.knumeric, {SQL_NUMERIC, SQL_NUMERIC, SQL_NUMERIC, kNotApplicable, numeric_precision, numeric_scale, 8}},
        {dataTypeName.kreal, {SQL_REAL, SQL_REAL, SQL_REAL, kNotApplicable, 7, 6, sizeof(float)}},
        {dataTypeName.kdouble_precision, {SQL_DOUBLE, SQL_DOUBLE, SQL_DOUBLE, kNotApplicable, 15, 15, sizeof(double)}},
        {dataTypeName.kboolean, {SQL_BIT, SQL_BIT, SQL_BIT, kNotApplicable, 1, kNotApplicable, sizeof(bool)}},
        {dataTypeName.kcharacter, {SQL_CHAR, SQL_CHAR, SQL_CHAR, kNotApplicable, character_maximum_length, kNotApplicable, character_maximum_length}},
        {dataTypeName.kcharacter_varying, {SQL_VARCHAR, SQL_VARCHAR, SQL_VARCHAR, kNotApplicable, character_maximum_length, kNotApplicable, character_maximum_length}},
        {dataTypeName.kstring, {SQL_VARCHAR, SQL_VARCHAR, SQL_VARCHAR, kNotApplicable, character_maximum_length, kNotApplicable, character_maximum_length}},
        {dataTypeName.kdate, {SQL_TYPE_DATE, SQL_DATE, SQL_DATETIME, SQL_CODE_DATE, 10, kNotApplicable, sizeof(SQL_DATE_STRUCT)}},
        {dataTypeName.ktime_without_time_zone, {SQL_TYPE_TIME, SQL_TIME, SQL_DATETIME, SQL_CODE_TIME, 15, 6, sizeof(SQL_TIME_STRUCT)}},
        {dataTypeName.ktime, {SQL_TYPE_TIME, SQL_TIME, SQL_DATETIME, SQL_CODE_TIME, 15, 6, sizeof(SQL_TIME_STRUCT)}},
        {dataTypeName.ktime_with_time_zone, {SQL_TYPE_TIME, SQL_TIME, SQL_DATETIME, SQL_CODE_TIME, 21, 6, sizeof(SQL_TIME_STRUCT)}},
        {dataTypeName.ktimetz, {SQL_TYPE_TIME, SQL_TIME, SQL_DATETIME, SQL_CODE_TIME, 21, 6, sizeof(SQL_TIME_STRUCT)}},
        {dataTypeName.ktimestamp_without_time_zone, {SQL_TYPE_TIMESTAMP, SQL_TIMESTAMP, SQL_DATETIME, SQL_CODE_TIMESTAMP, 29, 6, sizeof(SQL_TIMESTAMP_STRUCT)}},
        {dataTypeName.ktimestamp, {SQL_TYPE_TIMESTAMP, SQL_TIMESTAMP, SQL_DATETIME, SQL_CODE_TIMESTAMP, 29, 6, sizeof(SQL_TIMESTAMP_STRUCT)}},
        {dataTypeName.ktimestamp_with_time_zone, {SQL_TYPE_TIMESTAMP, SQL_TIMESTAMP, SQL_DATETIME, SQL_CODE_TIMESTAMP, 35, 6, sizeof(SQL_TIMESTAMP_STRUCT)}},
        {dataTypeName.ktimestamptz, {SQL_TYPE_TIMESTAMP, SQL_TIMESTAMP, SQL_DATETIME, SQL_CODE_TIMESTAMP, 35, 6, sizeof(SQL_TIMESTAMP_STRUCT)}},
        {dataTypeName.kinterval_year_to_month, {SQL_INTERVAL_YEAR_TO_MONTH, SQL_INTERVAL_YEAR_TO_MONTH, SQL_INTERVAL, SQL_CODE_YEAR_TO_MONTH, 32, kNotApplicable, sizeof(INTERVALY2M_STRUCT)}},
        {dataTypeName.kinterval_day_to_second, {SQL_INTERVAL_DAY_TO_SECOND, SQL_INTERVAL_DAY_TO_SECOND, SQL_INTERVAL, SQL_CODE_DAY_TO_SECOND, 64, 6, sizeof(INTERVALD2S_STRUCT)}},
        {dataTypeName.kbinary, {SQL_LONGVARBINARY, SQL_LONGVARBINARY, SQL_LONGVARBINARY, kNotApplicable, kNotApplicable, kNotApplicable, kNotApplicable}},
        {dataTypeName.ksuper, {SQL_LONGVARCHAR, SQL_LONGVARCHAR, SQL_LONGVARCHAR, kNotApplicable, kNotApplicable, kNotApplicable, kNotApplicable}},
        {dataTypeName.karray, {SQL_LONGVARCHAR, SQL_LONGVARCHAR, SQL_LONGVARCHAR, kNotApplicable, kNotApplicable, kNotApplicable, kNotApplicable}},
        {dataTypeName.kmap, {SQL_LONGVARCHAR, SQL_LONGVARCHAR, SQL_LONGVARCHAR, kNotApplicable, kNotApplicable, kNotApplicable, kNotApplicable}},
        {dataTypeName.kstruct, {SQL_LONGVARCHAR, SQL_LONGVARCHAR, SQL_LONGVARCHAR, kNotApplicable, kNotApplicable, kNotApplicable, kNotApplicable}},
        {dataTypeName.kgeometry, {SQL_LONGVARBINARY, SQL_LONGVARBINARY, SQL_LONGVARBINARY, kNotApplicable, kNotApplicable, kNotApplicable, kNotApplicable}},
        {dataTypeName.kgeography, {SQL_LONGVARBINARY, SQL_LONGVARBINARY, SQL_LONGVARBINARY, kNotApplicable, kNotApplicable, kNotApplicable, kNotApplicable}}
    };
}

TEST_P(TestGetRsType, test_rsType) {
    short sqlType = 0, sqlDataType = 0, sqlDateSub = 0;
    std::string rsType;
    TypeInfoResult typeInfoResult = RsMetadataAPIHelper::getTypeInfo(dataType, isODBC_SpecV2);
    if(typeInfoResult.found){
        DATA_TYPE_INFO typeInfo = typeInfoResult.typeInfo;
        sqlType = typeInfo.sqlType;
        sqlDataType = typeInfo.sqlDataType;
        sqlDateSub = typeInfo.sqlDateSub;
        rsType = typeInfo.typeName;
    }
    else{
        rsType = dataType;
    }
    ASSERT_EQ(rsType, dataType2RsTypeMap[dataType]);
}

TEST_P(TestGetRsType, test_rsTypeLen) {
    std::string rsType;
    TypeInfoResult typeInfoResult = RsMetadataAPIHelper::getTypeInfo(dataType, isODBC_SpecV2);
    if(typeInfoResult.found){
        DATA_TYPE_INFO typeInfo = typeInfoResult.typeInfo;
        rsType = typeInfo.typeName;
    }
    else{
        rsType = dataType;
    }
    ASSERT_EQ(rsType.size(), dataType2RsTypeMap[dataType].size());
}

TEST_P(TestGetRsType, test_sqlType) {
    short sqlType = 0;
    TypeInfoResult typeInfoResult = RsMetadataAPIHelper::getTypeInfo(dataType, isODBC_SpecV2);
    if(typeInfoResult.found){
        DATA_TYPE_INFO typeInfo = typeInfoResult.typeInfo;
        sqlType = typeInfo.sqlType;
    }
    if(isODBC_SpecV2){
        ASSERT_EQ(sqlType, dataTypeMap[dataType].sqlTypeODBC2);
    }
    else{
        ASSERT_EQ(sqlType, dataTypeMap[dataType].sqlType);
    }
    
}

TEST_P(TestGetRsType, test_sqlDataType) {
    short sqlDataType = 0;
    TypeInfoResult typeInfoResult = RsMetadataAPIHelper::getTypeInfo(dataType, isODBC_SpecV2);
    if(typeInfoResult.found){
        DATA_TYPE_INFO typeInfo = typeInfoResult.typeInfo;
        sqlDataType = typeInfo.sqlDataType;
    }
    ASSERT_EQ(sqlDataType, dataTypeMap[dataType].sqlDataType);
}

TEST_P(TestGetRsType, test_sqlDateSub) {
    short sqlDateSub = 0;
    TypeInfoResult typeInfoResult = RsMetadataAPIHelper::getTypeInfo(dataType, isODBC_SpecV2);
    if(typeInfoResult.found){
        DATA_TYPE_INFO typeInfo = typeInfoResult.typeInfo;
        sqlDateSub = typeInfo.sqlDateSub;
    }
    ASSERT_EQ(sqlDateSub, dataTypeMap[dataType].sqlDateSub);
}

TEST_P(TestGetRsType, test_columnSize) {
    std::string rsType;
    TypeInfoResult typeInfoResult = RsMetadataAPIHelper::getTypeInfo(dataType, isODBC_SpecV2);
    if(typeInfoResult.found){
        DATA_TYPE_INFO typeInfo = typeInfoResult.typeInfo;
        rsType = typeInfo.typeName;
    }
    else{
        rsType = dataType;
    }
    int columnSize = RsMetadataAPIHelper::getColumnSize(rsType, character_maximum_length, numeric_precision);
    ASSERT_EQ(columnSize, dataTypeMap[dataType].colSize);
}

TEST_P(TestGetRsType, test_decimalDigit) {
    std::string rsType;
    TypeInfoResult typeInfoResult = RsMetadataAPIHelper::getTypeInfo(dataType, isODBC_SpecV2);
    if(typeInfoResult.found){
        DATA_TYPE_INFO typeInfo = typeInfoResult.typeInfo;
        rsType = typeInfo.typeName;
    }
    else{
        rsType = dataType;
    }
    ASSERT_EQ(RsMetadataAPIHelper::getDecimalDigit(rsType, numeric_scale, 0, false), dataTypeMap[dataType].decimalDigit);
}

TEST_P(TestGetRsType, test_bufferLen) {
    std::string rsType;
    TypeInfoResult typeInfoResult = RsMetadataAPIHelper::getTypeInfo(dataType, isODBC_SpecV2);
    if(typeInfoResult.found){
        DATA_TYPE_INFO typeInfo = typeInfoResult.typeInfo;
        rsType = typeInfo.typeName;
    }
    else{
        rsType = dataType;
    }
    ASSERT_EQ(RsMetadataAPIHelper::getBufferLen(rsType, character_maximum_length, numeric_precision), dataTypeMap[dataType].bufferLen);
}

INSTANTIATE_TEST_SUITE_P(METADATA_API_HELPER_TEST_SUITE, TestGetRsType, 
    ::testing::Combine(
        ::testing::Values(
            RsMetadataAPIHelper::getDataTypeNameStruct().ksmallint,
            RsMetadataAPIHelper::getDataTypeNameStruct().kinteger,
            RsMetadataAPIHelper::getDataTypeNameStruct().kbigint,
            RsMetadataAPIHelper::getDataTypeNameStruct().knumeric,
            RsMetadataAPIHelper::getDataTypeNameStruct().kreal,
            RsMetadataAPIHelper::getDataTypeNameStruct().kdouble_precision,
            RsMetadataAPIHelper::getDataTypeNameStruct().kboolean,
            RsMetadataAPIHelper::getDataTypeNameStruct().kcharacter,
            RsMetadataAPIHelper::getDataTypeNameStruct().kcharacter_varying,
            RsMetadataAPIHelper::getDataTypeNameStruct().kstring,
            RsMetadataAPIHelper::getDataTypeNameStruct().kdate,
            RsMetadataAPIHelper::getDataTypeNameStruct().ktime,
            RsMetadataAPIHelper::getDataTypeNameStruct().ktime_without_time_zone,
            RsMetadataAPIHelper::getDataTypeNameStruct().ktimetz,
            RsMetadataAPIHelper::getDataTypeNameStruct().ktime_with_time_zone,
            RsMetadataAPIHelper::getDataTypeNameStruct().ktimestamp,
            RsMetadataAPIHelper::getDataTypeNameStruct().ktimestamp_without_time_zone,
            RsMetadataAPIHelper::getDataTypeNameStruct().ktimestamptz,
            RsMetadataAPIHelper::getDataTypeNameStruct().ktimestamp_with_time_zone,
            RsMetadataAPIHelper::getDataTypeNameStruct().kinterval_year_to_month,
            RsMetadataAPIHelper::getDataTypeNameStruct().kinterval_day_to_second,
            RsMetadataAPIHelper::getDataTypeNameStruct().kbinary,
            RsMetadataAPIHelper::getDataTypeNameStruct().ksuper,
            RsMetadataAPIHelper::getDataTypeNameStruct().karray,
            RsMetadataAPIHelper::getDataTypeNameStruct().kmap,
            RsMetadataAPIHelper::getDataTypeNameStruct().kstruct,
            RsMetadataAPIHelper::getDataTypeNameStruct().kgeometry,
            RsMetadataAPIHelper::getDataTypeNameStruct().kgeography
        ),
        ::testing::Values(true, false)
    ));

// Unit test for helper function initializeColumnNames
class TestInitializeColumnNames : public ::testing::Test {
    protected:
        TestInitializeColumnNames() {
            pStmt = nullptr;
        }
    
        void SetUp() override {
            RS_ENV_INFO* pEnv = new RS_ENV_INFO();
            RS_CONN_INFO* pConn = new RS_CONN_INFO(pEnv);
            pStmt = new RS_STMT_INFO(pConn);
        }
    
        void TearDown() override {
            if (pStmt) {
                if (pStmt->phdbc) {
                    if (pStmt->phdbc->phenv) {
                        delete pStmt->phdbc->phenv;
                    }
                    delete pStmt->phdbc;
                }
                delete pStmt;
            }
        }
    
        RS_STMT_INFO* pStmt;
};

// Structure to hold test parameters
struct ProcedureFunctionColumnTypeTestParam {
    std::string input;           // Input parameter type
    int expected_output;         // Expected parameter type
};

class ProcedureFunctionColumnTypeTest : public ::testing::TestWithParam<ProcedureFunctionColumnTypeTestParam> {};

// Test cases
TEST_P(ProcedureFunctionColumnTypeTest, ParameterTypeMapping) {
    const auto& param = GetParam();
    EXPECT_EQ(RsMetadataAPIHelper::getProcedureFunctionColumnType(param.input), param.expected_output)
        << "Failed for input: " << param.input;
}

// Define test cases
INSTANTIATE_TEST_SUITE_P(
    ProcedureFunctionColumnTypesTestSuite,
    ProcedureFunctionColumnTypeTest,
    ::testing::Values(
        // Normal cases with different casings
        ProcedureFunctionColumnTypeTestParam{
            "IN",
            SQL_PARAM_INPUT
        },
        ProcedureFunctionColumnTypeTestParam{
            "INOUT",
            SQL_PARAM_INPUT_OUTPUT
        },
        ProcedureFunctionColumnTypeTestParam{
            "OUT",
            SQL_PARAM_OUTPUT
        },
        ProcedureFunctionColumnTypeTestParam{
            "TABLE",
            SQL_RESULT_COL
        },
        ProcedureFunctionColumnTypeTestParam{
            "RETURN",
            SQL_RETURN_VALUE
        },
        
        // Edge cases
        ProcedureFunctionColumnTypeTestParam{
            "",
            SQL_PARAM_TYPE_UNKNOWN,
        },
        ProcedureFunctionColumnTypeTestParam{
            "UNKNOWN",
            SQL_PARAM_TYPE_UNKNOWN,
        },
        ProcedureFunctionColumnTypeTestParam{
            "IN ",
            SQL_PARAM_TYPE_UNKNOWN
        },
        ProcedureFunctionColumnTypeTestParam{
            " IN",
            SQL_PARAM_TYPE_UNKNOWN
        },
        ProcedureFunctionColumnTypeTestParam{
            "INPUT",
            SQL_PARAM_TYPE_UNKNOWN
        }
    )
);

// Optional: Add some non-parameterized tests for specific scenarios
TEST_F(ProcedureFunctionColumnTypeTest, NullParameterType) {
    std::string empty;
    EXPECT_EQ(RsMetadataAPIHelper::getProcedureFunctionColumnType(empty), SQL_PARAM_TYPE_UNKNOWN);
}

class RsMetadataAPIHelperPatternMatchTest
    : public testing::TestWithParam<std::tuple<std::string, std::string, bool>> {
protected:
    RsMetadataAPIHelper helper;
};

TEST_P(RsMetadataAPIHelperPatternMatchTest, PatternMatchResults) {
    const auto& [str, pattern, expected] = GetParam();
    EXPECT_EQ(helper.patternMatch(str, pattern), expected)
        << "Failed for string: '" << str
        << "' with pattern: '" << pattern
        << "'. Expected: " << expected;
}

INSTANTIATE_TEST_SUITE_P(
    PatternMatchTests,
    RsMetadataAPIHelperPatternMatchTest,
    testing::Values(
        // Empty pattern tests
        std::make_tuple("", "", true),
        std::make_tuple("hello", "", true),
        std::make_tuple("123", "", true),

        // Pattern with only % tests
        std::make_tuple("", "%", true),
        std::make_tuple("hello", "%", true),
        std::make_tuple("hello", "%%%", true),
        std::make_tuple("123!@#", "%%%%", true),

        // Empty string tests with non-empty patterns
        std::make_tuple("", "a", false),
        std::make_tuple("", "_", false),
        std::make_tuple("", "a%", false),
        std::make_tuple("", "%a", false),

        // Single character tests
        std::make_tuple("a", "a", true),
        std::make_tuple("a", "b", false),
        std::make_tuple("a", "_", true),
        std::make_tuple("a", "%", true),

        // Underscore tests
        std::make_tuple("hello", "_ello", true),
        std::make_tuple("hello", "h_llo", true),
        std::make_tuple("hello", "he_lo", true),
        std::make_tuple("hello", "hel_o", true),
        std::make_tuple("hello", "hell_", true),
        std::make_tuple("hello", "_____", true),
        std::make_tuple("hello", "____", false),
        std::make_tuple("hello", "______", false),

        // Percent tests
        std::make_tuple("hello", "h%", true),
        std::make_tuple("hello", "%o", true),
        std::make_tuple("hello", "h%o", true),
        std::make_tuple("hello", "%l%", true),
        std::make_tuple("hello", "h%l%o", true),
        std::make_tuple("hello", "%hello%", true),
        std::make_tuple("hello", "%hel%", true),
        std::make_tuple("hello", "%h%l%o%", true),

        // Mixed underscore and percent tests
        std::make_tuple("hello", "h_%o", true),
        std::make_tuple("hello", "h_%", true),
        std::make_tuple("hello", "%_l%", true),
        std::make_tuple("hello", "_el%", true),
        std::make_tuple("hello", "%l_o", true),

        // Negative test cases
        std::make_tuple("hello", "world", false),
        std::make_tuple("hello", "hell", false),
        std::make_tuple("hello", "hello!", false),
        std::make_tuple("hello", "h%x", false),
        std::make_tuple("hello", "h_x", false),

        // Special characters tests
        std::make_tuple("h.llo", "h.llo", true),
        std::make_tuple("h*llo", "h*llo", true),
        std::make_tuple("h[llo", "h[llo", true),
        std::make_tuple("h#llo", "h#llo", true),
        std::make_tuple("h@llo", "_@_lo", true),

        // Case sensitivity tests
        std::make_tuple("Hello", "hello", false),
        std::make_tuple("HELLO", "hello", false),
        std::make_tuple("Hello", "H%o", true),
        std::make_tuple("HELLO", "H_LLO", true),

        // Number tests
        std::make_tuple("123", "123", true),
        std::make_tuple("123", "1%3", true),
        std::make_tuple("123", "1_3", true),
        std::make_tuple("123", "%2%", true),

        // Long string tests
        std::make_tuple("HelloWorld123", "Hello%123", true),
        std::make_tuple("HelloWorld123", "Hello_%_123", true),
        std::make_tuple("HelloWorld123", "%World%", true),
        std::make_tuple("HelloWorld123", "HelloWorld124", false),

        // Multiple consecutive wildcards
        std::make_tuple("abc", "a%%b%c", true),
        std::make_tuple("abc", "a___%c", false),
        std::make_tuple("abc", "%_%_%", true),
        std::make_tuple("a", "%%_%", true),
        std::make_tuple("", "%%", true),

        // Pattern longer than string
        std::make_tuple("hi", "hello", false),
        std::make_tuple("hi", "h%ello", false),
        std::make_tuple("hi", "h_llo", false),
        std::make_tuple("a", "ab", false),

        // String longer than pattern
        std::make_tuple("hello", "hi", false),
        std::make_tuple("hello", "h", false),
        std::make_tuple("hello", "he", false),

        // Patterns starting/ending with wildcards
        std::make_tuple("%hello", "%hello", true),
        std::make_tuple("hello%", "hello%", true),
        std::make_tuple("hello", "%hello", true),
        std::make_tuple("hello", "hello%", true),
        std::make_tuple("hello", "%ello", true),
        std::make_tuple("hello", "hell%", true),

        // Complex mixed patterns
        std::make_tuple("database_table_name", "database_%_name", true),
        std::make_tuple("database_table_name", "database_%name", true),
        std::make_tuple("database_table_name", "data%table%", true),
        std::make_tuple("database_table_name", "data%_table_%", true),
        std::make_tuple("database_table_name", "_atabase_table_nam_", true),

        // Whitespace and special characters
        std::make_tuple("hello world", "hello%world", true),
        std::make_tuple("hello world", "hello_world", true),
        std::make_tuple("hello  world", "hello__world", true),
        std::make_tuple("hello\tworld", "hello_world", true),
        std::make_tuple("hello\nworld", "hello_world", true),
        std::make_tuple("test@email.com", "test%email%", true),
        std::make_tuple("file.txt", "%.txt", true),
        std::make_tuple("file.txt", "file.%", true),

        // SQL injection-like patterns
        std::make_tuple("'; DROP TABLE", "%DROP%", true),
        std::make_tuple("'; DROP TABLE", "_%TABLE", true),
        std::make_tuple("admin'--", "admin%", true),

        // Numeric patterns
        std::make_tuple("12345", "1%5", true),
        std::make_tuple("12345", "1_3_5", true),
        std::make_tuple("12345", "%3%", true),
        std::make_tuple("12.34", "%.%", true),
        std::make_tuple("12.34", "12._4", true),

        // Repeated characters
        std::make_tuple("aaaa", "a%", true),
        std::make_tuple("aaaa", "a%a", true),
        std::make_tuple("aaaa", "aa%", true),
        std::make_tuple("aaaa", "%aa", true),
        std::make_tuple("aaaa", "a_a_", true),
        std::make_tuple("aaaa", "____", true),
        std::make_tuple("aaaa", "_____", false),

        // Edge cases with single wildcards
        std::make_tuple("a", "_%", true),
        std::make_tuple("a", "%_", true),
        std::make_tuple("ab", "_%", true),
        std::make_tuple("ab", "%_", true),
        std::make_tuple("ab", "_%_", true),

        // Patterns that should fail
        std::make_tuple("hello", "h%x%o", false),
        std::make_tuple("hello", "h_x_o", false),
        std::make_tuple("hello", "%x%", false),
        std::make_tuple("hello", "_x_", false),

        // Very specific patterns
        std::make_tuple("column_name", "column%", true),
        std::make_tuple("column_name", "%name", true),
        std::make_tuple("column_name", "column_name", true),
        std::make_tuple("column_name", "column_%", true),
        std::make_tuple("column_name", "%_name", true),
        std::make_tuple("column_name", "col%nam%", true),

        // Boundary conditions
        std::make_tuple("x", "x%", true),
        std::make_tuple("x", "%x", true),
        std::make_tuple("x", "_", true),
        std::make_tuple("xy", "x_", true),
        std::make_tuple("xy", "_y", true),

        // Multiple underscores with percent
        std::make_tuple("hello", "h_%_%o", true),
        std::make_tuple("hello", "h%_%o", true),
        std::make_tuple("hello", "%_%_%", true),
        std::make_tuple("he", "%_%", true),
        std::make_tuple("h", "%_%", true),

        // Patterns with literal % and _ (escaped)
        std::make_tuple("hello%world", "hello\\%world", true),
        std::make_tuple("hello_world", "hello\\_world", true),
        std::make_tuple("test\\file", "test\\\\file", true)
    )
);

// Additional non-parameterized tests for specific edge cases
TEST_F(RsMetadataAPIHelperPatternMatchTest, VeryLongString) {
    std::string longStr(1000, 'a');  // String of 1000 'a's
    EXPECT_TRUE(helper.patternMatch(longStr, "%"));
    EXPECT_TRUE(helper.patternMatch(longStr, "a%a"));
    EXPECT_TRUE(helper.patternMatch(longStr, std::string(1000, 'a')));
    EXPECT_FALSE(helper.patternMatch(longStr, std::string(1001, 'a')));
}

TEST_F(RsMetadataAPIHelperPatternMatchTest, ConsecutiveWildcards) {
    EXPECT_TRUE(helper.patternMatch("hello", "h%_%o"));
    EXPECT_TRUE(helper.patternMatch("hello", "h_%_%o"));
    EXPECT_TRUE(helper.patternMatch("hello", "h%_%_%o"));
}
