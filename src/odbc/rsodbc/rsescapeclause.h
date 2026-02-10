
#pragma once

#include <string>
#include <vector>

// Forward declarations
struct RS_STMT_INFO;
struct _RS_STR_BUF;
typedef struct _RS_STR_BUF RS_STR_BUF;


// Maximum replacement length for ODBC escape clause conversion
#define MAX_ODBC_ESCAPE_CLAUSE_REPLACE_LEN 20

// ODBC escape clause markers
#define ODBC_ESCAPE_CLAUSE_START_MARKER '{'
#define ODBC_ESCAPE_CLAUSE_END_MARKER '}'

// Maximum nesting depth for escape clauses to prevent stack overflow
#define MAX_ESCAPE_CLAUSE_NESTING_DEPTH 100

// Parser context and state definitions
enum class ParseState {
    NORMAL,                 // Default state: not in any special context
    IN_SINGLE_QUOTE,        // Inside single-quoted string literal
    IN_DOUBLE_QUOTE,        // Inside double-quoted identifier
    IN_SINGLE_LINE_COMMENT, // Inside single-line comment (--)
    IN_MULTI_LINE_COMMENT,  // Inside multi-line comment (/* */)
    AFTER_STRING,           // Just finished parsing a string literal
    AFTER_IDENTIFIER        // Just finished parsing an identifier
};

struct ParserContext {
    const std::string &sql;
    size_t pos;
    size_t length;
    ParseState state;
    int parenthesesDepth;
    int braceDepth;
    std::vector<std::string> &arguments;

    ParserContext(const std::string &s, size_t p,
                  std::vector<std::string> &args)
        : sql(s), pos(p), length(s.length()), state(ParseState::NORMAL),
          parenthesesDepth(0), braceDepth(0), arguments(args) {}
};

class ODBCEscapeClauseProcessor {
  public:
    // Public interface - main entry points
    /**
     * @brief Check and replace ODBC parameter markers and escape clauses.
     *
     * Replaces ODBC '?' parameter markers with PostgreSQL '$n' format and
     * processes ODBC escape clauses in the SQL statement.
     *
     * @param pStmt Pointer to statement info structure
     * @param pData Pointer to SQL statement data
     * @param cbLen Length of SQL statement (or SQL_NTS for null-terminated)
     * @param pPaStrBuf Buffer structure for result storage
     * @param iReplaceParamMarker TRUE to replace parameter markers, FALSE to
     * skip
     * @return Pointer to processed SQL string, or NULL on error
     */
    static unsigned char *checkReplaceParamMarkerAndODBCEscapeClause(
        RS_STMT_INFO *pStmt, char *pData, size_t cbLen, RS_STR_BUF *pPaStrBuf,
        int iReplaceParamMarker);
    /**
     * @brief Replace ODBC parameter markers and escape clauses in SQL
     * statement.
     *
     * Performs the actual replacement of '?' markers with '$n' format and
     * converts ODBC escape sequences to Redshift-native syntax.
     *
     * @param pStmt Pointer to statement info structure
     * @param pData Pointer to SQL statement data
     * @param cbLen Length of SQL statement
     * @param pPaStrBuf Buffer structure for result storage
     * @param numOfParamMarkers Number of parameter markers in the statement
     * @param numOfODBCEscapeClauses Number of escape clauses in the statement
     * @return Pointer to processed SQL string
     */
    static unsigned char *replaceParamMarkerAndODBCEscapeClause(
        RS_STMT_INFO *pStmt, char *pData, size_t cbLen, RS_STR_BUF *pPaStrBuf,
        int numOfParamMarkers, int numOfODBCEscapeClauses);

    /**
     * @brief Determine if ODBC escape clause scanning is needed.
     * @param pStmt Pointer to statement info, or NULL
     * @return TRUE if escape clause scanning should be performed, FALSE
     * otherwise
     */
    static int needToScanODBCEscapeClause(RS_STMT_INFO *pStmt);
    /**
     * @brief Count parameter markers in a SQL statement.
     *
     * Counts '?' parameter markers while respecting quoted strings,
     * double-quoted identifiers, and SQL comments.
     *
     * @param pData Pointer to SQL statement data
     * @param cbLen Length of SQL statement (or SQL_NTS for null-terminated)
     * @return Number of parameter markers found
     */
    static int countParamMarkers(char *pData, size_t cbLen);
    /**
     * @brief Count ODBC escape clauses in a SQL statement.
     *
     * Counts '{' markers that indicate ODBC escape clauses, considering whether
     * escape clause scanning is enabled.
     *
     * @param pStmt Pointer to statement info structure
     * @param pData Pointer to SQL statement data
     * @param cbLen Length of SQL statement (or SQL_NTS for null-terminated)
     * @return Number of escape clauses found
     */
    static int countODBCEscapeClauses(RS_STMT_INFO *pStmt, char *pData,
                                      size_t cbLen);

