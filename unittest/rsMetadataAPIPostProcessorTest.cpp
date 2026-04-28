/*-------------------------------------------------------------------------
 *
 * Copyright(c) 2025, Amazon.com, Inc. or Its Affiliates. All rights reserved.
 *
 * Author: mamluis
 *-------------------------------------------------------------------------
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <vector>

#include "rsodbc.h"
#include <sql.h>
#include <sqlext.h>
#include "rsMetadataAPIPostProcessor.h"
#include "rsMetadataAPIHelper.h"
#include "rsexecute.h"
#include "rsutil.h"

// Function types from SQL_PROCEDURE_TYPE in SQLProcedures
#ifndef SQL_PT_PROCEDURE
#define SQL_PT_PROCEDURE            1
#endif


/**
 * @brief Base test fixture for metadata API tests
 *
 * Provides common setup and teardown functionality for ODBC handle management
 * and utility functions for string handling.
 */
class RsMetadataAPIPostProcessorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize environment, connection, and statement handles
        SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
        SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
        SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
        SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    }

    void TearDown() override {
        // Free handles in reverse order
        if (stmt != SQL_NULL_HANDLE) SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        if (dbc != SQL_NULL_HANDLE) SQLFreeHandle(SQL_HANDLE_DBC, dbc);
        if (env != SQL_NULL_HANDLE) SQLFreeHandle(SQL_HANDLE_ENV, env);
    }

    SQLHENV env = SQL_NULL_HANDLE;
    SQLHDBC dbc = SQL_NULL_HANDLE;
    SQLHSTMT stmt = SQL_NULL_HANDLE;
};

/**
 * @brief Template class for testing metadata API post-processing
 *
 * Provides a framework for testing different types of metadata API results
 * with common validation logic for empty, single, and multiple row results.
 *
 * @tparam T The type of metadata result structure being tested
 */
template<typename T>
class MetadataResultSetTest : public RsMetadataAPIPostProcessorTest {
protected:
    /**
     * @brief Mixed case variants of Glue data types for testing case-insensitive
     * type resolution. Glue catalogs may return data type names in arbitrary
     * casing; the driver must normalize them to lowercase before looking up
     * type information. Includes both "string" (SQL_VARCHAR) and "binary"
     * (SQL_LONGVARBINARY) Glue types.
     */
    std::vector<std::string> mixedCaseDataType = {
        "String", "sTring", "STRING", "string",
        "Binary", "bInary", "BINARY", "binary"};

    /**
     * @brief Helper to normalize a data type name to lowercase for comparison
     */
    static std::string toLower(const std::string& s) {
        std::string result = s;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }

    /**
     * @brief Check if a data type (case-insensitive) is a binary Glue type
     */
    static bool isBinaryType(const std::string& dataType) {
        return toLower(dataType) == "binary";
    }

    /**
     * @brief Process the result set through the metadata API
     * @return SQLRETURN status code
     */
    virtual SQLRETURN processResult(SQLHSTMT stmt, const std::vector<T>& rs) {
        return SQL_ERROR;
    }

    /**
     * @brief Get the expected number of columns in the result set
     * @return Column count
     */
    virtual int getExpectedColumnCount() {
        return 0;
    }

    /**
     * @brief Get the expected column names and their positions
     * @return Vector of column index and name pairs
     */
    virtual std::vector<std::pair<int, const char*>> getExpectedColumns() {
        return {};
    }

    /**
     * @brief Get expected values for result validation
     * @return Vector of column index and value pairs
     */
    virtual std::vector<std::pair<int, const char*>> getExpectedValues(const T& record) {
        return {};
    }

    /**
     * @brief Create test records for the result set
     * @param count Number of records to create
     * @param maxLength whether to use name with max length
     * @return Vector of test records
     */
    virtual std::vector<T> createTestRecords(int count, bool maxLength) {
        return {};
    }

    /**
     * @brief Create test records for the Glue data type result set
     * @return Vector of test records
     */
    virtual std::vector<T> createMixedCaseGlueDataTypeTestRecords() {
        return {};
    }

    /**
     * @brief Validate the processed results
     * @param records The test records to validate against
     * @param maxLength whether to use name with max length
     */
    virtual void validateResults(const std::vector<T>& records, bool maxLength) {};

    /**
     * @brief Validate the processed results for mixed case Glue data type
     * @param records The test records to validate against
     */
    virtual void validateMixedCaseGlueDataTypeResults(const std::vector<T>& records) {};

    static const std::string& getMaxStr() {
        static const std::string maxStr(NAMEDATALEN - 1, 'X');
        return maxStr;
    }

    void TestEmptyResult() {
        std::vector<T> emptyRS;

        SQLRETURN rc = processResult(stmt, emptyRS);
        EXPECT_EQ(rc, SQL_SUCCESS);

        ValidateResultSetProperties(0);
        ValidateColumnMetadata();
    }

    void TestSingleResult() {
        std::vector<T> testRS = createTestRecords(1, false);

        SQLRETURN rc = processResult(stmt, testRS);
        EXPECT_EQ(rc, SQL_SUCCESS);

        ValidateResultSetProperties(1);
        ValidateColumnMetadata();
        validateResults(testRS, false);
    }

    void TestMultipleResults(int rowCount) {
        std::vector<T> testRS = createTestRecords(rowCount, false);

        SQLRETURN rc = processResult(stmt, testRS);
        EXPECT_EQ(rc, SQL_SUCCESS);

        ValidateResultSetProperties(rowCount);
        ValidateColumnMetadata();
        validateResults(testRS, false);
    }

    void TestMaxLength() {
        std::vector<T> testRS = createTestRecords(1, true);

        SQLRETURN rc = processResult(stmt, testRS);
        EXPECT_EQ(rc, SQL_SUCCESS);

        ValidateResultSetProperties(1);
        ValidateColumnMetadata();
        validateResults(testRS, true);
    }

    void TestNullStmt() {
        SQLHSTMT nullStmt = SQL_NULL_HANDLE;
        std::vector<T> testRS;

        SQLRETURN rc = processResult(nullStmt, testRS);
        EXPECT_EQ(rc, SQL_INVALID_HANDLE);
    }

    void TestMixedCaseGlueDatatype() {
        std::vector<T> testRS = createMixedCaseGlueDataTypeTestRecords();

        SQLRETURN rc = processResult(stmt, testRS);
        EXPECT_EQ(rc, SQL_SUCCESS);

        ValidateResultSetProperties(mixedCaseDataType.size());
        ValidateColumnMetadata();
        validateMixedCaseGlueDataTypeResults(testRS);
    }

private:
    void ValidateResultSetProperties(int expectedRows) {
        RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
        ASSERT_NE(pStmt, nullptr) << "Statement cast failed";
        ASSERT_NE(pStmt->pResultHead, nullptr) << "Result head is null";
        EXPECT_EQ(pStmt->pResultHead->iNumberOfRowsInMem, expectedRows);
        EXPECT_EQ(pStmt->pResultHead->iCurRow, -1);
    }

    void ValidateColumnMetadata() {
        if (getExpectedColumnCount() > 0) {
            RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
            PGresult *res = pStmt->pResultHead->pgResult;
            ASSERT_NE(res, nullptr) << "PGresult is null";

            for (const auto& col : getExpectedColumns()) {
                EXPECT_STREQ(PQfname(res, col.first), col.second)
                    << "Column name mismatch at index " << col.first;
            }
            EXPECT_EQ(PQnfields(res), getExpectedColumnCount());
        }
    }
};


/**
 * @brief Test implementation for SQLTables metadata API
 *
 * Implements specific test logic for table metadata discovery such as
 * column count, column name and data for post-processing
 */
class SQLTablesTest : public MetadataResultSetTest<SHOWTABLESResult> {
protected:
    SQLRETURN processResult(SQLHSTMT stmt, const std::vector<SHOWTABLESResult>& rs) override {
        return RsMetadataAPIPostProcessor::sqlTablesPostProcessing(stmt, "TABLE", rs);
    }

    int getExpectedColumnCount() override { return 10; }

    std::vector<std::pair<int, const char*>> getExpectedColumns() override {
        return {
            {kSQLTables_TABLE_CATALOG_COL_NUM, "TABLE_CAT"},
            {kSQLTables_TABLE_SCHEM_COL_NUM, "TABLE_SCHEM"},
            {kSQLTables_TABLE_NAME_COL_NUM, "TABLE_NAME"},
            {kSQLTables_TABLE_TYPE_COL_NUM, "TABLE_TYPE"},
            {kSQLTables_REMARKS_COL_NUM, "REMARKS"},
            {kSQLTables_OWNER_COL_NUM, "OWNER"},
            {kSQLTables_LAST_ALTERED_TIME_COL_NUM, "LAST_ALTERED_TIME"},
            {kSQLTables_LAST_MODIFIED_TIME_COL_NUM, "LAST_MODIFIED_TIME"},
            {kSQLTables_DIST_STYLE_COL_NUM, "DIST_STYLE"},
            {kSQLTables_TABLE_SUBTYPE_COL_NUM, "TABLE_SUBTYPE"}
        };
    }

    std::vector<SHOWTABLESResult> createTestRecords(int count,
                                                    bool maxLength) override {
        std::vector<SHOWTABLESResult> results;

        for (int i = 1; i <= count; i++) {
            SHOWTABLESResult result{}; // Zero-initialize
            std::string suffix = std::to_string(i);

            if (maxLength) {
                result.database_name = getMaxStr();
                result.schema_name = getMaxStr();
                result.table_name = getMaxStr();
            } else {
                result.database_name = "catalog" + suffix;
                result.schema_name = "schema" + suffix;
                result.table_name = "table" + suffix;
            }
            result.table_type = "TABLE";
            result.remarks = "remark" + suffix;
            results.push_back(result);
        }
        return results;
    }

    void validateResults(const std::vector<SHOWTABLESResult>& records, bool maxLength) override {
        RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
        PGresult *res = pStmt->pResultHead->pgResult;

        ASSERT_EQ(records.size(), static_cast<size_t>(PQntuples(res)))
            << "Result set row count mismatch";

        for (size_t i = 0; i < records.size(); i++) {
            std::string suffix = std::to_string(i + 1);

            if (maxLength) {
                EXPECT_STREQ(PQgetvalue(res, i, kSQLTables_TABLE_CATALOG_COL_NUM), getMaxStr().c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLTables_TABLE_SCHEM_COL_NUM), getMaxStr().c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLTables_TABLE_NAME_COL_NUM), getMaxStr().c_str());
            } else {
                EXPECT_STREQ(PQgetvalue(res, i, kSQLTables_TABLE_CATALOG_COL_NUM), ("catalog" + suffix).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLTables_TABLE_SCHEM_COL_NUM), ("schema" + suffix).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLTables_TABLE_NAME_COL_NUM), ("table" + suffix).c_str());
            }
            EXPECT_STREQ(PQgetvalue(res, i, kSQLTables_TABLE_TYPE_COL_NUM), "TABLE");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLTables_REMARKS_COL_NUM), ("remark" + suffix).c_str());
        }
    }
};

TEST_F(SQLTablesTest, EmptyResultSet) { TestEmptyResult(); }

TEST_F(SQLTablesTest, PopulatedResultSet) { TestSingleResult(); }

TEST_F(SQLTablesTest, MultipleRowResultSet) { TestMultipleResults(3); }

TEST_F(SQLTablesTest, MaxLength) { TestMaxLength(); }

TEST_F(SQLTablesTest, NullStmt) { TestNullStmt(); }


/**
 * @brief Test implementation for SQLColumns metadata API
 *
 * Implements specific test logic for table metadata discovery such as
 * column count, column name and data for post-processing
 */
class SQLColumnsTest : public MetadataResultSetTest<SHOWCOLUMNSResult> {
protected:
    SQLRETURN processResult(SQLHSTMT stmt, const std::vector<SHOWCOLUMNSResult>& rs) override {
        return RsMetadataAPIPostProcessor::sqlColumnsPostProcessing(stmt, rs);
    }

    int getExpectedColumnCount() override { return 23; }

    std::vector<std::pair<int, const char*>> getExpectedColumns() override {
        return {
            {kSQLColumns_TABLE_CAT_COL_NUM, "TABLE_CAT"},
            {kSQLColumns_TABLE_SCHEM_COL_NUM, "TABLE_SCHEM"},
            {kSQLColumns_TABLE_NAME_COL_NUM, "TABLE_NAME"},
            {kSQLColumns_COLUMN_NAME_COL_NUM, "COLUMN_NAME"},
            {kSQLColumns_DATA_TYPE_COL_NUM, "DATA_TYPE"},
            {kSQLColumns_TYPE_NAME_COL_NUM, "TYPE_NAME"},
            {kSQLColumns_COLUMN_SIZE_COL_NUM, "COLUMN_SIZE"},
            {kSQLColumns_BUFFER_LENGTH_COL_NUM, "BUFFER_LENGTH"},
            {kSQLColumns_DECIMAL_DIGITS_COL_NUM, "DECIMAL_DIGITS"},
            {kSQLColumns_NUM_PREC_RADIX_COL_NUM, "NUM_PREC_RADIX"},
            {kSQLColumns_NULLABLE_COL_NUM, "NULLABLE"},
            {kSQLColumns_REMARKS_COL_NUM, "REMARKS"},
            {kSQLColumns_COLUMN_DEF_COL_NUM, "COLUMN_DEF"},
            {kSQLColumns_SQL_DATA_TYPE_COL_NUM, "SQL_DATA_TYPE"},
            {kSQLColumns_SQL_DATETIME_SUB_COL_NUM, "SQL_DATETIME_SUB"},
            {kSQLColumns_CHAR_OCTET_LENGTH_COL_NUM, "CHAR_OCTET_LENGTH"},
            {kSQLColumns_ORDINAL_POSITION_COL_NUM, "ORDINAL_POSITION"},
            {kSQLColumns_IS_NULLABLE_COL_NUM, "IS_NULLABLE"},
            {kSQLColumns_SORT_KEY_TYPE_COL_NUM, "SORT_KEY_TYPE"},
            {kSQLColumns_SORT_KEY_COL_NUM, "SORT_KEY"},
            {kSQLColumns_DIST_KEY_COL_NUM, "DIST_KEY"},
            {kSQLColumns_ENCODING_COL_NUM, "ENCODING"},
            {kSQLColumns_COLLATION_COL_NUM, "COLLATION"}
        };
    }

