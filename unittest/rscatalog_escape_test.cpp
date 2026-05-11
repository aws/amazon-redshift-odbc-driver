#include "common.h"
#include "rscatalog.h"
#include <cstring>
#include <string>

// Helper: calls escapedFilterCondition and returns the result as a std::string,
// freeing the allocated buffer.
static std::string escape(const char *input) {
    char *result = escapedFilterCondition(input, (short)strlen(input));
    std::string s(result);
    free(result);
    return s;
}

// --- Basic functionality ---

TEST(EscapedFilterCondition, PlainString) {
    EXPECT_EQ(escape("hello"), "hello");
}

TEST(EscapedFilterCondition, EmptyString) {
    EXPECT_EQ(escape(""), "");
}

// --- Quote escaping (pre-existing behavior) ---

TEST(EscapedFilterCondition, SingleQuoteIsDoubled) {
    EXPECT_EQ(escape("it's"), "it''s");
}

TEST(EscapedFilterCondition, MultipleQuotes) {
    EXPECT_EQ(escape("can't won't"), "can''t won''t");
}

// --- Backslash escaping (the fix) ---

TEST(EscapedFilterCondition, BackslashIsDoubled) {
    EXPECT_EQ(escape("my\\db"), "my\\\\db");
}

TEST(EscapedFilterCondition, MultipleBackslashes) {
    EXPECT_EQ(escape("a\\\\b"), "a\\\\\\\\b");
}

// --- SQL injection attack vector ---

TEST(EscapedFilterCondition, BackslashQuoteInjection) {
    // The attack input: test\' OR 1=1 --
    // After escaping: test\\'' OR 1=1 --
    // When wrapped in SQL quotes: 'test\\'' OR 1=1 --'
    // Server sees: \\ = literal backslash, '' = literal quote, rest is string content.
    EXPECT_EQ(escape("test\\' OR 1=1 --"), "test\\\\'' OR 1=1 --");
}

// --- Edge cases ---

TEST(EscapedFilterCondition, OnlyBackslash) {
    EXPECT_EQ(escape("\\"), "\\\\");
}

TEST(EscapedFilterCondition, OnlyQuote) {
    EXPECT_EQ(escape("'"), "''");
}

TEST(EscapedFilterCondition, BackslashAtEnd) {
    EXPECT_EQ(escape("abc\\"), "abc\\\\");
}

TEST(EscapedFilterCondition, QuoteAtEnd) {
    EXPECT_EQ(escape("abc'"), "abc''");
}

TEST(EscapedFilterCondition, ConsecutiveBackslashQuote) {
    // \' should become \\''
    EXPECT_EQ(escape("\\'"), "\\\\''");
}

TEST(EscapedFilterCondition, NoSpecialChars) {
    EXPECT_EQ(escape("normal_database_name"), "normal_database_name");
}