    /**
     * @brief Parse function arguments from SQL string into a vector of argument
     * strings.
     *
     * Extracts arguments from ODBC escape sequences like {fn CONCAT('a', 'b')}
     * or {d '2023-01-01'}. Handles SQL syntax within arguments including
     * quotes, comments, nested parentheses, parameter markers (?), and
     * recursive escape clauses.
     *
     * Examples:
     *   {fn FUNC(?, 'test')} → ["$1", "'test'"] (with parameter conversion)
     *   {fn CONCAT('Hello', 'World')} → ["'Hello'", "'World'"]
     *   {d '2023-01-01'} → ["'2023-01-01'"]
     *
     * @param[in]     sql              SQL string containing the escape clause
     * function
     * @param[in,out] i                Position in SQL (updated to after closing
     * parenthesis)
     * @param[out]    out              Vector to store parsed argument strings
     * (trimmed of whitespace)
     * @param[in]     pStmt            Statement context for nested escape
     * processing
     * @param[in]     numOfParamMarkers Total parameter markers for ? → $n
     * conversion
     * @param[in,out] paramNumber      Reference to current parameter number
     * counter (for parameter replacement)
     * @param[in]     depth            Nesting depth to prevent stack overflow
     *
     * @return true if parsing succeeded and closing parenthesis was found,
     * false otherwise
     *
     * @note The function expects the starting position to be at or before the
     * opening parenthesis '('
     * @note Empty arguments are automatically removed from the output vector
     * @note Argument strings are trimmed of leading and trailing whitespace
     * @note Nested ODBC escape sequences are processed recursively if pStmt is
     * provided
     * @note Parameter markers ('?') are converted to PostgreSQL format ('($n)')
     * if numOfParamMarkers > 0
     * @note Maximum nesting depth is controlled by
     * MAX_ESCAPE_CLAUSE_NESTING_DEPTH constant
     */
    static bool parseFunctionArguments(const std::string &sql, size_t &i,
                                       std::vector<std::string> &out,
                                       RS_STMT_INFO *pStmt,
                                       int numOfParamMarkers, int &paramNumber,
                                       int depth);
    /**
     * @brief Look up Redshift SQL type name from ODBC SQL type name.
     * @param odbcTypeName ODBC SQL type name (e.g., "SQL_VARCHAR")
     * @return Corresponding Redshift type name, or nullptr if not found
     */
    static const char *lookupSqlType(const std::string &odbcTypeName);
    /**
     * @brief Look up Redshift date part name from ODBC interval token.
     * @param odbcIntervalToken ODBC interval token (e.g., "SQL_TSI_DAY")
     * @return Corresponding Redshift date part name, or nullptr if not found
     */
    static const char *
    lookupIntervalToken(const std::string &odbcIntervalToken);