    std::vector<SHOWCOLUMNSResult> createTestRecords(int count, bool maxLength) override {
        std::vector<SHOWCOLUMNSResult> results;

        for (int i = 1; i <= count; i++) {
            SHOWCOLUMNSResult result{};  // Zero-initialize
            std::string suffix = std::to_string(i);

            if (maxLength) {
                result.database_name = getMaxStr();
                result.schema_name = getMaxStr();
                result.table_name = getMaxStr();
                result.column_name = getMaxStr();
            } else {
                result.database_name = "catalog" + suffix;
                result.schema_name = "schema" + suffix;
                result.table_name = "table" + suffix;
                result.column_name = "column" + suffix;
            }
            result.ordinal_position = 1;
            result.column_default = "";
            result.is_nullable = "NO";
            result.data_type = "character varying";
            result.character_maximum_length = 256;
            result.numeric_precision = 0;
            result.numeric_scale = 0;
            result.remarks = "";

            results.push_back(result);
        }
        return results;
    }

    std::vector<SHOWCOLUMNSResult> createMixedCaseGlueDataTypeTestRecords() override {
        std::vector<SHOWCOLUMNSResult> results;

        for (size_t i = 0; i < mixedCaseDataType.size(); i++) {
            SHOWCOLUMNSResult result{};
            std::string suffix = std::to_string(i + 1);

            result.database_name = "catalog" + suffix;
            result.schema_name = "schema" + suffix;
            result.table_name = "table" + suffix;
            result.column_name = "column" + suffix;
            result.ordinal_position = 1;
            result.column_default = "";
            result.is_nullable = "NO";
            result.data_type = mixedCaseDataType[i];
            // "string" types use character_maximum_length; "binary" types do not
            result.character_maximum_length = isBinaryType(mixedCaseDataType[i]) ? 0 : 256;
            result.numeric_precision = 0;
            result.numeric_scale = 0;
            result.remarks = "";

            results.push_back(result);
        }
        return results;
    }

    void validateResults(const std::vector<SHOWCOLUMNSResult>& records, bool maxLength) override {
        RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
        PGresult *res = pStmt->pResultHead->pgResult;

        ASSERT_EQ(records.size(), static_cast<size_t>(PQntuples(res)))
            << "Result set row count mismatch";

        for (size_t i = 0; i < records.size(); i++) {
            std::string suffix = std::to_string(i + 1);
            if (maxLength) {
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_TABLE_CAT_COL_NUM), getMaxStr().c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_TABLE_SCHEM_COL_NUM), getMaxStr().c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_TABLE_NAME_COL_NUM), getMaxStr().c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_COLUMN_NAME_COL_NUM), getMaxStr().c_str());
            } else {
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_TABLE_CAT_COL_NUM), ("catalog" + suffix).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_TABLE_SCHEM_COL_NUM), ("schema" + suffix).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_TABLE_NAME_COL_NUM), ("table" + suffix).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_COLUMN_NAME_COL_NUM), ("column" + suffix).c_str());
            }
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_DATA_TYPE_COL_NUM), std::to_string(SQL_VARCHAR).c_str());
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_TYPE_NAME_COL_NUM), "varchar");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_COLUMN_SIZE_COL_NUM), "256");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_BUFFER_LENGTH_COL_NUM), "256");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_DECIMAL_DIGITS_COL_NUM), "");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_NUM_PREC_RADIX_COL_NUM), "0");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_NULLABLE_COL_NUM), "0");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_REMARKS_COL_NUM), "");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_COLUMN_DEF_COL_NUM), "");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_SQL_DATA_TYPE_COL_NUM),  std::to_string(SQL_VARCHAR).c_str());
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_SQL_DATETIME_SUB_COL_NUM), "");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_CHAR_OCTET_LENGTH_COL_NUM), "256");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_ORDINAL_POSITION_COL_NUM), "1");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_IS_NULLABLE_COL_NUM), "NO");
        }
    }

    void validateMixedCaseGlueDataTypeResults(const std::vector<SHOWCOLUMNSResult>& records) override {
        RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
        PGresult *res = pStmt->pResultHead->pgResult;

        ASSERT_EQ(records.size(), static_cast<size_t>(PQntuples(res)))
            << "Result set row count mismatch";

        ASSERT_EQ(records.size(), mixedCaseDataType.size());
        for (size_t i = 0; i < records.size(); i++) {
            std::string suffix = std::to_string(i + 1);
            bool isBinary = isBinaryType(mixedCaseDataType[i]);

            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_TABLE_CAT_COL_NUM), ("catalog" + suffix).c_str());
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_TABLE_SCHEM_COL_NUM), ("schema" + suffix).c_str());
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_TABLE_NAME_COL_NUM), ("table" + suffix).c_str());
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_COLUMN_NAME_COL_NUM), ("column" + suffix).c_str());

            // Type-specific assertions: "string" maps to SQL_VARCHAR, "binary" maps to SQL_LONGVARBINARY
            if (isBinary) {
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_DATA_TYPE_COL_NUM), std::to_string(SQL_LONGVARBINARY).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_TYPE_NAME_COL_NUM), "binary");
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_COLUMN_SIZE_COL_NUM), "");  // NULL (kNotApplicable)
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_BUFFER_LENGTH_COL_NUM), "");  // NULL (kNotApplicable)
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_SQL_DATA_TYPE_COL_NUM), std::to_string(SQL_LONGVARBINARY).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_NUM_PREC_RADIX_COL_NUM), "10");
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_CHAR_OCTET_LENGTH_COL_NUM), std::to_string(kUnknownColumnSize).c_str());
            } else {
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_DATA_TYPE_COL_NUM), std::to_string(SQL_VARCHAR).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_TYPE_NAME_COL_NUM), "string");
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_COLUMN_SIZE_COL_NUM), "256");
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_BUFFER_LENGTH_COL_NUM), "256");
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_SQL_DATA_TYPE_COL_NUM), std::to_string(SQL_VARCHAR).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_NUM_PREC_RADIX_COL_NUM), "0");
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_CHAR_OCTET_LENGTH_COL_NUM), "256");
            }

            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_DECIMAL_DIGITS_COL_NUM), "");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_NULLABLE_COL_NUM), "0");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_REMARKS_COL_NUM), "");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_COLUMN_DEF_COL_NUM), "");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_SQL_DATETIME_SUB_COL_NUM), "");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_ORDINAL_POSITION_COL_NUM), "1");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_IS_NULLABLE_COL_NUM), "NO");
        }
    }
};

TEST_F(SQLColumnsTest, EmptyResultSet) { TestEmptyResult(); }

TEST_F(SQLColumnsTest, PopulatedResultSet) { TestSingleResult(); }

TEST_F(SQLColumnsTest, MultipleRowResultSet) { TestMultipleResults(3); }

TEST_F(SQLColumnsTest, MaxLength) { TestMaxLength(); }

TEST_F(SQLColumnsTest, NullStmt) { TestNullStmt(); }

TEST_F(SQLColumnsTest, MixedCaseGlueDataType) { TestMixedCaseGlueDatatype(); }


/**
 * @brief Test implementation for SQLPrimaryKeys metadata API
 *
 * Implements specific test logic for primary key metadata discovery such as
 * column count, column name and data for post-processing
 */
class SQLPrimaryKeysTest : public MetadataResultSetTest<SHOWCONSTRAINTSPRIMARYKEYSResult> {
protected:
    SQLRETURN processResult(SQLHSTMT stmt, const std::vector<SHOWCONSTRAINTSPRIMARYKEYSResult>& rs) override {
        return RsMetadataAPIPostProcessor::sqlPrimaryKeysPostProcessing(stmt, rs);
    }

    int getExpectedColumnCount() override { return 6; }

    std::vector<std::pair<int, const char*>> getExpectedColumns() override {
        return {
            {kSQLPrimaryKeys_TABLE_CAT_COL_NUM, "TABLE_CAT"},
            {kSQLPrimaryKeys_TABLE_SCHEM_COL_NUM, "TABLE_SCHEM"},
            {kSQLPrimaryKeys_TABLE_NAME_COL_NUM, "TABLE_NAME"},
            {kSQLPrimaryKeys_COLUMN_NAME_COL_NUM, "COLUMN_NAME"},
            {kSQLPrimaryKeys_KEY_SEQ_COL_NUM, "KEY_SEQ"},
            {kSQLPrimaryKeys_PK_NAME_COL_NUM, "PK_NAME"}
        };
    }

    std::vector<SHOWCONSTRAINTSPRIMARYKEYSResult> createTestRecords(int count, bool maxLength) override {
        std::vector<SHOWCONSTRAINTSPRIMARYKEYSResult> results;

        for (int i = 1; i <= count; i++) {
            SHOWCONSTRAINTSPRIMARYKEYSResult result{};  // Zero-initialize
            std::string suffix = std::to_string(i);

            if (maxLength) {
                result.database_name = getMaxStr();
                result.schema_name = getMaxStr();
                result.table_name = getMaxStr();
                result.column_name = getMaxStr();
            } else {
                result.database_name = "catalog" + suffix;
                result.schema_name = "schema" + suffix;
                result.table_name = "table" + suffix;
                result.column_name = "col" + suffix;
            }
            result.key_seq = i;
            result.pk_name = "pk" + suffix;
            results.push_back(result);
        }
        return results;
    }

    void validateResults(const std::vector<SHOWCONSTRAINTSPRIMARYKEYSResult>& records, bool maxLength) override {
        RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
        PGresult *res = pStmt->pResultHead->pgResult;

        ASSERT_EQ(records.size(), static_cast<size_t>(PQntuples(res)))
            << "Result set row count mismatch";

        for (size_t i = 0; i < records.size(); i++) {
            std::string suffix = std::to_string(i + 1);

            if (maxLength) {
                EXPECT_STREQ(PQgetvalue(res, i, kSQLPrimaryKeys_TABLE_CAT_COL_NUM), getMaxStr().c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLPrimaryKeys_TABLE_SCHEM_COL_NUM), getMaxStr().c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLPrimaryKeys_TABLE_NAME_COL_NUM), getMaxStr().c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLPrimaryKeys_COLUMN_NAME_COL_NUM), getMaxStr().c_str());
            } else {
                EXPECT_STREQ(PQgetvalue(res, i, kSQLPrimaryKeys_TABLE_CAT_COL_NUM), ("catalog" + suffix).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLPrimaryKeys_TABLE_SCHEM_COL_NUM), ("schema" + suffix).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLPrimaryKeys_TABLE_NAME_COL_NUM), ("table" + suffix).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLPrimaryKeys_COLUMN_NAME_COL_NUM), ("col" + suffix).c_str());
            }
            EXPECT_STREQ(PQgetvalue(res, i, kSQLPrimaryKeys_KEY_SEQ_COL_NUM), suffix.c_str());
            EXPECT_STREQ(PQgetvalue(res, i, kSQLPrimaryKeys_PK_NAME_COL_NUM), ("pk" + suffix).c_str());
        }
    }
};

TEST_F(SQLPrimaryKeysTest, EmptyResultSet) { TestEmptyResult(); }

TEST_F(SQLPrimaryKeysTest, PopulatedResultSet) { TestSingleResult(); }

TEST_F(SQLPrimaryKeysTest, MultipleRowResultSet) { TestMultipleResults(3); }

TEST_F(SQLPrimaryKeysTest, MaxLength) { TestMaxLength(); }

TEST_F(SQLPrimaryKeysTest, NullStmt) { TestNullStmt(); }


/**
 * @brief Test implementation for SQLForeignKeys metadata API
 *
 * Implements specific test logic for primary key metadata discovery such as
 * column count, column name and data for post-processing
 */
class SQLForeignKeysTest : public MetadataResultSetTest<SHOWCONSTRAINTSFOREIGNKEYSResult> {
protected:
    SQLRETURN processResult(SQLHSTMT stmt, const std::vector<SHOWCONSTRAINTSFOREIGNKEYSResult>& rs) override {
        return RsMetadataAPIPostProcessor::sqlForeignKeysPostProcessing(stmt, rs);
    }

    int getExpectedColumnCount() override { return 14; }

    std::vector<std::pair<int, const char*>> getExpectedColumns() override {
        return {
            {kSQLForeignKeys_PKTABLE_CAT_COL_NUM, "PKTABLE_CAT"},
            {kSQLForeignKeys_PKTABLE_SCHEM_COL_NUM, "PKTABLE_SCHEM"},
            {kSQLForeignKeys_PKTABLE_NAME_COL_NUM, "PKTABLE_NAME"},
            {kSQLForeignKeys_PKCOLUMN_NAME_COL_NUM, "PKCOLUMN_NAME"},
            {kSQLForeignKeys_FKTABLE_CAT_COL_NUM, "FKTABLE_CAT"},
            {kSQLForeignKeys_FKTABLE_SCHEM_COL_NUM, "FKTABLE_SCHEM"},
            {kSQLForeignKeys_FKTABLE_NAME_COL_NUM, "FKTABLE_NAME"},
            {kSQLForeignKeys_FKCOLUMN_NAME_COL_NUM, "FKCOLUMN_NAME"},
            {kSQLForeignKeys_KEY_SEQ_COL_NUM, "KEY_SEQ"},
            {kSQLForeignKeys_UPDATE_RULE_COL_NUM, "UPDATE_RULE"},
            {kSQLForeignKeys_DELETE_RULE_COL_NUM, "DELETE_RULE"},
            {kSQLForeignKeys_FK_NAME_COL_NUM, "FK_NAME"},
            {kSQLForeignKeys_PK_NAME_COL_NUM, "PK_NAME"},
            {kSQLForeignKeys_DEFERRABILITY_COL_NUM, "DEFERRABILITY"}
        };
    }

    std::vector<SHOWCONSTRAINTSFOREIGNKEYSResult> createTestRecords(int count, bool maxLength) override {
        std::vector<SHOWCONSTRAINTSFOREIGNKEYSResult> results;

        for (int i = 1; i <= count; i++) {
            SHOWCONSTRAINTSFOREIGNKEYSResult result{};  // Zero-initialize
            std::string suffix = std::to_string(i);

            if (maxLength) {
                result.pk_table_cat = getMaxStr();
                result.pk_table_schem = getMaxStr();
                result.pk_table_name = getMaxStr();
                result.pk_column_name = getMaxStr();
                result.fk_table_cat = getMaxStr();
                result.fk_table_schem = getMaxStr();
                result.fk_table_name = getMaxStr();
                result.fk_column_name = getMaxStr();
            } else {
                result.pk_table_cat = "test_catalog_pk"+ suffix;
                result.pk_table_schem = "test_schema_pk"+ suffix;
                result.pk_table_name = "test_table_pk"+ suffix;
                result.pk_column_name = "test_column_pk"+ suffix;
                result.fk_table_cat = "test_catalog_fk"+ suffix;
                result.fk_table_schem = "test_schema_fk"+ suffix;
                result.fk_table_name = "test_table_fk"+ suffix;
                result.fk_column_name = "test_column_fk"+ suffix;
            }
            result.key_seq = 1;
            result.update_rule = 1;
            result.delete_rule = 1;
            result.pk_name = "test_pk"+ suffix;
            result.fk_name = "test_fk"+ suffix;
            result.deferrability = 1;

            results.push_back(result);
        }
        return results;
    }

    void validateResults(const std::vector<SHOWCONSTRAINTSFOREIGNKEYSResult>& records, bool maxLength) override {
        RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
        PGresult *res = pStmt->pResultHead->pgResult;

        ASSERT_EQ(records.size(), static_cast<size_t>(PQntuples(res)))
            << "Result set row count mismatch";

        for (size_t i = 0; i < records.size(); i++) {
            std::string suffix = std::to_string(i + 1);

            if (maxLength) {
                EXPECT_STREQ(PQgetvalue(res, i, kSQLForeignKeys_PKTABLE_CAT_COL_NUM), getMaxStr().c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLForeignKeys_PKTABLE_SCHEM_COL_NUM), getMaxStr().c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLForeignKeys_PKTABLE_NAME_COL_NUM), getMaxStr().c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLForeignKeys_PKCOLUMN_NAME_COL_NUM), getMaxStr().c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLForeignKeys_FKTABLE_CAT_COL_NUM), getMaxStr().c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLForeignKeys_FKTABLE_SCHEM_COL_NUM), getMaxStr().c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLForeignKeys_FKTABLE_NAME_COL_NUM), getMaxStr().c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLForeignKeys_FKCOLUMN_NAME_COL_NUM), getMaxStr().c_str());
            } else {
                EXPECT_STREQ(PQgetvalue(res, i, kSQLForeignKeys_PKTABLE_CAT_COL_NUM), ("test_catalog_pk"+ suffix).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLForeignKeys_PKTABLE_SCHEM_COL_NUM), ("test_schema_pk"+ suffix).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLForeignKeys_PKTABLE_NAME_COL_NUM), ("test_table_pk"+ suffix).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLForeignKeys_PKCOLUMN_NAME_COL_NUM), ("test_column_pk"+ suffix).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLForeignKeys_FKTABLE_CAT_COL_NUM), ("test_catalog_fk"+ suffix).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLForeignKeys_FKTABLE_SCHEM_COL_NUM), ("test_schema_fk"+ suffix).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLForeignKeys_FKTABLE_NAME_COL_NUM), ("test_table_fk"+ suffix).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLForeignKeys_FKCOLUMN_NAME_COL_NUM), ("test_column_fk"+ suffix).c_str());
            }
            EXPECT_STREQ(PQgetvalue(res, i, kSQLForeignKeys_KEY_SEQ_COL_NUM), "1");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLForeignKeys_UPDATE_RULE_COL_NUM), "1");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLForeignKeys_DELETE_RULE_COL_NUM), "1");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLForeignKeys_FK_NAME_COL_NUM), ("test_fk"+ suffix).c_str());
            EXPECT_STREQ(PQgetvalue(res, i, kSQLForeignKeys_PK_NAME_COL_NUM), ("test_pk"+ suffix).c_str());
            EXPECT_STREQ(PQgetvalue(res, i, kSQLForeignKeys_DEFERRABILITY_COL_NUM), "1");
        }
    }
};

TEST_F(SQLForeignKeysTest, EmptyResultSet) { TestEmptyResult(); }

TEST_F(SQLForeignKeysTest, PopulatedResultSet) { TestSingleResult(); }

TEST_F(SQLForeignKeysTest, MultipleRowResultSet) { TestMultipleResults(3); }

TEST_F(SQLForeignKeysTest, MaxLength) { TestMaxLength(); }

TEST_F(SQLForeignKeysTest, NullStmt) { TestNullStmt(); }


/**
 * @brief Test implementation for SQLSpecialColumns metadata API
 *
 * Implements specific test logic for primary key metadata discovery such as
 * column count, column name and data for post-processing
 */
class SQLSpecialColumnsTest : public MetadataResultSetTest<SHOWCOLUMNSResult> {
protected:
    SQLRETURN processResult(SQLHSTMT stmt, const std::vector<SHOWCOLUMNSResult>& rs) override {
        return RsMetadataAPIPostProcessor::sqlSpecialColumnsPostProcessing(stmt, rs, SQL_BEST_ROWID);
    }

    int getExpectedColumnCount() override { return 8; }

    std::vector<std::pair<int, const char*>> getExpectedColumns() override {
        return {
            {kSQLSpecialColumns_SCOPE, "SCOPE"},
            {kSQLSpecialColumns_COLUMN_NAME, "COLUMN_NAME"},
            {kSQLSpecialColumns_DATA_TYPE, "DATA_TYPE"},
            {kSQLSpecialColumns_TYPE_NAME, "TYPE_NAME"},
            {kSQLSpecialColumns_COLUMN_SIZE, "COLUMN_SIZE"},
            {kSQLSpecialColumns_BUFFER_LENGTH, "BUFFER_LENGTH"},
            {kSQLSpecialColumns_DECIMAL_DIGITS, "DECIMAL_DIGITS"},
            {kSQLSpecialColumns_PSEUDO_COLUMN, "PSEUDO_COLUMN"}
        };
    }

    std::vector<SHOWCOLUMNSResult> createTestRecords(int count, bool maxLength) override {
        std::vector<SHOWCOLUMNSResult> results;

        for (int i = 1; i <= count; i++) {
            SHOWCOLUMNSResult result{};  // Zero-initialize
            std::string suffix = std::to_string(i);

            if (maxLength) {
                result.database_name = getMaxStr();
                result.schema_name = getMaxStr();
                result.table_name = getMaxStr();
                result.column_name = getMaxStr();
            } else {
                result.database_name = "catalog" + suffix;
                result.schema_name = "schema" + suffix;
                result.table_name = "table" + suffix;
                result.column_name = "column" + suffix;
            }
            result.ordinal_position = 1;
            result.column_default = "";
            result.is_nullable = "NO";
            result.data_type = "character varying";
            result.character_maximum_length = 256;
            result.numeric_precision = 0;
            result.numeric_scale = 0;
            result.remarks = "";

            results.push_back(result);
        }
        return results;
    }

    void validateResults(const std::vector<SHOWCOLUMNSResult>& records, bool maxLength) override {
        RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
        PGresult *res = pStmt->pResultHead->pgResult;

        ASSERT_EQ(records.size(), static_cast<size_t>(PQntuples(res)))
            << "Result set row count mismatch";

        for (size_t i = 0; i < records.size(); i++) {
            std::string suffix = std::to_string(i + 1);

            EXPECT_STREQ(PQgetvalue(res, i, kSQLSpecialColumns_SCOPE), "2");
            if (maxLength) {
                EXPECT_STREQ(PQgetvalue(res, i, kSQLSpecialColumns_COLUMN_NAME), getMaxStr().c_str());
            } else {
                EXPECT_STREQ(PQgetvalue(res, i, kSQLSpecialColumns_COLUMN_NAME), ("column"+ suffix).c_str());
            }
            EXPECT_STREQ(PQgetvalue(res, i, kSQLSpecialColumns_DATA_TYPE), std::to_string(SQL_VARCHAR).c_str());
            EXPECT_STREQ(PQgetvalue(res, i, kSQLSpecialColumns_TYPE_NAME), "varchar");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLSpecialColumns_COLUMN_SIZE), "256");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLSpecialColumns_BUFFER_LENGTH), "256");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLSpecialColumns_DECIMAL_DIGITS), "");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLSpecialColumns_PSEUDO_COLUMN), std::to_string(SQL_PC_NOT_PSEUDO).c_str());
        }
    }
};

TEST_F(SQLSpecialColumnsTest, EmptyResultSet) { TestEmptyResult(); }

TEST_F(SQLSpecialColumnsTest, PopulatedResultSet) { TestSingleResult(); }

TEST_F(SQLSpecialColumnsTest, MultipleRowResultSet) { TestMultipleResults(3); }

TEST_F(SQLSpecialColumnsTest, MaxLength) { TestMaxLength(); }

TEST_F(SQLSpecialColumnsTest, NullStmt) { TestNullStmt(); }

TEST_F(SQLSpecialColumnsTest, SpecialColumnsRowVer) {
    ASSERT_TRUE(stmt != SQL_NULL_HANDLE);
    
    std::vector<SHOWCOLUMNSResult> intermediateRS;
    SHOWCOLUMNSResult result;
    
    // Set up test data for a timestamp column
    result.column_name = "last_updated";
    result.data_type = "timestamp";
    result.numeric_precision = 0;
    result.numeric_scale = 6;  // microseconds
    result.character_maximum_length = 0;
    
    intermediateRS.push_back(result);
    
    SQLRETURN rc = RsMetadataAPIPostProcessor::sqlSpecialColumnsPostProcessing(
        stmt, intermediateRS, SQL_ROWVER);
    
    EXPECT_EQ(rc, SQL_SUCCESS);
    
    // Verify result set
    RS_STMT_INFO* pStmt = (RS_STMT_INFO*)stmt;
    ASSERT_TRUE(pStmt->pResultHead != nullptr);
    EXPECT_EQ(pStmt->pResultHead->iNumberOfRowsInMem, 1);
    
    PGresult* res = pStmt->pResultHead->pgResult;
    ASSERT_TRUE(res != nullptr);
    
    // Verify PSEUDO_COLUMN is SQL_PC_NOT_PSEUDO
    int pseudo_column;
    sscanf(PQgetvalue(res, 0, kSQLSpecialColumns_PSEUDO_COLUMN), "%d", &pseudo_column);
    EXPECT_EQ(pseudo_column, 2);  // Should be 2 for SQL_ROWVER
    
    // Verify DATA_TYPE is SQL_TYPE_TIMESTAMP
    int data_type;
    sscanf(PQgetvalue(res, 0, kSQLSpecialColumns_DATA_TYPE), "%d", &data_type);
    EXPECT_EQ(data_type, SQL_TYPE_TIMESTAMP);
    
    // Verify DECIMAL_DIGITS (scale)
    int decimal_digits;
    sscanf(PQgetvalue(res, 0, kSQLSpecialColumns_DECIMAL_DIGITS), "%d", &decimal_digits);
    EXPECT_EQ(decimal_digits, 6);

    // Verify SCOPE
    int scope;
    sscanf(PQgetvalue(res, 0, kSQLSpecialColumns_SCOPE), "%d", &scope);
    EXPECT_EQ(scope, 0);  // Should be 0 for SQL_ROWVER
}


/**
 * @brief Test implementation for SQLColumnPrivileges metadata API
 * 
 * Implements specific test logic for primary key metadata discovery such as
 * column count, column name and data for post-processing
 */
class SQLColumnPrivilegesTest : public MetadataResultSetTest<SHOWGRANTSCOLUMNResult> {
protected:
    SQLRETURN processResult(SQLHSTMT stmt, const std::vector<SHOWGRANTSCOLUMNResult>& rs) override {
        return RsMetadataAPIPostProcessor::sqlColumnPrivilegesPostProcessing(stmt, rs);
    }

    int getExpectedColumnCount() override { return 8; }

    std::vector<std::pair<int, const char*>> getExpectedColumns() override {
        return {
            {kSQLColumnPrivileges_TABLE_CAT_COL_NUM, "TABLE_CAT"},
            {kSQLColumnPrivileges_TABLE_SCHEM_COL_NUM, "TABLE_SCHEM"},
            {kSQLColumnPrivileges_TABLE_NAME_COL_NUM, "TABLE_NAME"},
            {kSQLColumnPrivileges_COLUMN_NAME_COL_NUM, "COLUMN_NAME"},
            {kSQLColumnPrivileges_GRANTOR_COL_NUM, "GRANTOR"},
            {kSQLColumnPrivileges_GRANTEE_COL_NUM, "GRANTEE"},
            {kSQLColumnPrivileges_PRIVILEGE_COL_NUM, "PRIVILEGE"},
            {kSQLColumnPrivileges_IS_GRANTABLE_COL_NUM, "IS_GRANTABLE"}
        };
    }

    std::vector<SHOWGRANTSCOLUMNResult> createTestRecords(int count, bool maxLength) override {
        std::vector<SHOWGRANTSCOLUMNResult> results;

        for (int i = 1; i <= count; i++) {
            SHOWGRANTSCOLUMNResult result{};  // Zero-initialize
            std::string suffix = std::to_string(i);

            if (maxLength) {
                result.table_cat = getMaxStr();
                result.table_schem = getMaxStr();
                result.table_name = getMaxStr();
                result.column_name = getMaxStr();
            } else {
                result.table_cat = "catalog" + suffix;
                result.table_schem = "schema" + suffix;
                result.table_name = "table" + suffix;
                result.column_name = "column" + suffix;
            }
            result.grantor = "admin";
            result.grantee = "user1";
            result.privilege = "SELECT";
            result.admin_option = 1;
            results.push_back(result);
        }
        return results;
    }

    void validateResults(const std::vector<SHOWGRANTSCOLUMNResult>& records, bool maxLength) override {
        RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
        PGresult *res = pStmt->pResultHead->pgResult;

        ASSERT_EQ(records.size(), static_cast<size_t>(PQntuples(res)))
            << "Result set row count mismatch";

        for (size_t i = 0; i < records.size(); i++) {
            std::string suffix = std::to_string(i + 1);

            if (maxLength) {
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumnPrivileges_TABLE_CAT_COL_NUM), getMaxStr().c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumnPrivileges_TABLE_SCHEM_COL_NUM), getMaxStr().c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumnPrivileges_TABLE_NAME_COL_NUM), getMaxStr().c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumnPrivileges_COLUMN_NAME_COL_NUM), getMaxStr().c_str());
            } else {
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumnPrivileges_TABLE_CAT_COL_NUM), ("catalog" + suffix).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumnPrivileges_TABLE_SCHEM_COL_NUM), ("schema" + suffix).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumnPrivileges_TABLE_NAME_COL_NUM), ("table" + suffix).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLColumnPrivileges_COLUMN_NAME_COL_NUM), ("column" + suffix).c_str());
            }
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumnPrivileges_GRANTOR_COL_NUM), "admin");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumnPrivileges_GRANTEE_COL_NUM), "user1");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumnPrivileges_PRIVILEGE_COL_NUM), "SELECT");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumnPrivileges_IS_GRANTABLE_COL_NUM), "YES");
        }
    }
};