  private:
    /**
     * @brief Skip whitespace characters in SQL string starting from given
     * position.
     * @param sql SQL string to parse
     * @param pos Starting position
     * @return Position of first non-whitespace character or end of string
     */
    static size_t skipWhitespace(const std::string &sql, size_t pos);
    /**
     * @brief Check if the current character matches any delimiter in the list.
     * @param pSrc Pointer to current character in source string
     * @param fnNameDelimiterList Null-terminated string of delimiter characters
     * @return TRUE if character matches a delimiter, FALSE otherwise
     */
    static int
    checkDelimiterForODBCEscapeClauseToken(const char *pSrc,
                                           char *fnNameDelimiterList);
    /**
     * @brief Extract the next token from an ODBC escape clause.
     * @param ppSrc Pointer to pointer to current position in source string
     * (updated on return)
     * @param cbLen Total length of source buffer
     * @param pi Pointer to current index position (updated on return)
     * @param fnNameDelimiterList Optional delimiter characters that terminate
     * the token
     * @return Pointer to start of token, or same as updated *ppSrc if no token
     * found
     */
    static char *getNextTokenForODBCEscapeClause(char **ppSrc, size_t cbLen,
                                                 int *pi,
                                                 char *fnNameDelimiterList);
    /**
     * @brief Skip function parentheses in ODBC escape clause.
     * @param ppSrc Pointer to pointer to current position (updated if brackets
     * found)
     * @param cbLen Total length of source buffer
     * @param i Current index position
     * @param iBoth TRUE to skip both '(' and ')', FALSE to skip only '('
     * @return Updated index position after skipping brackets
     */
    static int skipFunctionBracketsForODBCEscapeClauseToken(char **ppSrc,
                                                            size_t cbLen, int i,
                                                            int iBoth);
    /**
     * @brief Process ODBC escape function call and convert to Redshift-specific
     * syntax.
     *
     * This function parses an ODBC escape sequence function call (e.g., {fn
     * UCASE(column)}) and converts it to the equivalent Redshift-native
     * function call (e.g., UPPER(column)). It handles function name mapping,
     * argument parsing, parameter marker replacement, and nested escape
     * sequences within function arguments.
     *
     * @param[in]     pStmt           Pointer to statement information structure
     * @param[in,out] ppDest          Pointer to destination buffer pointer
     * (updated during processing)
     * @param[in]     pDestStart      Start of destination buffer (for bounds
     * checking)
     * @param[in]     iDestBufLen     Length of destination buffer
     * @param[in,out] ppSrc           Pointer to source buffer pointer (updated
     * during processing)
     * @param[in]     cbLen           Length of source buffer
     * @param[in]     i               Current position in source buffer
     * @param[in]     numOfParamMarkers Total number of parameter markers in the
     * query
     * @param[in,out] paramNumber     Reference to current parameter number
     * counter
     * @param[in]     pToken          Pointer to function name token (not used
     * in current implementation)
     * @param[in]     iTokenLen       Length of function name token (not used in
     * current implementation)
     * @param[in]     depth           Current nesting depth for recursive escape
     * clause processing (prevents stack overflow)
     * @param[out]    parseError      Reference to parse error flag (set to TRUE
     * if parsing fails)
     *
     * @return Updated position in source buffer after processing the function
     * call. Returns -1 if parsing fails or unknown function encountered
     * (signals to preserve original escape clause).
     */
    static int processEscapeFunctionCall(RS_STMT_INFO *pStmt, char **ppDest,
                                         char *pDestStart, int iDestBufLen,
                                         char **ppSrc, size_t cbLen, int i,
                                         int numOfParamMarkers,
                                         int &paramNumber, char *pToken,
                                         int iTokenLen, int depth,
                                         int &parseError);
    /**
     * @brief Replace ODBC escape clause from the SQL statement.
     *
     * This function processes various ODBC escape sequences and converts them
     * to Redshift-native syntax. Handles escape types: fn (scalar functions), d
     * (date), ts (timestamp), t (time), call (procedure calls), oj (outer
     * join), escape (LIKE escape), and interval types.
     *
     * @param[in]     pStmt           Pointer to statement information structure
     * @param[in,out] ppDest          Pointer to destination buffer pointer
     * (updated during processing)
     * @param[in]     pDestStart      Start of destination buffer (for bounds
     * checking)
     * @param[in]     iDestBufLen     Length of destination buffer
     * @param[in,out] ppSrc           Pointer to source buffer pointer (must
     * point to '{' on entry, points to '}' on exit)
     * @param[in]     cbLen           Length of source buffer
     * @param[in]     i               Current position in source buffer
     * @param[in]     numOfParamMarkers Total number of parameter markers in the
     * query
     * @param[in,out] paramNumber     Reference to current parameter number
     * counter
     * @param[in]     depth           Current nesting depth for recursive escape
     * clause processing (prevents stack overflow)
     * @param[out]    parseError      Reference to parse error flag (set to TRUE
     * if parsing fails or unsupported escape sequence encountered)
     *
     * @return Updated position in source buffer after processing the escape
     * clause. Returns -1 if parsing fails or unsupported escape sequence
     * encountered.
     *
     * @note Supports nested escape sequences within function arguments
     * @note Maximum nesting depth is controlled by
     * MAX_ESCAPE_CLAUSE_NESTING_DEPTH
     * @note Source pointer (pSrc) must be at '{' when called and will be at '}'
     * when returning successfully
     */
    static int replaceODBCEscapeClause(RS_STMT_INFO *pStmt, char **ppDest,
                                       char *pDestStart, int iDestBufLen,
                                       char **ppSrc, size_t cbLen, int srcPos,
                                       int numOfParamMarkers, int &paramNumber,
                                       int depth, int &parseError);
};