TEST_F(SQLColumnPrivilegesTest, EmptyResultSet) { TestEmptyResult(); }

TEST_F(SQLColumnPrivilegesTest, PopulatedResultSet) { TestSingleResult(); }

TEST_F(SQLColumnPrivilegesTest, MultipleRowResultSet) { TestMultipleResults(3); }

TEST_F(SQLColumnPrivilegesTest, MaxLength) { TestMaxLength(); }

TEST_F(SQLColumnPrivilegesTest, NullStmt) { TestNullStmt(); }


/**
 * @brief Test implementation for SQLTablePrivileges metadata API
 * 
 * Implements specific test logic for primary key metadata discovery such as
 * column count, column name and data for post-processing
 */
class SQLTablePrivilegesTest : public MetadataResultSetTest<SHOWGRANTSTABLEResult> {
protected:
    SQLRETURN processResult(SQLHSTMT stmt, const std::vector<SHOWGRANTSTABLEResult>& rs) override {
        return RsMetadataAPIPostProcessor::sqlTablePrivilegesPostProcessing(stmt, rs);
    }

    int getExpectedColumnCount() override { return 7; }

    std::vector<std::pair<int, const char*>> getExpectedColumns() override {
        return {
            {kSQLTablePrivileges_TABLE_CAT_COL_NUM, "TABLE_CAT"},
            {kSQLTablePrivileges_TABLE_SCHEM_COL_NUM, "TABLE_SCHEM"},
            {kSQLTablePrivileges_TABLE_NAME_COL_NUM, "TABLE_NAME"},
            {kSQLTablePrivileges_GRANTOR_COL_NUM, "GRANTOR"},
            {kSQLTablePrivileges_GRANTEE_COL_NUM, "GRANTEE"},
            {kSQLTablePrivileges_PRIVILEGE_COL_NUM, "PRIVILEGE"},
            {kSQLTablePrivileges_IS_GRANTABLE_COL_NUM, "IS_GRANTABLE"}
        };
    }

    std::vector<SHOWGRANTSTABLEResult> createTestRecords(int count, bool maxLength) override {
        std::vector<SHOWGRANTSTABLEResult> results;

        for (int i = 1; i <= count; i++) {
            SHOWGRANTSTABLEResult result{};  // Zero-initialize
            std::string suffix = std::to_string(i);

            if (maxLength) {
                result.table_cat = getMaxStr();
                result.table_schem = getMaxStr();
                result.table_name = getMaxStr();
            } else {
                result.table_cat = "catalog" + suffix;
                result.table_schem = "schema" + suffix;
                result.table_name = "table" + suffix;
            }
            result.grantor = "admin";
            result.grantee = "user1";
            result.privilege = "SELECT";
            result.admin_option = 1;
            results.push_back(result);
        }
        return results;
    }

    void validateResults(const std::vector<SHOWGRANTSTABLEResult>& records, bool maxLength) override {
        RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
        PGresult *res = pStmt->pResultHead->pgResult;

        ASSERT_EQ(records.size(), static_cast<size_t>(PQntuples(res)))
            << "Result set row count mismatch";

        for (size_t i = 0; i < records.size(); i++) {
            std::string suffix = std::to_string(i + 1);

            if (maxLength) {
                EXPECT_STREQ(PQgetvalue(res, i, kSQLTablePrivileges_TABLE_CAT_COL_NUM), getMaxStr().c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLTablePrivileges_TABLE_SCHEM_COL_NUM), getMaxStr().c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLTablePrivileges_TABLE_NAME_COL_NUM), getMaxStr().c_str());
            } else {
                EXPECT_STREQ(PQgetvalue(res, i, kSQLTablePrivileges_TABLE_CAT_COL_NUM), ("catalog" + suffix).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLTablePrivileges_TABLE_SCHEM_COL_NUM), ("schema" + suffix).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLTablePrivileges_TABLE_NAME_COL_NUM), ("table" + suffix).c_str());
            }
            EXPECT_STREQ(PQgetvalue(res, i, kSQLTablePrivileges_GRANTOR_COL_NUM), "admin");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLTablePrivileges_GRANTEE_COL_NUM), "user1");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLTablePrivileges_PRIVILEGE_COL_NUM), "SELECT");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLTablePrivileges_IS_GRANTABLE_COL_NUM), "YES");
        }
    }
};

TEST_F(SQLTablePrivilegesTest, EmptyResultSet) { TestEmptyResult(); }

TEST_F(SQLTablePrivilegesTest, PopulatedResultSet) { TestSingleResult(); }

TEST_F(SQLTablePrivilegesTest, MultipleRowResultSet) { TestMultipleResults(3); }

TEST_F(SQLTablePrivilegesTest, MaxLength) { TestMaxLength(); }

TEST_F(SQLTablePrivilegesTest, NullStmt) { TestNullStmt(); }


/**
 * @brief Test implementation for SQLProcedures metadata API
 * 
 * Implements specific test logic for primary key metadata discovery such as
 * column count, column name and data for post-processing
 */
class SQLProceduresTest : public MetadataResultSetTest<SHOWPROCEDURESFUNCTIONSResult> {
protected:
    SQLRETURN processResult(SQLHSTMT stmt, const std::vector<SHOWPROCEDURESFUNCTIONSResult>& rs) override {
        return RsMetadataAPIPostProcessor::sqlProceduresPostProcessing(stmt, rs);
    }

    int getExpectedColumnCount() override { return 8; }

    std::vector<std::pair<int, const char*>> getExpectedColumns() override {
        return {
            {kSQLProcedures_PROCEDURE_CAT_COL_NUM, "PROCEDURE_CAT"},
            {kSQLProcedures_PROCEDURE_SCHEM_COL_NUM, "PROCEDURE_SCHEM"},
            {kSQLProcedures_PROCEDURE_NAME_COL_NUM, "PROCEDURE_NAME"},
            {kSQLProcedures_NUM_INPUT_PARAMS_COL_NUM, "NUM_INPUT_PARAMS"},
            {kSQLProcedures_NUM_OUTPUT_PARAMS_COL_NUM, "NUM_OUTPUT_PARAMS"},
            {kSQLProcedures_NUM_RESULT_SETS_COL_NUM, "NUM_RESULT_SETS"},
            {kSQLProcedures_REMARKS_COL_NUM, "REMARKS"},
            {kSQLProcedures_PROCEDURE_TYPE_COL_NUM, "PROCEDURE_TYPE"}
        };
    }

    std::vector<SHOWPROCEDURESFUNCTIONSResult> createTestRecords(int count, bool maxLength) override {
        std::vector<SHOWPROCEDURESFUNCTIONSResult> results;

        for (int i = 1; i <= count; i++) {
            SHOWPROCEDURESFUNCTIONSResult result{};  // Zero-initialize
            std::string suffix = std::to_string(i);

            if (maxLength) {
                result.object_cat = getMaxStr();
                result.object_schem = getMaxStr();
                result.object_name = getMaxStr();
            } else {
                result.object_cat = "catalog" + suffix;
                result.object_schem = "schema" + suffix;
                result.object_name = "procedure_function" + suffix;
            }
            result.object_type = SQL_PT_PROCEDURE;
            results.push_back(result);
        }
        return results;
    }

    void validateResults(const std::vector<SHOWPROCEDURESFUNCTIONSResult>& records, bool maxLength) override {
        RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
        PGresult *res = pStmt->pResultHead->pgResult;

        ASSERT_EQ(records.size(), static_cast<size_t>(PQntuples(res)))
            << "Result set row count mismatch";

        for (size_t i = 0; i < records.size(); i++) {
            std::string suffix = std::to_string(i + 1);

            if (maxLength) {
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedures_PROCEDURE_CAT_COL_NUM), getMaxStr().c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedures_PROCEDURE_SCHEM_COL_NUM), getMaxStr().c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedures_PROCEDURE_NAME_COL_NUM), getMaxStr().c_str());
            } else {
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedures_PROCEDURE_CAT_COL_NUM), ("catalog" + suffix).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedures_PROCEDURE_SCHEM_COL_NUM), ("schema" + suffix).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedures_PROCEDURE_NAME_COL_NUM), ("procedure_function" + suffix).c_str());
            }
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedures_NUM_INPUT_PARAMS_COL_NUM), "");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedures_NUM_OUTPUT_PARAMS_COL_NUM), "");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedures_NUM_RESULT_SETS_COL_NUM), "");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedures_REMARKS_COL_NUM), "");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedures_PROCEDURE_TYPE_COL_NUM), std::to_string(SQL_PT_PROCEDURE).c_str());
        }
    }
};

TEST_F(SQLProceduresTest, EmptyResultSet) { TestEmptyResult(); }

TEST_F(SQLProceduresTest, PopulatedResultSet) { TestSingleResult(); }

TEST_F(SQLProceduresTest, MultipleRowResultSet) { TestMultipleResults(3); }

TEST_F(SQLProceduresTest, MaxLength) { TestMaxLength(); }

TEST_F(SQLProceduresTest, NullStmt) { TestNullStmt(); }


/**
 * @brief Test implementation for SQLProcedureColumns metadata API
 * 
 * Implements specific test logic for table metadata discovery such as
 * column count, column name and data for post-processing
 */
class SQLProcedureColumnsTest : public MetadataResultSetTest<SHOWCOLUMNSResult> {
protected:
    SQLRETURN processResult(SQLHSTMT stmt, const std::vector<SHOWCOLUMNSResult>& rs) override {
        return RsMetadataAPIPostProcessor::sqlProcedureColumnsPostProcessing(stmt, rs);
    }

    int getExpectedColumnCount() override { return 19; }

    std::vector<std::pair<int, const char*>> getExpectedColumns() override {
        return {
            {kSQLProcedureColumns_PROCEDURE_CAT_COL_NUM, "PROCEDURE_CAT"},
            {kSQLProcedureColumns_PROCEDURE_SCHEM_COL_NUM, "PROCEDURE_SCHEM"},
            {kSQLProcedureColumns_PROCEDURE_NAME_COL_NUM, "PROCEDURE_NAME"},
            {kSQLProcedureColumns_COLUMN_NAME_COL_NUM, "COLUMN_NAME"},
            {kSQLProcedureColumns_COLUMN_TYPE_COL_NUM, "COLUMN_TYPE"},
            {kSQLProcedureColumns_DATA_TYPE_COL_NUM, "DATA_TYPE"},
            {kSQLProcedureColumns_TYPE_NAME_COL_NUM, "TYPE_NAME"},
            {kSQLProcedureColumns_COLUMN_SIZE_COL_NUM, "COLUMN_SIZE"},
            {kSQLProcedureColumns_BUFFER_LENGTH_COL_NUM, "BUFFER_LENGTH"},
            {kSQLProcedureColumns_DECIMAL_DIGITS_COL_NUM, "DECIMAL_DIGITS"},
            {kSQLProcedureColumns_NUM_PREC_RADIX_COL_NUM, "NUM_PREC_RADIX"},
            {kSQLProcedureColumns_NULLABLE_COL_NUM, "NULLABLE"},
            {kSQLProcedureColumns_REMARKS_COL_NUM, "REMARKS"},
            {kSQLProcedureColumns_COLUMN_DEF_COL_NUM, "COLUMN_DEF"},
            {kSQLProcedureColumns_SQL_DATA_TYPE_COL_NUM, "SQL_DATA_TYPE"},
            {kSQLProcedureColumns_SQL_DATETIME_SUB_COL_NUM, "SQL_DATETIME_SUB"},
            {kSQLProcedureColumns_CHAR_OCTET_LENGTH_COL_NUM, "CHAR_OCTET_LENGTH"},
            {kSQLProcedureColumns_ORDINAL_POSITION_COL_NUM, "ORDINAL_POSITION"},
            {kSQLProcedureColumns_IS_NULLABLE_COL_NUM, "IS_NULLABLE"}
        };
    }

    std::vector<SHOWCOLUMNSResult> createTestRecords(int count, bool maxLength) override {
        std::vector<SHOWCOLUMNSResult> results;

        for (int i = 1; i <= count; i++) {
            SHOWCOLUMNSResult result{};  // Zero-initialize
            std::string suffix = std::to_string(i);

            if (maxLength) {
                result.database_name = getMaxStr();
                result.schema_name = getMaxStr();
                result.procedure_function_name = getMaxStr();
                result.column_name = getMaxStr();
            } else {
                result.database_name = "catalog" + suffix;
                result.schema_name = "schema" + suffix;
                result.procedure_function_name = "procedure_function" + suffix;
                result.column_name = "column" + suffix;
            }
            result.parameter_type = "IN";
            result.ordinal_position = 1;
            result.column_default = "";
            result.is_nullable = "NO";
            result.data_type = "character varying";
            result.character_maximum_length = 256;
            result.numeric_precision = 0;
            result.numeric_scale = 0;
            result.remarks = "";
            
            results.push_back(result);
        }
        return results;
    }

    std::vector<SHOWCOLUMNSResult> createMixedCaseGlueDataTypeTestRecords() override {
        std::vector<SHOWCOLUMNSResult> results;

        for (size_t i = 0; i < mixedCaseDataType.size(); i++) {
            SHOWCOLUMNSResult result{};  // Zero-initialize
            std::string suffix = std::to_string(i+1);

            result.database_name = "catalog" + suffix;
            result.schema_name = "schema" + suffix;
            result.procedure_function_name = "procedure_function" + suffix;
            result.column_name = "column" + suffix;
            result.parameter_type = "IN";
            result.ordinal_position = 1;
            result.column_default = "";
            result.is_nullable = "NO";
            result.data_type = mixedCaseDataType[i];
            // "string" types use character_maximum_length; "binary" types do not
            result.character_maximum_length = isBinaryType(mixedCaseDataType[i]) ? 0 : 256;
            result.numeric_precision = 0;
            result.numeric_scale = 0;
            result.remarks = "";
            results.push_back(result);
        }
        return results;
    }

    void validateResults(const std::vector<SHOWCOLUMNSResult>& records, bool maxLength) override {
        RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
        PGresult *res = pStmt->pResultHead->pgResult;

        ASSERT_EQ(records.size(), static_cast<size_t>(PQntuples(res)))
            << "Result set row count mismatch";

        for (size_t i = 0; i < records.size(); i++) {
            std::string suffix = std::to_string(i + 1);
            if (maxLength) {
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_PROCEDURE_CAT_COL_NUM), getMaxStr().c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_PROCEDURE_SCHEM_COL_NUM), getMaxStr().c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_PROCEDURE_NAME_COL_NUM), getMaxStr().c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_COLUMN_NAME_COL_NUM), getMaxStr().c_str());
            } else {
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_PROCEDURE_CAT_COL_NUM), ("catalog" + suffix).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_PROCEDURE_SCHEM_COL_NUM), ("schema" + suffix).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_PROCEDURE_NAME_COL_NUM), ("procedure_function" + suffix).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_COLUMN_NAME_COL_NUM), ("column" + suffix).c_str());
            }
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_DATA_TYPE_COL_NUM), std::to_string(SQL_VARCHAR).c_str());
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_TYPE_NAME_COL_NUM), "varchar");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_COLUMN_SIZE_COL_NUM), "256");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_BUFFER_LENGTH_COL_NUM), "256");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_DECIMAL_DIGITS_COL_NUM), "");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_NUM_PREC_RADIX_COL_NUM), "0");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_NULLABLE_COL_NUM), "2");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_REMARKS_COL_NUM), "");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_COLUMN_DEF_COL_NUM), "");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_SQL_DATA_TYPE_COL_NUM),  std::to_string(SQL_VARCHAR).c_str());
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_SQL_DATETIME_SUB_COL_NUM), "");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_CHAR_OCTET_LENGTH_COL_NUM), "256");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_ORDINAL_POSITION_COL_NUM), "1");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_IS_NULLABLE_COL_NUM), "");
        }
    }

    void validateMixedCaseGlueDataTypeResults(const std::vector<SHOWCOLUMNSResult>& records) override {
        RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
        PGresult *res = pStmt->pResultHead->pgResult;

        ASSERT_EQ(records.size(), static_cast<size_t>(PQntuples(res)))
            << "Result set row count mismatch";

        ASSERT_EQ(records.size(), mixedCaseDataType.size());
        for (size_t i = 0; i < records.size(); i++) {
            std::string suffix = std::to_string(i + 1);
            bool isBinary = isBinaryType(mixedCaseDataType[i]);

            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_PROCEDURE_CAT_COL_NUM), ("catalog" + suffix).c_str());
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_PROCEDURE_SCHEM_COL_NUM), ("schema" + suffix).c_str());
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_PROCEDURE_NAME_COL_NUM), ("procedure_function" + suffix).c_str());
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_COLUMN_NAME_COL_NUM), ("column" + suffix).c_str());

            // Type-specific assertions: "string" maps to SQL_VARCHAR, "binary" maps to SQL_LONGVARBINARY
            if (isBinary) {
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_DATA_TYPE_COL_NUM), std::to_string(SQL_LONGVARBINARY).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_TYPE_NAME_COL_NUM), "binary");
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_COLUMN_SIZE_COL_NUM), "");  // NULL (kNotApplicable)
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_BUFFER_LENGTH_COL_NUM), "");  // NULL (kNotApplicable)
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_SQL_DATA_TYPE_COL_NUM), std::to_string(SQL_LONGVARBINARY).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_NUM_PREC_RADIX_COL_NUM), "10");
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_CHAR_OCTET_LENGTH_COL_NUM), std::to_string(kUnknownColumnSize).c_str());
            } else {
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_DATA_TYPE_COL_NUM), std::to_string(SQL_VARCHAR).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_TYPE_NAME_COL_NUM), "string");
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_COLUMN_SIZE_COL_NUM), "256");
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_BUFFER_LENGTH_COL_NUM), "256");
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_SQL_DATA_TYPE_COL_NUM), std::to_string(SQL_VARCHAR).c_str());
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_NUM_PREC_RADIX_COL_NUM), "0");
                EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_CHAR_OCTET_LENGTH_COL_NUM), "256");
            }

            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_DECIMAL_DIGITS_COL_NUM), "");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_NULLABLE_COL_NUM), "2");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_REMARKS_COL_NUM), "");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_COLUMN_DEF_COL_NUM), "");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_SQL_DATETIME_SUB_COL_NUM), "");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_ORDINAL_POSITION_COL_NUM), "1");
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_IS_NULLABLE_COL_NUM), "");
        }
    }
};

TEST_F(SQLProcedureColumnsTest, EmptyResultSet) { TestEmptyResult(); }

TEST_F(SQLProcedureColumnsTest, PopulatedResultSet) { TestSingleResult(); }

TEST_F(SQLProcedureColumnsTest, MultipleRowResultSet) { TestMultipleResults(3); }

TEST_F(SQLProcedureColumnsTest, MaxLength) { TestMaxLength(); }

TEST_F(SQLProcedureColumnsTest, NullStmt) { TestNullStmt(); }

TEST_F(SQLProcedureColumnsTest, MixedCaseGlueDataType) { TestMixedCaseGlueDatatype(); }


struct ColumnsTestInput {
    int ordinal_position;
    std::string is_nullable;
    std::string data_type;
    std::string remark;
    int character_maximum_length;
    int numeric_precision;
    int numeric_scale;
};

struct ColumnsTestExpected {
    std::string expected_rs_type_name;
    SQLSMALLINT expected_data_type;
    SQLSMALLINT expected_sql_data_type;
    SQLSMALLINT expected_datetime_sub;
    std::string expected_column_size;
    std::string expected_buffer_length;
    std::string expected_decimal_digit;
    std::string expected_num_prec_radix;
    std::string expected_nullable;
    std::string expected_char_octet_length;
    std::string expected_ordinal_position;
    std::string expected_is_nullable;
};

class RsMetadataAPIPostProcessorParamTest :
    public RsMetadataAPIPostProcessorTest {
public:
    static std::vector<std::tuple<ColumnsTestInput, ColumnsTestExpected>> getDataTypeTestCase() {
        return {
            // Smallint
            std::make_tuple(
                ColumnsTestInput{
                    1, "NO", "smallint", "", NULL, 16, NULL
                },
                ColumnsTestExpected{
                    "int2", SQL_SMALLINT, SQL_SMALLINT, NULL,
                    "5", "2", "", "10", "0", "", "1", "NO"
                }
            ),
            // Integer
            std::make_tuple(
                ColumnsTestInput{
                    2, "NO", "integer", "", NULL, 32, NULL
                },
                ColumnsTestExpected{
                    "int4", SQL_INTEGER, SQL_INTEGER, NULL,
                    "10", "4", "", "10", "0", "", "2", "NO"
                }
            ),
            // Bigint
            std::make_tuple(
                ColumnsTestInput{
                    3, "NO", "bigint", "", NULL, 64, NULL
                },
                ColumnsTestExpected{
                    "int8", SQL_BIGINT, SQL_BIGINT, NULL,
                    "19", "8", "", "10", "0", "", "3", "NO"
                }
            ),
            // Numeric
            std::make_tuple(
                ColumnsTestInput{
                    4, "NO", "numeric", "", NULL, 10, 5
                },
                ColumnsTestExpected{
                    "numeric", SQL_NUMERIC, SQL_NUMERIC, NULL,
                    "10", "8", "5", "10", "0", "", "4", "NO"
                }
            ),
            // Real
            std::make_tuple(
                ColumnsTestInput{
                    5, "NO", "real", "", NULL, 10, NULL
                },
                ColumnsTestExpected{
                    "float4", SQL_REAL, SQL_REAL, NULL,
                    "7", "4", "6", "10", "0", "", "5", "NO"
                }
            ),
            // Double
            std::make_tuple(
                ColumnsTestInput{
                    6, "NO", "double precision", "", NULL, 10, NULL
                },
                ColumnsTestExpected{
                    "float8", SQL_DOUBLE, SQL_DOUBLE, NULL,
                    "15", "8", "15", "10", "0", "", "6", "NO"
                }
            ),
            // boolean
            std::make_tuple(
                ColumnsTestInput{
                    7, "NO", "boolean", "", NULL, NULL, NULL
                },
                ColumnsTestExpected{
                    "bool", SQL_BIT, SQL_BIT, NULL,
                    "1", "1", "", "10", "0", "", "7", "NO"
                }
            ),
            // char
            std::make_tuple(
                ColumnsTestInput{
                    8, "NO", "character", "", 10, NULL, NULL
                },
                ColumnsTestExpected{
                    "char", SQL_CHAR, SQL_CHAR, NULL,
                    "10", "10", "", "0", "0", "10", "8", "NO"
                }
            ),
            // VARCHAR
            std::make_tuple(
                ColumnsTestInput{
                    9, "NO", "character varying", "", 256, NULL, NULL
                },
                ColumnsTestExpected{
                    "varchar", SQL_VARCHAR, SQL_VARCHAR, NULL,
                    "256", "256", "", "0", "0", "256", "9", "NO"
                }
            ),
            // Date
            std::make_tuple(
                ColumnsTestInput{
                    10, "NO", "date", "", NULL, NULL, NULL
                },
                ColumnsTestExpected{
                    "date", SQL_TYPE_DATE, SQL_DATETIME, SQL_CODE_DATE,
                    "10", "6", "", "10", "0", "", "10", "NO"
                }
            ),
            // time
            std::make_tuple(
                ColumnsTestInput{
                    11, "NO", "time without time zone", "", NULL, NULL, NULL
                },
                ColumnsTestExpected{
                    "time", SQL_TYPE_TIME, SQL_DATETIME, SQL_CODE_TIME,
                    "15", "6", "6", "10", "0", "", "11", "NO"
                }
            ),
            // timetz
            std::make_tuple(
                ColumnsTestInput{
                    12, "NO", "time with time zone", "", NULL, NULL, NULL
                },
                ColumnsTestExpected{
                    "timetz", SQL_TYPE_TIME, SQL_DATETIME, SQL_CODE_TIME,
                    "21", "6", "6", "10", "0", "", "12", "NO"
                }
            ),
            // timestamp
            std::make_tuple(
                ColumnsTestInput{
                    13, "NO", "timestamp without time zone", "", NULL, NULL, NULL
                },
                ColumnsTestExpected{
                    "timestamp", SQL_TYPE_TIMESTAMP, SQL_DATETIME, SQL_CODE_TIMESTAMP,
                    "29", "16", "6", "10", "0", "", "13", "NO"
                }
            ),
            // timestamp customized second fraction
            std::make_tuple(
                ColumnsTestInput{
                    13, "NO", "timestamp without time zone (4)", "", NULL, NULL, NULL
                },
                ColumnsTestExpected{
                    "timestamp", SQL_TYPE_TIMESTAMP, SQL_DATETIME, SQL_CODE_TIMESTAMP,
                    "29", "16", "4", "10", "0", "", "13", "NO"
                }
            ),
            // timestamptz
            std::make_tuple(
                ColumnsTestInput{
                    14, "NO", "timestamp with time zone", "", NULL, NULL, NULL
                },
                ColumnsTestExpected{
                    "timestamptz", SQL_TYPE_TIMESTAMP, SQL_DATETIME, SQL_CODE_TIMESTAMP,
                    "35", "16", "6", "10", "0", "", "14", "NO"
                }
            ),
            // timestamptz customized second fraction
            std::make_tuple(
                ColumnsTestInput{
                    14, "NO", "timestamp with time zone (4)", "", NULL, NULL, NULL
                },
                ColumnsTestExpected{
                    "timestamptz", SQL_TYPE_TIMESTAMP, SQL_DATETIME, SQL_CODE_TIMESTAMP,
                    "35", "16", "4", "10", "0", "", "14", "NO"
                }
            ),
            // intervaly2m
            std::make_tuple(
                ColumnsTestInput{
                    15, "NO", "interval year to month", "", NULL, NULL, NULL
                },
                ColumnsTestExpected{
                    "intervaly2m", SQL_INTERVAL_YEAR_TO_MONTH, SQL_INTERVAL, SQL_CODE_YEAR_TO_MONTH,
                    "32", "8", "", "10", "0", "", "15", "NO"
                }
            ),
            // intervald2s
            std::make_tuple(
                ColumnsTestInput{
                    16, "NO", "interval day to second", "", NULL, NULL, NULL
                },
                ColumnsTestExpected{
                    "intervald2s", SQL_INTERVAL_DAY_TO_SECOND, SQL_INTERVAL, SQL_CODE_DAY_TO_SECOND,
                    "64", "20", "6", "10", "0", "", "16", "NO"
                }
            ),
            // intervald2s customized second fraction
            std::make_tuple(
                ColumnsTestInput{
                    16, "NO", "interval day to second (4)", "", NULL, NULL, NULL
                },
                ColumnsTestExpected{
                    "intervald2s", SQL_INTERVAL_DAY_TO_SECOND, SQL_INTERVAL, SQL_CODE_DAY_TO_SECOND,
                    "64", "20", "4", "10", "0", "", "16", "NO"
                }
            )
        };
    }
};

class SQLColumnsParamTest :
    public RsMetadataAPIPostProcessorParamTest,
    public ::testing::WithParamInterface<std::tuple<ColumnsTestInput, ColumnsTestExpected>> {};

TEST_P(SQLColumnsParamTest, SQLColumnsAllDataTypes) {
    const auto& [input, expected] = GetParam();
    std::vector<SHOWCOLUMNSResult> testRS;

    // Create multiple test rows
    for (int i = 0; i < 3; i++) {
        SHOWCOLUMNSResult result;
        std::string suffix = std::to_string(i + 1);
        result.database_name = "test_catalog" + suffix;
        result.schema_name = "test_schema" + suffix;
        result.table_name = "test_table" + suffix;
        result.column_name = "test_column" + suffix;
        result.ordinal_position = input.ordinal_position;
        result.column_default = "";
        result.is_nullable = input.is_nullable;
        result.data_type = input.data_type;
        result.character_maximum_length = input.character_maximum_length;
        result.numeric_precision = input.numeric_precision;
        result.numeric_scale = input.numeric_scale;
        result.remarks = input.remark;
        testRS.push_back(result);
    }

    SQLRETURN rc = RsMetadataAPIPostProcessor::sqlColumnsPostProcessing(
        stmt, testRS);

    EXPECT_EQ(rc, SQL_SUCCESS);

    // Verify result set properties
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
    EXPECT_EQ(pStmt->pResultHead->iNumberOfRowsInMem, 3);
    EXPECT_EQ(pStmt->pResultHead->iCurRow, -1);

    // Verify each row
    PGresult *res = pStmt->pResultHead->pgResult;
    for (int i = 0; i < 3; i++) {
        std::string suffix = std::to_string(i + 1);
        EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_TABLE_CAT_COL_NUM), ("test_catalog" + suffix).c_str());
        EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_TABLE_SCHEM_COL_NUM), ("test_schema" + suffix).c_str());
        EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_TABLE_NAME_COL_NUM), ("test_table" + suffix).c_str());
        EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_COLUMN_NAME_COL_NUM), ("test_column" + suffix).c_str());
        EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_DATA_TYPE_COL_NUM), std::to_string(expected.expected_data_type).c_str());
        EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_TYPE_NAME_COL_NUM), expected.expected_rs_type_name.c_str());
        EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_COLUMN_SIZE_COL_NUM), expected.expected_column_size.c_str());
        EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_BUFFER_LENGTH_COL_NUM), expected.expected_buffer_length.c_str());
        EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_DECIMAL_DIGITS_COL_NUM), expected.expected_decimal_digit.c_str());
        EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_NUM_PREC_RADIX_COL_NUM), expected.expected_num_prec_radix.c_str());
        EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_NULLABLE_COL_NUM), expected.expected_nullable.c_str());
        EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_REMARKS_COL_NUM), "");
        EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_COLUMN_DEF_COL_NUM), "");
        EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_SQL_DATA_TYPE_COL_NUM), std::to_string(expected.expected_sql_data_type).c_str());
        if (expected.expected_datetime_sub == NULL) {
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_SQL_DATETIME_SUB_COL_NUM), "");
        } else {
            EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_SQL_DATETIME_SUB_COL_NUM), std::to_string(expected.expected_datetime_sub).c_str());
        }
        EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_CHAR_OCTET_LENGTH_COL_NUM), expected.expected_char_octet_length.c_str());
        EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_ORDINAL_POSITION_COL_NUM), expected.expected_ordinal_position.c_str());
        EXPECT_STREQ(PQgetvalue(res, i, kSQLColumns_IS_NULLABLE_COL_NUM), expected.expected_is_nullable.c_str());
    }
}

INSTANTIATE_TEST_SUITE_P(METADATA_API_SUITE, SQLColumnsParamTest,
    ::testing::ValuesIn(
        RsMetadataAPIPostProcessorParamTest::getDataTypeTestCase()
    )
);


class SQLProcedureColumnsParamTest :
    public RsMetadataAPIPostProcessorParamTest,
    public ::testing::WithParamInterface<std::tuple<std::tuple<ColumnsTestInput, ColumnsTestExpected>, std::string>> {
        private:
            const std::unordered_map<std::string, int> procedureFunctionColumnTypeMap = {
                {"IN", SQL_PARAM_INPUT},
                {"INOUT", SQL_PARAM_INPUT_OUTPUT},
                {"OUT", SQL_PARAM_OUTPUT},
                {"TABLE", SQL_RESULT_COL},
                {"RETURN", SQL_RETURN_VALUE}
            };
        public:
            int getColumnType(const std::string& parameterType){
                auto it = procedureFunctionColumnTypeMap.find(parameterType);
                if (it != procedureFunctionColumnTypeMap.end()) {
                    return it->second;
                }
                return SQL_PARAM_TYPE_UNKNOWN;
            }
    };

TEST_P(SQLProcedureColumnsParamTest, SQLProcedureColumnsAllDataTypes) {
    const auto& [params, parameter_type] = GetParam();
    const auto& [input, expected] = params;
    std::vector<SHOWCOLUMNSResult> testRS;
    int columnType = getColumnType(parameter_type);
    // Create multiple test rows
    for (int i = 0; i < 3; i++) {
        SHOWCOLUMNSResult result;
        std::string suffix = std::to_string(i + 1);
        result.database_name = "test_catalog" + suffix;
        result.schema_name = "test_schema" + suffix;
        result.procedure_function_name = "test_procedure_function" + suffix;
        result.column_name = "test_column" + suffix;
        result.ordinal_position = input.ordinal_position;
        result.column_default = "";
        result.is_nullable = input.is_nullable;
        result.data_type = input.data_type;
        result.character_maximum_length = input.character_maximum_length;
        result.numeric_precision = input.numeric_precision;
        result.numeric_scale = input.numeric_scale;
        result.remarks = input.remark;
        result.parameter_type = parameter_type;
        testRS.push_back(result);
    }

    SQLRETURN rc = RsMetadataAPIPostProcessor::sqlProcedureColumnsPostProcessing(
        stmt, testRS);

    EXPECT_EQ(rc, SQL_SUCCESS);

    // Verify result set properties
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
    EXPECT_EQ(pStmt->pResultHead->iNumberOfRowsInMem, 3);
    EXPECT_EQ(pStmt->pResultHead->iCurRow, -1);

    // Verify each row
    PGresult *res = pStmt->pResultHead->pgResult;
    for (int i = 0; i < 3; i++) {
        std::string suffix = std::to_string(i + 1);
        EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_PROCEDURE_CAT_COL_NUM), ("test_catalog" + suffix).c_str());
        EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_PROCEDURE_SCHEM_COL_NUM), ("test_schema" + suffix).c_str());
        EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_PROCEDURE_NAME_COL_NUM), ("test_procedure_function" + suffix).c_str());
        EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_COLUMN_NAME_COL_NUM), ("test_column" + suffix).c_str());
        EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_COLUMN_TYPE_COL_NUM), std::to_string(columnType).c_str()); // Testing for now
        EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_DATA_TYPE_COL_NUM), std::to_string(expected.expected_data_type).c_str());
        EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_TYPE_NAME_COL_NUM), expected.expected_rs_type_name.c_str());
        EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_COLUMN_SIZE_COL_NUM), expected.expected_column_size.c_str());
        EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_BUFFER_LENGTH_COL_NUM), expected.expected_buffer_length.c_str());
        EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_DECIMAL_DIGITS_COL_NUM), expected.expected_decimal_digit.c_str());
        EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_NUM_PREC_RADIX_COL_NUM), expected.expected_num_prec_radix.c_str());
        EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_NULLABLE_COL_NUM), "2");
        EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_REMARKS_COL_NUM), "");
        EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_COLUMN_DEF_COL_NUM), "");
        EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_SQL_DATA_TYPE_COL_NUM), std::to_string(expected.expected_sql_data_type).c_str());
        if (expected.expected_datetime_sub == NULL) {
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_SQL_DATETIME_SUB_COL_NUM), "");
        } else {
            EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_SQL_DATETIME_SUB_COL_NUM), std::to_string(expected.expected_datetime_sub).c_str());
        }
        EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_CHAR_OCTET_LENGTH_COL_NUM), expected.expected_char_octet_length.c_str());
        EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_ORDINAL_POSITION_COL_NUM), expected.expected_ordinal_position.c_str());
        EXPECT_STREQ(PQgetvalue(res, i, kSQLProcedureColumns_IS_NULLABLE_COL_NUM), "");
    }
}

INSTANTIATE_TEST_SUITE_P(METADATA_API_SUITE, SQLProcedureColumnsParamTest,
    ::testing::Combine(
        ::testing::ValuesIn(
            RsMetadataAPIPostProcessorParamTest::getDataTypeTestCase()
        ),
        ::testing::Values("IN", "INOUT", "OUT", "TABLE", "RETURN")
    )
);




/*====================================================================================================================================================*/
// Tests for long string preservation through the post-processing pipeline.
// Verify that std::string-based SHOW*Result structs correctly preserve
// full-length data (e.g., long table/column comments, long column defaults)
// without truncation or data corruption.
//
// With the old fixed-buffer design, a mismatch between _Len and buffer size
// caused PQsetvalue to do a heap overread. With std::string fields, data and
// length are always in sync, making the overread structurally impossible.
// These tests verify that long strings survive the full pipeline intact.
/*====================================================================================================================================================*/

class SQLTablesBufferOverreadTest : public RsMetadataAPIPostProcessorTest {
};

// Verify that sqlTablesPostProcessing preserves remarks longer than 256 bytes without truncation.
// Passes a 1473-byte remark string and validates the stored value matches exactly.
TEST_F(SQLTablesBufferOverreadTest, LongRemarksPreservedFully) {
    SHOWTABLESResult result{};
    result.database_name = "testdb";
    result.schema_name = "public";
    result.table_name = "test_table";
    result.table_type = "TABLE";

    // Create a remark longer than the old MAX_REMARK_LEN (256)
    std::string longRemark(1473, 'R');
    result.remarks = longRemark;

    std::vector<SHOWTABLESResult> testRS = {result};

    SQLRETURN rc = RsMetadataAPIPostProcessor::sqlTablesPostProcessing(
        stmt, "TABLE", testRS);
    EXPECT_EQ(rc, SQL_SUCCESS);

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
    PGresult *res = pStmt->pResultHead->pgResult;
    ASSERT_NE(res, nullptr);
    ASSERT_EQ(PQntuples(res), 1);

    int storedLen = PQgetlength(res, 0, kSQLTables_REMARKS_COL_NUM);
    const char* storedVal = PQgetvalue(res, 0, kSQLTables_REMARKS_COL_NUM);

    // With std::string, the full 1473-byte remark should be stored
    EXPECT_EQ(storedLen, (int)longRemark.size())
        << "Remarks should be stored at full length, not truncated";
    EXPECT_EQ(std::string(storedVal, storedLen), longRemark)
        << "Stored remarks content should match the original";
}

// Verify that sqlTablesPostProcessing preserves database names longer than 128 bytes without truncation.
// Passes a 200-byte database_name and validates the stored length matches exactly.
TEST_F(SQLTablesBufferOverreadTest, LongNameFieldPreservedFully) {
    SHOWTABLESResult result{};

    std::string longName(500, 'D');
    result.database_name = longName;
    result.schema_name = "public";
    result.table_name = "test_table";
    result.table_type = "TABLE";
    result.remarks = "ok";

    std::vector<SHOWTABLESResult> testRS = {result};

    SQLRETURN rc = RsMetadataAPIPostProcessor::sqlTablesPostProcessing(
        stmt, "TABLE", testRS);
    EXPECT_EQ(rc, SQL_SUCCESS);

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
    PGresult *res = pStmt->pResultHead->pgResult;
    ASSERT_NE(res, nullptr);
    ASSERT_EQ(PQntuples(res), 1);

    int storedLen = PQgetlength(res, 0, kSQLTables_TABLE_CATALOG_COL_NUM);
    EXPECT_EQ(storedLen, (int)longName.size())
        << "database_name should be stored at full length";
}

// Verify that sqlTablesPostProcessing preserves all fields when multiple fields exceed 128 bytes.
// Passes rows with 200-byte name fields and 1473-byte remarks, validates all stored lengths.
TEST_F(SQLTablesBufferOverreadTest, MultipleLongFieldsPreserved) {
    SHOWTABLESResult result{};

    std::string longName(300, 'X');
    std::string longRemark(2000, 'R');

    result.database_name = longName;
    result.schema_name = std::string(400, 'S');
    result.table_name = std::string(500, 'T');
    result.table_type = "TABLE";
    result.remarks = longRemark;

    std::vector<SHOWTABLESResult> testRS = {result};

    SQLRETURN rc = RsMetadataAPIPostProcessor::sqlTablesPostProcessing(
        stmt, "TABLE", testRS);
    EXPECT_EQ(rc, SQL_SUCCESS);

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
    PGresult *res = pStmt->pResultHead->pgResult;
    ASSERT_NE(res, nullptr);
    ASSERT_EQ(PQntuples(res), 1);

    EXPECT_EQ(PQgetlength(res, 0, kSQLTables_TABLE_CATALOG_COL_NUM), 300)
        << "database_name length mismatch";
    EXPECT_EQ(PQgetlength(res, 0, kSQLTables_TABLE_SCHEM_COL_NUM), 400)
        << "schema_name length mismatch";
    EXPECT_EQ(PQgetlength(res, 0, kSQLTables_TABLE_NAME_COL_NUM), 500)
        << "table_name length mismatch";
    EXPECT_EQ(PQgetlength(res, 0, kSQLTables_REMARKS_COL_NUM), 2000)
        << "remarks length mismatch";
}

/*====================================================================================================================================================*/
// Level 1: String Length Boundary Tests
// Verify that std::string fields of all lengths survive the post-processing
// pipeline (PQsetvalue → PQgetvalue round-trip) without truncation or corruption.
/*====================================================================================================================================================*/

class SQLTablesStringBoundaryTest : public RsMetadataAPIPostProcessorTest {
protected:
    // Helper: create a SHOWTABLESResult with specified remarks length and
    // optional field lengths for name fields
    SHOWTABLESResult makeResult(size_t remarksLen, size_t nameLen = 10) {
        SHOWTABLESResult r;
        r.database_name = std::string(nameLen, 'D');
        r.schema_name = std::string(nameLen, 'S');
        r.table_name = std::string(nameLen, 'T');
        r.table_type = "TABLE";
        r.remarks = std::string(remarksLen, 'R');
        return r;
    }

    void verifyRoundTrip(const std::vector<SHOWTABLESResult>& testRS) {
        SQLRETURN rc = RsMetadataAPIPostProcessor::sqlTablesPostProcessing(
            stmt, "TABLE", testRS);
        EXPECT_EQ(rc, SQL_SUCCESS);

        RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
        PGresult *res = pStmt->pResultHead->pgResult;
        ASSERT_NE(res, nullptr);
        ASSERT_EQ(PQntuples(res), (int)testRS.size());

        for (size_t i = 0; i < testRS.size(); i++) {
            EXPECT_EQ(PQgetlength(res, i, kSQLTables_TABLE_CATALOG_COL_NUM),
                       (int)testRS[i].database_name.value().size())
                << "Row " << i << " database_name length mismatch";
            EXPECT_EQ(std::string(PQgetvalue(res, i, kSQLTables_TABLE_CATALOG_COL_NUM),
                                  PQgetlength(res, i, kSQLTables_TABLE_CATALOG_COL_NUM)),
                       testRS[i].database_name)
                << "Row " << i << " database_name content mismatch";

            EXPECT_EQ(PQgetlength(res, i, kSQLTables_TABLE_SCHEM_COL_NUM),
                       (int)testRS[i].schema_name.value().size())
                << "Row " << i << " schema_name length mismatch";

            EXPECT_EQ(PQgetlength(res, i, kSQLTables_TABLE_NAME_COL_NUM),
                       (int)testRS[i].table_name.value().size())
                << "Row " << i << " table_name length mismatch";

            EXPECT_EQ(PQgetlength(res, i, kSQLTables_REMARKS_COL_NUM),
                       (int)testRS[i].remarks.value().size())
                << "Row " << i << " remarks length mismatch";
            EXPECT_EQ(std::string(PQgetvalue(res, i, kSQLTables_REMARKS_COL_NUM),
                                  PQgetlength(res, i, kSQLTables_REMARKS_COL_NUM)),
                       testRS[i].remarks.value_or(""))
                << "Row " << i << " remarks content mismatch";
        }
    }
};

// Verify that empty string fields survive the post-processing round-trip without corruption.
TEST_F(SQLTablesStringBoundaryTest, EmptyStrings) {
    SHOWTABLESResult r;
    r.database_name = "";
    r.schema_name = "";
    r.table_name = "";
    r.table_type = "TABLE";
    r.remarks = "";
    verifyRoundTrip({r});
}

// Verify that normal-length strings (well within limits) round-trip correctly through post-processing.
TEST_F(SQLTablesStringBoundaryTest, NormalLength) {
    verifyRoundTrip({makeResult(50, 20)});
}

// Verify that name fields at exactly 127 bytes (max identifier length) round-trip correctly.
TEST_F(SQLTablesStringBoundaryTest, BoundaryNAMEDATALEN_Minus1) {
    // 127 bytes — exactly fits old NAMEDATALEN buffer
    verifyRoundTrip({makeResult(50, NAMEDATALEN - 1)});
}

// Verify that remarks at exactly 255 bytes round-trip correctly.
TEST_F(SQLTablesStringBoundaryTest, BoundaryMAX_REMARK_LEN_Minus1) {
    // 255 bytes — exactly fits old MAX_REMARK_LEN buffer
    verifyRoundTrip({makeResult(DEFAULT_MAX_REMARK_LEN - 1)});
}

// Verify that name fields at 128 bytes (one byte over max identifier length) are preserved.
TEST_F(SQLTablesStringBoundaryTest, OverBoundaryNAMEDATALEN) {
    // 128 bytes — would have overread by 1 byte with old fixed buffer
    verifyRoundTrip({makeResult(50, NAMEDATALEN)});
}

// Verify that name fields at 129 bytes are preserved without truncation.
TEST_F(SQLTablesStringBoundaryTest, OverBoundaryNAMEDATALEN_Plus1) {
    // 129 bytes
    verifyRoundTrip({makeResult(50, NAMEDATALEN + 1)});
}

// Verify that remarks at 256 bytes (one byte over the old limit) are preserved.
TEST_F(SQLTablesStringBoundaryTest, OverBoundaryMAX_REMARK_LEN) {
    // 256 bytes — would have overread by 1 byte
    verifyRoundTrip({makeResult(DEFAULT_MAX_REMARK_LEN)});
}

// Verify that remarks at 257 bytes are preserved without truncation.
TEST_F(SQLTablesStringBoundaryTest, OverBoundaryMAX_REMARK_LEN_Plus1) {
    // 257 bytes
    verifyRoundTrip({makeResult(DEFAULT_MAX_REMARK_LEN + 1)});
}

// Verify that 1473-byte remarks (a realistic large table COMMENT size) round-trip correctly.
TEST_F(SQLTablesStringBoundaryTest, LargeRemarks_DMS_Bug) {
    // 1473 bytes — exact length from the DMS customer bug report
    verifyRoundTrip({makeResult(1473)});
}

// Verify that very large remarks (8192 bytes) round-trip correctly through post-processing.
TEST_F(SQLTablesStringBoundaryTest, VeryLargeRemarks) {
    // 8192 bytes — realistic large table COMMENT
    verifyRoundTrip({makeResult(8192)});
}

// Verify that single-character string fields round-trip correctly through post-processing.
TEST_F(SQLTablesStringBoundaryTest, SingleCharStrings) {
    verifyRoundTrip({makeResult(1, 1)});
}

// Verify that multiple rows with varying field lengths all survive post-processing correctly.
TEST_F(SQLTablesStringBoundaryTest, MultiRowMixedLengths) {
    std::vector<SHOWTABLESResult> testRS = {
        makeResult(0, 5),           // empty remarks, short names
        makeResult(50, 20),         // normal
        makeResult(255, 127),       // at old boundary
        makeResult(1473, 200),      // over boundary (DMS bug)
        makeResult(8192, 500),      // very large
    };
    verifyRoundTrip(testRS);
}

// Verify that table type filtering works correctly when string fields exceed typical lengths.
TEST_F(SQLTablesStringBoundaryTest, TableTypeFilter_LongFields) {
    // Verify table type filtering still works with oversized fields
    SHOWTABLESResult view;
    view.database_name = std::string(200, 'D');
    view.schema_name = std::string(200, 'S');
    view.table_name = std::string(200, 'T');
    view.table_type = "VIEW";
    view.remarks = std::string(1000, 'R');

    SHOWTABLESResult table = makeResult(1473, 200);

    std::vector<SHOWTABLESResult> testRS = {view, table};

    // Filter for TABLE only — should exclude the VIEW
    SQLRETURN rc = RsMetadataAPIPostProcessor::sqlTablesPostProcessing(
        stmt, "TABLE", testRS);
    EXPECT_EQ(rc, SQL_SUCCESS);

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
    PGresult *res = pStmt->pResultHead->pgResult;
    ASSERT_NE(res, nullptr);
    ASSERT_EQ(PQntuples(res), 1);  // Only the TABLE row
    EXPECT_EQ(PQgetlength(res, 0, kSQLTables_REMARKS_COL_NUM), 1473);
}

// Verify that an empty result set is handled correctly by post-processing.
TEST_F(SQLTablesStringBoundaryTest, ZeroRows) {
    std::vector<SHOWTABLESResult> empty;
    SQLRETURN rc = RsMetadataAPIPostProcessor::sqlTablesPostProcessing(
        stmt, "TABLE", empty);
    EXPECT_EQ(rc, SQL_SUCCESS);

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
    EXPECT_EQ(pStmt->pResultHead->iNumberOfRowsInMem, 0);
}

// Verify that std::nullopt fields produce SQL NULL (PQgetisnull == true),
// while empty strings produce non-null zero-length values.
TEST_F(SQLTablesStringBoundaryTest, NulloptFieldsProduceSQLNull) {
    SHOWTABLESResult r;
    r.database_name = std::nullopt;  // SQL NULL
    r.schema_name = "";              // empty but not NULL
    r.table_name = "test";
    r.table_type = "TABLE";
    r.remarks = std::nullopt;        // SQL NULL

    SQLRETURN rc = RsMetadataAPIPostProcessor::sqlTablesPostProcessing(
        stmt, "TABLE", {r});
    EXPECT_EQ(rc, SQL_SUCCESS);

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
    PGresult *res = pStmt->pResultHead->pgResult;
    ASSERT_NE(res, nullptr);
    ASSERT_EQ(PQntuples(res), 1);

    // nullopt → SQL NULL
    EXPECT_TRUE(PQgetisnull(res, 0, kSQLTables_TABLE_CATALOG_COL_NUM))
        << "database_name=nullopt should produce SQL NULL";
    EXPECT_TRUE(PQgetisnull(res, 0, kSQLTables_REMARKS_COL_NUM))
        << "remarks=nullopt should produce SQL NULL";

    // "" → non-null, zero-length
    EXPECT_FALSE(PQgetisnull(res, 0, kSQLTables_TABLE_SCHEM_COL_NUM))
        << "schema_name=\"\" should NOT be SQL NULL";
    EXPECT_EQ(PQgetlength(res, 0, kSQLTables_TABLE_SCHEM_COL_NUM), 0)
        << "schema_name=\"\" should have zero length";

    // "test" → non-null, length 4
    EXPECT_FALSE(PQgetisnull(res, 0, kSQLTables_TABLE_NAME_COL_NUM));
    EXPECT_EQ(PQgetlength(res, 0, kSQLTables_TABLE_NAME_COL_NUM), 4);
}

/*====================================================================================================================================================*/
// UTF-8 Content Tests — Verify multi-byte UTF-8 characters are preserved through post-processing.
/*====================================================================================================================================================*/

// Verify that 2-byte UTF-8 characters (e.g., ñ) in remarks are preserved through post-processing.
TEST_F(SQLTablesStringBoundaryTest, UTF8_TwoByte) {
    SHOWTABLESResult r;
    // Build a string with 2-byte UTF-8 chars (ñ = 0xC3 0xB1) at boundary
    std::string twoByteChars;
    for (int i = 0; i < 128; i++) twoByteChars += "\xC3\xB1";  // 256 bytes, 128 chars
    r.database_name = "testdb";
    r.schema_name = "public";
    r.table_name = "test";
    r.table_type = "TABLE";
    r.remarks = twoByteChars;
    verifyRoundTrip({r});
}

// Verify that 3-byte UTF-8 characters (e.g., 中) in remarks are preserved through post-processing.
TEST_F(SQLTablesStringBoundaryTest, UTF8_ThreeByte) {
    SHOWTABLESResult r;
    // Build a string with 3-byte UTF-8 chars (中 = 0xE4 0xB8 0xAD)
    std::string threeByteChars;
    for (int i = 0; i < 100; i++) threeByteChars += "\xE4\xB8\xAD";  // 300 bytes
    r.database_name = "testdb";
    r.schema_name = "public";
    r.table_name = "test";
    r.table_type = "TABLE";
    r.remarks = threeByteChars;
    verifyRoundTrip({r});
}

// Verify that 4-byte UTF-8 characters (e.g., 😀) in remarks are preserved through post-processing.
TEST_F(SQLTablesStringBoundaryTest, UTF8_FourByte) {
    SHOWTABLESResult r;
    // Build a string with 4-byte UTF-8 chars (😀 = 0xF0 0x9F 0x98 0x80)
    std::string fourByteChars;
    for (int i = 0; i < 75; i++) fourByteChars += "\xF0\x9F\x98\x80";  // 300 bytes
    r.database_name = "testdb";
    r.schema_name = "public";
    r.table_name = "test";
    r.table_type = "TABLE";
    r.remarks = fourByteChars;
    verifyRoundTrip({r});
}

// Verify that mixed ASCII and multi-byte UTF-8 strings in remarks are preserved through post-processing.
TEST_F(SQLTablesStringBoundaryTest, UTF8_MixedWithASCII) {
    SHOWTABLESResult r;
    std::string mixed = "Hello ";
    for (int i = 0; i < 50; i++) mixed += "\xC3\xB1";   // 2-byte
    mixed += " World ";
    for (int i = 0; i < 30; i++) mixed += "\xE4\xB8\xAD"; // 3-byte
    mixed += " End";
    r.database_name = "testdb";
    r.schema_name = "public";
    r.table_name = "test";
    r.table_type = "TABLE";
    r.remarks = mixed;
    verifyRoundTrip({r});
}

/*====================================================================================================================================================*/
// All Struct Types — Boundary Tests
// Verify that each metadata result struct type preserves fields exceeding typical identifier lengths.
/*====================================================================================================================================================*/

class SQLColumnsStringBoundaryTest : public RsMetadataAPIPostProcessorTest {};

// Verify that sqlColumnsPostProcessing preserves column defaults and remarks exceeding typical lengths.
// Passes a 4500-byte COLUMN_DEF and 500-byte REMARKS, validates both are stored at full length.
TEST_F(SQLColumnsStringBoundaryTest, LongColumnDefault) {
    SHOWCOLUMNSResult r;
    r.database_name = "testdb";
    r.schema_name = "public";
    r.table_name = "test";
    r.column_name = "col1";
    r.ordinal_position = 1;
    r.column_default = std::string(4500, 'D');  // exceeds old MAX_COLUMN_DEF_LEN (4000)
    r.is_nullable = "YES";
    r.data_type = "character varying";
    r.character_maximum_length = 256;
    r.remarks = std::string(500, 'R');  // exceeds old MAX_REMARK_LEN (256)

    std::vector<SHOWCOLUMNSResult> testRS = {r};
    SQLRETURN rc = RsMetadataAPIPostProcessor::sqlColumnsPostProcessing(stmt, testRS);
    EXPECT_EQ(rc, SQL_SUCCESS);

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
    PGresult *res = pStmt->pResultHead->pgResult;
    ASSERT_NE(res, nullptr);
    ASSERT_EQ(PQntuples(res), 1);

    EXPECT_EQ(PQgetlength(res, 0, kSQLColumns_COLUMN_DEF_COL_NUM), 4500)
        << "column_default should preserve full 4500-byte value";
    EXPECT_EQ(PQgetlength(res, 0, kSQLColumns_REMARKS_COL_NUM), 500)
        << "remarks should preserve full 500-byte value";
    EXPECT_EQ(std::string(PQgetvalue(res, 0, kSQLColumns_COLUMN_DEF_COL_NUM), 4500),
              std::string(4500, 'D'));
}

// Verify that sqlColumnsPostProcessing preserves all name fields when they exceed 128 bytes.
// Passes 200-byte catalog, schema, table, and column names, validates all stored lengths.
TEST_F(SQLColumnsStringBoundaryTest, LongColumnName) {
    SHOWCOLUMNSResult r;
    r.database_name = std::string(200, 'D');
    r.schema_name = std::string(200, 'S');
    r.table_name = std::string(200, 'T');
    r.column_name = std::string(200, 'C');
    r.ordinal_position = 1;
    r.is_nullable = "NO";
    r.data_type = "integer";
    r.character_maximum_length = 0;
    r.numeric_precision = 32;

    std::vector<SHOWCOLUMNSResult> testRS = {r};
    SQLRETURN rc = RsMetadataAPIPostProcessor::sqlColumnsPostProcessing(stmt, testRS);
    EXPECT_EQ(rc, SQL_SUCCESS);

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
    PGresult *res = pStmt->pResultHead->pgResult;
    ASSERT_NE(res, nullptr);

    EXPECT_EQ(PQgetlength(res, 0, kSQLColumns_TABLE_CAT_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLColumns_TABLE_SCHEM_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLColumns_TABLE_NAME_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLColumns_COLUMN_NAME_COL_NUM), 200);
}

// Verify that std::nullopt fields on COLUMN_DEF and REMARKS produce SQL NULL,
// while empty strings produce non-null zero-length values.
TEST_F(SQLColumnsStringBoundaryTest, NulloptFieldsProduceSQLNull) {
    SHOWCOLUMNSResult r;
    r.database_name = "testdb";
    r.schema_name = "public";
    r.table_name = "test";
    r.column_name = "col1";
    r.ordinal_position = 1;
    r.column_default = std::nullopt;  // SQL NULL — no default defined
    r.is_nullable = "YES";
    r.data_type = "character varying";
    r.character_maximum_length = 256;
    r.remarks = std::nullopt;         // SQL NULL — no comment

    std::vector<SHOWCOLUMNSResult> testRS = {r};
    SQLRETURN rc = RsMetadataAPIPostProcessor::sqlColumnsPostProcessing(stmt, testRS);
    EXPECT_EQ(rc, SQL_SUCCESS);

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
    PGresult *res = pStmt->pResultHead->pgResult;
    ASSERT_NE(res, nullptr);
    ASSERT_EQ(PQntuples(res), 1);

    // nullopt → SQL NULL
    EXPECT_TRUE(PQgetisnull(res, 0, kSQLColumns_COLUMN_DEF_COL_NUM))
        << "column_default=nullopt should produce SQL NULL";
    EXPECT_TRUE(PQgetisnull(res, 0, kSQLColumns_REMARKS_COL_NUM))
        << "remarks=nullopt should produce SQL NULL";

    // Non-null fields should not be NULL
    EXPECT_FALSE(PQgetisnull(res, 0, kSQLColumns_TABLE_CAT_COL_NUM));
    EXPECT_FALSE(PQgetisnull(res, 0, kSQLColumns_TABLE_NAME_COL_NUM));
}

// Verify that empty string COLUMN_DEF and REMARKS are distinct from SQL NULL.
TEST_F(SQLColumnsStringBoundaryTest, EmptyStringDistinctFromNull) {
    SHOWCOLUMNSResult r;
    r.database_name = "testdb";
    r.schema_name = "public";
    r.table_name = "test";
    r.column_name = "col1";
    r.ordinal_position = 1;
    r.column_default = "";    // empty string — DEFAULT ''
    r.is_nullable = "YES";
    r.data_type = "character varying";
    r.character_maximum_length = 256;
    r.remarks = "";           // empty string — empty comment

    std::vector<SHOWCOLUMNSResult> testRS = {r};
    SQLRETURN rc = RsMetadataAPIPostProcessor::sqlColumnsPostProcessing(stmt, testRS);
    EXPECT_EQ(rc, SQL_SUCCESS);

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
    PGresult *res = pStmt->pResultHead->pgResult;
    ASSERT_NE(res, nullptr);
    ASSERT_EQ(PQntuples(res), 1);

    // "" → non-null, zero-length (distinct from SQL NULL)
    EXPECT_FALSE(PQgetisnull(res, 0, kSQLColumns_COLUMN_DEF_COL_NUM))
        << "column_default=\"\" should NOT be SQL NULL";
    EXPECT_EQ(PQgetlength(res, 0, kSQLColumns_COLUMN_DEF_COL_NUM), 0);

    EXPECT_FALSE(PQgetisnull(res, 0, kSQLColumns_REMARKS_COL_NUM))
        << "remarks=\"\" should NOT be SQL NULL";
    EXPECT_EQ(PQgetlength(res, 0, kSQLColumns_REMARKS_COL_NUM), 0);
}

class SQLPrimaryKeysStringBoundaryTest : public RsMetadataAPIPostProcessorTest {};

// Verify that sqlPrimaryKeysPostProcessing preserves all fields when they exceed 128 bytes.
// Passes 200-byte catalog, schema, table, column, and PK names, validates all stored lengths.
TEST_F(SQLPrimaryKeysStringBoundaryTest, OverBoundaryAllFields) {
    SHOWCONSTRAINTSPRIMARYKEYSResult r;
    r.database_name = std::string(200, 'D');
    r.schema_name = std::string(200, 'S');
    r.table_name = std::string(200, 'T');
    r.column_name = std::string(200, 'C');
    r.key_seq = 1;
    r.pk_name = std::string(200, 'P');

    std::vector<SHOWCONSTRAINTSPRIMARYKEYSResult> testRS = {r};
    SQLRETURN rc = RsMetadataAPIPostProcessor::sqlPrimaryKeysPostProcessing(stmt, testRS);
    EXPECT_EQ(rc, SQL_SUCCESS);

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
    PGresult *res = pStmt->pResultHead->pgResult;
    ASSERT_NE(res, nullptr);
    ASSERT_EQ(PQntuples(res), 1);

    EXPECT_EQ(PQgetlength(res, 0, kSQLPrimaryKeys_TABLE_CAT_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLPrimaryKeys_TABLE_SCHEM_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLPrimaryKeys_TABLE_NAME_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLPrimaryKeys_COLUMN_NAME_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLPrimaryKeys_PK_NAME_COL_NUM), 200);
}

class SQLForeignKeysStringBoundaryTest : public RsMetadataAPIPostProcessorTest {};

// Verify that sqlForeignKeysPostProcessing preserves all fields when they exceed 128 bytes.
// Passes 200-byte PK/FK catalog, schema, table, column, and constraint names, validates all stored lengths.
TEST_F(SQLForeignKeysStringBoundaryTest, OverBoundaryAllFields) {
    SHOWCONSTRAINTSFOREIGNKEYSResult r;
    r.pk_table_cat = std::string(200, 'A');
    r.pk_table_schem = std::string(200, 'B');
    r.pk_table_name = std::string(200, 'C');
    r.pk_column_name = std::string(200, 'D');
    r.fk_table_cat = std::string(200, 'E');
    r.fk_table_schem = std::string(200, 'F');
    r.fk_table_name = std::string(200, 'G');
    r.fk_column_name = std::string(200, 'H');
    r.key_seq = 1;
    r.update_rule = 1;
    r.delete_rule = 1;
    r.fk_name = std::string(200, 'I');
    r.pk_name = std::string(200, 'J');
    r.deferrability = 7;

    std::vector<SHOWCONSTRAINTSFOREIGNKEYSResult> testRS = {r};
    SQLRETURN rc = RsMetadataAPIPostProcessor::sqlForeignKeysPostProcessing(stmt, testRS);
    EXPECT_EQ(rc, SQL_SUCCESS);

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
    PGresult *res = pStmt->pResultHead->pgResult;
    ASSERT_NE(res, nullptr);
    ASSERT_EQ(PQntuples(res), 1);

    EXPECT_EQ(PQgetlength(res, 0, kSQLForeignKeys_PKTABLE_CAT_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLForeignKeys_PKTABLE_SCHEM_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLForeignKeys_PKTABLE_NAME_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLForeignKeys_PKCOLUMN_NAME_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLForeignKeys_FKTABLE_CAT_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLForeignKeys_FKTABLE_SCHEM_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLForeignKeys_FKTABLE_NAME_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLForeignKeys_FKCOLUMN_NAME_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLForeignKeys_FK_NAME_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLForeignKeys_PK_NAME_COL_NUM), 200);
}

class SQLProceduresStringBoundaryTest : public RsMetadataAPIPostProcessorTest {};

// Verify that sqlProceduresPostProcessing handles a procedure with a very long argument list (5000 bytes).
// Validates that the procedure name and catalog are preserved even when argument_list is oversized.
TEST_F(SQLProceduresStringBoundaryTest, LongArgumentList) {
    SHOWPROCEDURESFUNCTIONSResult r;
    r.object_cat = "testdb";
    r.object_schem = "public";
    r.object_name = "my_proc";
    r.argument_list = std::string(5000, 'A');  // exceeds old 4096-byte buffer
    r.object_type = SQL_PT_PROCEDURE;

    std::vector<SHOWPROCEDURESFUNCTIONSResult> testRS = {r};
    SQLRETURN rc = RsMetadataAPIPostProcessor::sqlProceduresPostProcessing(stmt, testRS);
    EXPECT_EQ(rc, SQL_SUCCESS);

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
    PGresult *res = pStmt->pResultHead->pgResult;
    ASSERT_NE(res, nullptr);
    ASSERT_EQ(PQntuples(res), 1);

    // Procedures post-processing doesn't expose argument_list directly,
    // but verify the other fields survived
    EXPECT_EQ(PQgetlength(res, 0, kSQLProcedures_PROCEDURE_CAT_COL_NUM), 6);
    EXPECT_EQ(PQgetlength(res, 0, kSQLProcedures_PROCEDURE_NAME_COL_NUM), 7);
}

class SQLGrantsStringBoundaryTest : public RsMetadataAPIPostProcessorTest {};

// Verify that sqlTablePrivilegesPostProcessing preserves all fields when they exceed 128 bytes.
// Passes 200-byte catalog, schema, table, grantor, and grantee names, validates all stored lengths.
TEST_F(SQLGrantsStringBoundaryTest, TablePrivileges_OverBoundary) {
    SHOWGRANTSTABLEResult r;
    r.table_cat = std::string(200, 'C');
    r.table_schem = std::string(200, 'S');
    r.table_name = std::string(200, 'T');
    r.grantor = std::string(200, 'G');
    r.grantee = std::string(200, 'U');
    r.privilege = "SELECT";
    r.admin_option = 1;

    std::vector<SHOWGRANTSTABLEResult> testRS = {r};
    SQLRETURN rc = RsMetadataAPIPostProcessor::sqlTablePrivilegesPostProcessing(stmt, testRS);
    EXPECT_EQ(rc, SQL_SUCCESS);

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
    PGresult *res = pStmt->pResultHead->pgResult;
    ASSERT_NE(res, nullptr);
    ASSERT_EQ(PQntuples(res), 1);

    EXPECT_EQ(PQgetlength(res, 0, kSQLTablePrivileges_TABLE_CAT_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLTablePrivileges_TABLE_SCHEM_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLTablePrivileges_TABLE_NAME_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLTablePrivileges_GRANTOR_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLTablePrivileges_GRANTEE_COL_NUM), 200);
}

// Verify that sqlColumnPrivilegesPostProcessing preserves all fields when they exceed 128 bytes.
// Passes 200-byte catalog, schema, table, column, grantor, and grantee names, validates all stored lengths.
TEST_F(SQLGrantsStringBoundaryTest, ColumnPrivileges_OverBoundary) {
    SHOWGRANTSCOLUMNResult r;
    r.table_cat = std::string(200, 'C');
    r.table_schem = std::string(200, 'S');
    r.table_name = std::string(200, 'T');
    r.column_name = std::string(200, 'N');
    r.grantor = std::string(200, 'G');
    r.grantee = std::string(200, 'U');
    r.privilege = "INSERT";
    r.admin_option = 0;

    std::vector<SHOWGRANTSCOLUMNResult> testRS = {r};
    SQLRETURN rc = RsMetadataAPIPostProcessor::sqlColumnPrivilegesPostProcessing(stmt, testRS);
    EXPECT_EQ(rc, SQL_SUCCESS);

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
    PGresult *res = pStmt->pResultHead->pgResult;
    ASSERT_NE(res, nullptr);
    ASSERT_EQ(PQntuples(res), 1);

    EXPECT_EQ(PQgetlength(res, 0, kSQLColumnPrivileges_TABLE_CAT_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLColumnPrivileges_TABLE_SCHEM_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLColumnPrivileges_TABLE_NAME_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLColumnPrivileges_COLUMN_NAME_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLColumnPrivileges_GRANTOR_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLColumnPrivileges_GRANTEE_COL_NUM), 200);
}

class SQLSchemasStringBoundaryTest : public RsMetadataAPIPostProcessorTest {};

// Verify that sqlSchemasPostProcessing preserves database and schema names when they exceed 128 bytes.
// Passes 200-byte database and schema names, validates both stored lengths.
TEST_F(SQLSchemasStringBoundaryTest, OverBoundary) {
    SHOWSCHEMASResult r;
    r.database_name = std::string(200, 'D');
    r.schema_name = std::string(200, 'S');

    std::vector<SHOWSCHEMASResult> testRS = {r};
    SQLRETURN rc = RsMetadataAPIPostProcessor::sqlSchemasPostProcessing(stmt, testRS);
    EXPECT_EQ(rc, SQL_SUCCESS);

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
    PGresult *res = pStmt->pResultHead->pgResult;
    ASSERT_NE(res, nullptr);
    ASSERT_EQ(PQntuples(res), 1);

    EXPECT_EQ(PQgetlength(res, 0, kSQLTables_TABLE_CATALOG_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLTables_TABLE_SCHEM_COL_NUM), 200);
}

/*====================================================================================================================================================*/
// Null Numeric Fields with Long String Fields
// Verify that SQL_NULL_DATA on numeric fields does not interfere with long string field storage.
/*====================================================================================================================================================*/

// Verify that sqlColumnsPostProcessing correctly handles NULL numeric fields alongside long string fields.
// Passes a 200-byte database name and 500-byte remarks with NULL sort_key and dist_key, validates all fields.
TEST_F(SQLColumnsStringBoundaryTest, NullNumericFieldsWithLongStrings) {
    SHOWCOLUMNSResult r;
    r.database_name = std::string(200, 'D');
    r.schema_name = "public";
    r.table_name = "test";
    r.column_name = "col1";
    r.ordinal_position = 1;
    r.is_nullable = "YES";
    r.data_type = "character varying";
    r.character_maximum_length = 256;
    r.remarks = std::string(500, 'R');
    r.sort_key = 0;
    r.sort_key_Len = SQL_NULL_DATA;
    r.dist_key = 0;
    r.dist_key_Len = SQL_NULL_DATA;

    std::vector<SHOWCOLUMNSResult> testRS = {r};
    SQLRETURN rc = RsMetadataAPIPostProcessor::sqlColumnsPostProcessing(stmt, testRS);
    EXPECT_EQ(rc, SQL_SUCCESS);

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)stmt;
    PGresult *res = pStmt->pResultHead->pgResult;
    ASSERT_NE(res, nullptr);

    // Verify long strings survived
    EXPECT_EQ(PQgetlength(res, 0, kSQLColumns_TABLE_CAT_COL_NUM), 200);
    EXPECT_EQ(PQgetlength(res, 0, kSQLColumns_REMARKS_COL_NUM), 500);

    // Verify NULL numeric fields
    EXPECT_TRUE(PQgetisnull(res, 0, kSQLColumns_SORT_KEY_COL_NUM));
    EXPECT_TRUE(PQgetisnull(res, 0, kSQLColumns_DIST_KEY_COL_NUM));
}
