#include "rsescapeclause.h"
#include "rsodbc.h"
#include "rsutil.h"

// File-scope static mapping tables for ODBC escape clause processing
static const RS_MAP_INTERVAL_NAME s_intervalMappings[] = {
    {"SQL_TSI_FRAC_SECOND", "us"}, {"SQL_TSI_SECOND", "s"},
    {"SQL_TSI_MINUTE", "m"},       {"SQL_TSI_HOUR", "h"},
    {"SQL_TSI_DAY", "d"},          {"SQL_TSI_WEEK", "w"},
    {"SQL_TSI_MONTH", "mon"},      {"SQL_TSI_QUARTER", "qtr"},
    {"SQL_TSI_YEAR", "y"},         {NULL, NULL}};

static const RS_MAP_SQL_TYPE_NAME s_typeMappings[] = {
    {"SQL_BIGINT", "BIGINT"},
    {"SQL_BIT", "BOOL"},
    {"SQL_CHAR", "CHAR"},
    {"SQL_DECIMAL", "DECIMAL"},
    {"SQL_DOUBLE", "FLOAT8"},
    {"SQL_FLOAT", "FLOAT8"},
    {"SQL_INTEGER", "INTEGER"},
    {"SQL_NUMERIC", "NUMERIC"},
    {"SQL_REAL", "REAL"},
    {"SQL_SMALLINT", "SMALLINT"},
    {"SQL_TYPE_DATE", "DATE"},
    {"SQL_DATE", "DATE"},
    {"SQL_TYPE_TIMESTAMP", "TIMESTAMP"},
    {"SQL_TIMESTAMP", "TIMESTAMP"},
    {"SQL_TYPE_TIME", "TIME"},
    {"SQL_TIME", "TIME"},
    {"SQL_VARCHAR", "VARCHAR"},
    {"SQL_INTERVAL_YEAR_TO_MONTH", "INTERVALY2M"},
    {"SQL_INTERVAL_DAY_TO_SECOND", "INTERVALD2S"},
    {NULL, NULL}};

size_t ODBCEscapeClauseProcessor::skipWhitespace(const std::string &sql,
                                                 size_t pos) {
    while (pos < sql.length() && std::isspace(sql[pos])) {
        pos++;
    }
    return pos;
}

int ODBCEscapeClauseProcessor::checkDelimiterForODBCEscapeClauseToken(
    const char *pSrc, char *fnNameDelimiterList) {
    int iDelimiterFound = FALSE;

    while (*fnNameDelimiterList) {
        if (*pSrc == *fnNameDelimiterList) {
            iDelimiterFound = TRUE;
            break;
        }

        fnNameDelimiterList++;
    }

    return iDelimiterFound;
}

char *ODBCEscapeClauseProcessor::getNextTokenForODBCEscapeClause(
    char **ppSrc, size_t cbLen, int *pi, char *fnNameDelimiterList) {
    char *pToken = NULL;
    char *pSrc = *ppSrc;
    int i = *pi;

    // Trim leading space
    while (isspace(*pSrc) && *pSrc && i < (int)cbLen) {
        pSrc++;
        i++;
    }

    // Get the second token
    pToken = pSrc;
    while (!isspace(*pSrc) && *pSrc && i < (int)cbLen) {
        if (fnNameDelimiterList) {
            int iDelimiterFound = checkDelimiterForODBCEscapeClauseToken(
                pSrc, fnNameDelimiterList);

            if (iDelimiterFound)
                break;
        }

        pSrc++;
        i++;
    }

    // Update the output var
    *ppSrc = pSrc;
    *pi = i;

    return pToken;
}

int ODBCEscapeClauseProcessor::skipFunctionBracketsForODBCEscapeClauseToken(
    char **ppSrc, size_t cbLen, int i, int iBoth) {
    int iValid = FALSE;
    char *pSrc = *ppSrc;
    int iSavI = i;
    char *pToken;

    // Remove () or (
    pToken = getNextTokenForODBCEscapeClause(&pSrc, cbLen, &i, "(");
    if (pToken && *pToken == '(') {
        // Skip '('
        pSrc++;
        i++;

        if (iBoth) {
            pToken = getNextTokenForODBCEscapeClause(&pSrc, cbLen, &i, ")");
            if (pToken && *pToken == ')') {
                // Skip ')'
                pSrc++;
                i++;

                iValid = TRUE;
            }
        } else
            iValid = TRUE;
    }

    if (iValid)
        *ppSrc = pSrc;
    else
        i = iSavI;

    return i;
}

const char *ODBCEscapeClauseProcessor::lookupIntervalToken(
    const std::string &odbcIntervalToken) {
    for (const RS_MAP_INTERVAL_NAME *m = s_intervalMappings;
         m && m->pszOdbcIntervalName; ++m) {
        if (_stricmp(m->pszOdbcIntervalName, odbcIntervalToken.c_str()) == 0) {
            return m->pszPadbDatePartName;
        }
    }
    return nullptr;
}

const char *
ODBCEscapeClauseProcessor::lookupSqlType(const std::string &odbcTypeName) {
    for (const RS_MAP_SQL_TYPE_NAME *m = s_typeMappings;
         m && m->pszOdbcSQLTypeName; ++m) {
        if (_stricmp(m->pszOdbcSQLTypeName, odbcTypeName.c_str()) == 0) {
            return m->pszPadbSQLTypeName;
        }
    }
    return nullptr;
}

int ODBCEscapeClauseProcessor::needToScanODBCEscapeClause(RS_STMT_INFO *pStmt) {
    return (pStmt == NULL || pStmt->pStmtAttr->iNoScan == SQL_NOSCAN_OFF);
}

unsigned char *
ODBCEscapeClauseProcessor::checkReplaceParamMarkerAndODBCEscapeClause(
    RS_STMT_INFO *pStmt, char *pData, size_t cbLen, RS_STR_BUF *pPaStrBuf,
    int iReplaceParamMarker) {
    unsigned char *szData = (unsigned char *)pData;

    resetPaStrBuf(pPaStrBuf);

    if ((pData != NULL) && (INT_LEN(cbLen) != SQL_NULL_DATA)) {
        int numOfParamMarkers =
            (iReplaceParamMarker) ? countParamMarkers(pData, cbLen) : 0;
        int numOfODBCEscapeClauses =
            countODBCEscapeClauses(pStmt, pData, cbLen);

        if (pStmt)
            setParamMarkerCount(pStmt, numOfParamMarkers);

        if (!numOfParamMarkers && !numOfODBCEscapeClauses) {
            if (INT_LEN(cbLen) == SQL_NTS) {
                if (pPaStrBuf)
                    pPaStrBuf->pBuf = pData;
            } else {
                if (cbLen > SHORT_STR_DATA) {
                    pPaStrBuf->pBuf = (char *)rs_malloc(cbLen + 1);
                    szData = (unsigned char *)(pPaStrBuf->pBuf);
                    pPaStrBuf->iAllocDataLen = (int)cbLen;
                } else {
                    pPaStrBuf->pBuf = pPaStrBuf->buf;
                    szData = (unsigned char *)(pPaStrBuf->pBuf);
                    pPaStrBuf->iAllocDataLen = 0;
                }

                memcpy(szData, pData, cbLen);
                szData[cbLen] = '\0';
            }
        } // No param marker
        else {
            szData = replaceParamMarkerAndODBCEscapeClause(
                pStmt, pData, cbLen, pPaStrBuf, numOfParamMarkers,
                numOfODBCEscapeClauses);
        } // Param marker found
    } else {
        szData = NULL;
    }

    return szData;
}

unsigned char *ODBCEscapeClauseProcessor::replaceParamMarkerAndODBCEscapeClause(
    RS_STMT_INFO *pStmt, char *pData, size_t cbLen, RS_STR_BUF *pPaStrBuf,
    int numOfParamMarkers, int numOfODBCEscapeClauses) {
    unsigned char *szData = (unsigned char *)pData;

    if (numOfParamMarkers > 0 || numOfODBCEscapeClauses > 0) {
        // Replace param marker
        int iLen;
        int iMaxParamMarkerLen;
        char szTemp[MAX_NUMBER_BUF_LEN];
        int i;
        int iQuote = 0;
        int iDoubleQuote = 0;
        int iComment = 0;
        int iSingleLineComment = 0;
        char *pSrc = pData;
        char *pDest, *pDestStart;
        int iParamNumber = 0;
        int iTemp;
        int buf_len;
        int parseError = FALSE; // Flag to track parsing errors

        resetPaStrBuf(pPaStrBuf);
        cbLen = (INT_LEN(cbLen) == SQL_NTS) ? strlen(pData) : cbLen;

        iLen = (int)cbLen;

        if (numOfParamMarkers) {
            snprintf(szTemp, sizeof(szTemp), "%d", numOfParamMarkers);
            iMaxParamMarkerLen = (int)(strlen(szTemp) + 1);

            iLen += (iMaxParamMarkerLen * numOfParamMarkers);
        }

        if (numOfODBCEscapeClauses) {
            iLen +=
                (MAX_ODBC_ESCAPE_CLAUSE_REPLACE_LEN * numOfODBCEscapeClauses);
        }

        if (iLen > SHORT_STR_DATA) {
            buf_len = iLen + 1;
            pPaStrBuf->pBuf = (char *)rs_malloc(iLen + 1);
            szData = (unsigned char *)(pPaStrBuf->pBuf);
            pPaStrBuf->iAllocDataLen = (int)iLen;
        } else {
            buf_len = sizeof(pPaStrBuf->buf);
            pPaStrBuf->pBuf = pPaStrBuf->buf;
            szData = (unsigned char *)(pPaStrBuf->pBuf);
            pPaStrBuf->iAllocDataLen = 0;
        }

        pDestStart = pDest = (char *)szData;

        for (i = 0; i < (int)cbLen && !parseError; i++, pSrc++, pDest++) {
            switch (*pSrc) {
            case PARAM_MARKER: {
                if (!iQuote && !iDoubleQuote && !iComment &&
                    !iSingleLineComment && numOfParamMarkers) {
                    int occupied = (pDest - pDestStart);

                    iTemp = snprintf(pDest, buf_len - occupied, "%c%d",
                                     DOLLAR_SIGN, ++iParamNumber);
                    pDest += (iTemp - 1); // -1 because we do ++ in the loop end
                } else
                    *pDest = *pSrc;

                break;
            }

            case ODBC_ESCAPE_CLAUSE_START_MARKER: {
                if (!iQuote && !iDoubleQuote && !iComment &&
                    !iSingleLineComment) {
                    int result = replaceODBCEscapeClause(
                        pStmt, &pDest, pDestStart, buf_len, &pSrc, cbLen, i,
                        numOfParamMarkers, iParamNumber, 0, parseError);
                    if (result < 0 || parseError) {
                        parseError = TRUE;
                        break;
                    }
                    i = result;
                } else
                    *pDest = *pSrc;

                break;
            }

            case SINGLE_QUOTE: {
                if (!iComment && !iSingleLineComment) {
                    int escapedQuote =
                        ((pSrc != pData) && (*(pSrc - 1) == '\\'));

                    if (!escapedQuote) {
                        if (iQuote)
                            iQuote--;
                        else
                            iQuote++;
                    }
                }

                *pDest = *pSrc;
                break;
            }

            case DOUBLE_QUOTE: {
                if (!iComment && !iSingleLineComment) {
                    if (iDoubleQuote)
                        iDoubleQuote--;
                    else
                        iDoubleQuote++;
                }

                *pDest = *pSrc;
                break;
            }

            case STAR: {
                if (!iQuote && !iDoubleQuote) {
                    if ((pSrc != pData) && (*(pSrc - 1) == SLASH))
                        iComment++;
                }

                *pDest = *pSrc;
                break;
            }

            case SLASH: {
                if (!iQuote && !iDoubleQuote) {
                    if ((pSrc != pData) && (*(pSrc - 1) == STAR) && iComment)
                        iComment--;
                }

                *pDest = *pSrc;
                break;
            }

            case DASH: // Single line comment
            {
                if (!iQuote && !iDoubleQuote) {
                    if ((pSrc != pData) && (*(pSrc - 1) == DASH))
                        iSingleLineComment++;
                }

                *pDest = *pSrc;
                break;
            }

            case NEW_LINE: {
                if (!iQuote && !iDoubleQuote) {
                    if (iSingleLineComment)
                        iSingleLineComment--;
                }

                *pDest = *pSrc;
                break;
            }

            default: {
                *pDest = *pSrc;
                break;
            }
            } // Switch
        } // Loop

        // If parsing error occurred, return original data unchanged
        if (parseError) {
            RS_LOG_ERROR(
                "RSUTIL",
                "ODBC escape clause parsing error - preserving original query");
            releasePaStrBuf(pPaStrBuf);
            pPaStrBuf->pBuf = pData;
            return (unsigned char *)pData;
        }
        *pDest = '\0';
    }
    RS_LOG_DEBUG("RSUTIL", "The final escape clause generated is: %s", szData);
    return szData;
}

int ODBCEscapeClauseProcessor::replaceODBCEscapeClause(
    RS_STMT_INFO *pStmt, char **ppDest, char *pDestStart, int iDestBufLen,
    char **ppSrc, size_t cbLen, int srcPos, int numOfParamMarkers,
    int &paramNumber, int depth, int &parseError) {
    char *pDest = *ppDest;
    char *pSrc = *ppSrc;
    int scanForODBCEscapeClause = needToScanODBCEscapeClause(pStmt);

    if (scanForODBCEscapeClause) {
        int iODBCEscapeClauseEndMarkerFound = FALSE;
        int iODBCEscapeClauseSupportedKeyFound = TRUE;
        int iSavSrcPos = srcPos;
        char *pToken;
        int iTokenLen;
        int iTemp;
        int iScalarFunction = FALSE;
        int iCallEscapeClause = FALSE;
        int iParenthesis = FALSE;
        int occupied = (pDest - pDestStart);
        char *pDestBegin = pDest;

        // Check for invalid or maximum nesting depth to prevent stack overflow
        if (depth < 0 || depth >= MAX_ESCAPE_CLAUSE_NESTING_DEPTH) {
            RS_LOG_ERROR(
                "RSUTIL",
                "Maximum ODBC escape clause nesting depth (%d) exceeded",
                MAX_ESCAPE_CLAUSE_NESTING_DEPTH);
            if (pStmt) {
                std::string errMsg =
                    "ODBC escape clause nesting depth exceeds maximum limit "
                    "of " +
                    std::to_string(MAX_ESCAPE_CLAUSE_NESTING_DEPTH);
                addError(&pStmt->pErrorList, "HY000",
                         const_cast<char *>(errMsg.data()), 0, NULL);
            }
            parseError = TRUE;
            return -1;
        }
        // Skip '{'
        pSrc++;
        srcPos++;

        pToken = getNextTokenForODBCEscapeClause(&pSrc, cbLen, &srcPos, NULL);

        if (pToken != pSrc) {
            iTokenLen = (int)(pSrc - pToken);
            occupied = (pDest - pDestStart);

            if (iTokenLen == strlen("escape") &&
                _strnicmp(pToken, "escape", iTokenLen) == 0) {
                // LIKE escape char
                iTemp = snprintf(pDest, iDestBufLen - occupied, "%s", "ESCAPE");
                pDest += iTemp;
            } else if (iTokenLen == strlen("d") &&
                       _strnicmp(pToken, "d", iTokenLen) == 0) {
                iTemp = snprintf(pDest, iDestBufLen - occupied, "%s", "DATE");
                pDest += iTemp;
            } else if (iTokenLen == strlen("ts") &&
                       _strnicmp(pToken, "ts", iTokenLen) == 0) {
                iTemp =
                    snprintf(pDest, iDestBufLen - occupied, "%s", "TIMESTAMP");
                pDest += iTemp;
            } else if (iTokenLen == strlen("ivl") &&
                       _strnicmp(pToken, "ivl", iTokenLen) == 0) {
                iTemp =
                    snprintf(pDest, iDestBufLen - occupied, "%s", "INTERVAL");
                pDest += iTemp;
            } else if (iTokenLen == strlen("ym") &&
                       _strnicmp(pToken, "ym", iTokenLen) == 0) {
                iTemp = snprintf(pDest, iDestBufLen - occupied, "%s",
                                 "YEAR TO MONTH");
                pDest += iTemp;
            } else if (iTokenLen == strlen("ds") &&
                       _strnicmp(pToken, "ds", iTokenLen) == 0) {
                iTemp = snprintf(pDest, iDestBufLen - occupied, "%s",
                                 "DAY TO SECOND");
                pDest += iTemp;
            } else if (iTokenLen == strlen("t") &&
                       _strnicmp(pToken, "t", iTokenLen) == 0) {
                iTemp = snprintf(pDest, iDestBufLen - occupied, "%s", "TIME");
                pDest += iTemp;
            } else if (iTokenLen == strlen("call") &&
                       _strnicmp(pToken, "call", iTokenLen) == 0) {
                iTemp = snprintf(pDest, iDestBufLen - occupied, "%s", "CALL ");
                pDest += iTemp;
                if (pStmt)
                    pStmt->iFunctionCall = TRUE;
                iCallEscapeClause = TRUE;
            } else if (iTokenLen == strlen("oj") &&
                       _strnicmp(pToken, "oj", iTokenLen) == 0) {
                // Do nothing just skip "oj"
            } else if (iTokenLen == strlen("fn") &&
                       _strnicmp(pToken, "fn", iTokenLen) == 0) {
                iScalarFunction = TRUE;

                // Use helper function to modify the fn escape clause
                int result = processEscapeFunctionCall(
                    pStmt, &pDest, pDestStart, iDestBufLen, &pSrc, cbLen,
                    srcPos, numOfParamMarkers, paramNumber, pToken, iTokenLen,
                    depth, parseError);

                // Check if unknown function was detected (sentinel value -1)
                if (result == -1) {
                    parseError = TRUE;
                    RS_LOG_DEBUG("RSUTIL",
                                 "Unknown ODBC function detected - will "
                                 "preserve original escape clause");
                } else {
                    // Known function was transformed successfully
                    srcPos = result;
                }
            } else {
                iODBCEscapeClauseSupportedKeyFound = FALSE;
            }
        }

        if (iODBCEscapeClauseSupportedKeyFound && !parseError) {
            int iQuote = 0;
            int iDoubleQuote = 0;
            int iComment = 0;
            int iSingleLineComment = 0;

            // Copy upto '}'
            for (; srcPos < (int)cbLen; srcPos++) {
                if (*pSrc == ODBC_ESCAPE_CLAUSE_END_MARKER && !iQuote &&
                    !iDoubleQuote && !iComment && !iSingleLineComment) {
                    if (iCallEscapeClause == TRUE) {
                        if (iParenthesis == FALSE) {
                            occupied = (pDest - pDestStart);
                            iTemp = snprintf(pDest, iDestBufLen - occupied,
                                             "%s", "()");
                            pDest += iTemp;
                        }

                        iCallEscapeClause = FALSE;
                    }

                    if (iParenthesis == TRUE)
                        iParenthesis = FALSE;

                    iODBCEscapeClauseEndMarkerFound = TRUE;
                    break;
                } else if (*pSrc == ODBC_ESCAPE_CLAUSE_START_MARKER &&
                           iScalarFunction && !iQuote && !iDoubleQuote &&
                           !iComment && !iSingleLineComment) {
                    int result = replaceODBCEscapeClause(
                        pStmt, &pDest, pDestStart, iDestBufLen, &pSrc, cbLen,
                        srcPos, numOfParamMarkers, paramNumber, depth + 1,
                        parseError);

                    if (result < 0 || parseError) {
                        parseError = TRUE;
                        return -1;
                    }
                    srcPos = result;

                    // Skip '}'
                    pDest++;
                    pSrc++;
                    continue;
                } else if (*pSrc == '(' && !iQuote && !iDoubleQuote &&
                           !iComment && !iSingleLineComment) {
                    iParenthesis = TRUE;
                    *pDest++ = *pSrc++;
                } else {
                    switch (*pSrc) {
                    case PARAM_MARKER: {
                        if (!iQuote && !iDoubleQuote && !iComment &&
                            !iSingleLineComment && numOfParamMarkers) {
                            occupied = (pDest - pDestStart);
                            paramNumber++;
                            iTemp = snprintf(pDest, iDestBufLen - occupied,
                                             "%c%d", DOLLAR_SIGN, paramNumber);
                            pDest += iTemp;
                            pSrc++;
                        } else
                            *pDest++ = *pSrc++;

                        break;
                    }

                    case SINGLE_QUOTE: {
                        int escapedQuote =
                            ((pSrc != *ppSrc) && (*(pSrc - 1) == '\\'));

                        if (!escapedQuote) {
                            if (iQuote)
                                iQuote--;
                            else
                                iQuote++;
                        }

                        *pDest++ = *pSrc++;
                        break;
                    }

                    case DOUBLE_QUOTE: {
                        if (iDoubleQuote)
                            iDoubleQuote--;
                        else
                            iDoubleQuote++;

                        *pDest++ = *pSrc++;
                        break;
                    }

                    case STAR: {
                        if ((pSrc != *ppSrc) && (*(pSrc - 1) == SLASH))
                            iComment++;

                        *pDest++ = *pSrc++;
                        break;
                    }

                    case SLASH: {
                        if ((pSrc != *ppSrc) && (*(pSrc - 1) == STAR) &&
                            iComment)
                            iComment--;

                        *pDest++ = *pSrc++;
                        break;
                    }

                    case DASH: // Single line comment
                    {
                        if ((pSrc != *ppSrc) && (*(pSrc - 1) == DASH))
                            iSingleLineComment++;

                        *pDest++ = *pSrc++;
                        break;
                    }

                    case NEW_LINE: {
                        if (iSingleLineComment)
                            iSingleLineComment--;

                        *pDest++ = *pSrc++;
                        break;
                    }

                    case 'y': {
                        if (*(pSrc + 1) == 'm') {
                            iTemp = snprintf(pDest, iDestBufLen - occupied,
                                             "%s", "YEAR TO MONTH");
                            pDest += iTemp;
                            pSrc += 2;
                            break;
                        }
                        // Fallthrough
                    }
                    case 'd': {
                        if (*(pSrc + 1) == 's') {
                            iTemp = snprintf(pDest, iDestBufLen - occupied,
                                             "%s", "DAY TO SECOND");
                            pDest += iTemp;
                            pSrc += 2;
                            break;
                        }
                        // Fallthrough
                    }

                    default: {
                        *pDest++ = *pSrc++;
                        break;
                    }
                    } // Switch
                }
            } // Loop
        }

        if (iODBCEscapeClauseSupportedKeyFound &&
            iODBCEscapeClauseEndMarkerFound) {
            *ppDest = pDest - 1; // -1 because we do pDest++ in the caller loop
            *ppSrc = pSrc;
        } else {
            // Unsupported or malformed escape clause - signal error
            parseError = TRUE;
            RS_LOG_ERROR("RSUTIL",
                         "Unsupported or malformed ODBC escape clause detected "
                         "at position %d, iODBCEscapeClauseSupportedKeyFound = "
                         "%d, iODBCEscapeClauseEndMarkerFound =%d\n",
                         srcPos, iODBCEscapeClauseSupportedKeyFound,
                         iODBCEscapeClauseEndMarkerFound);
            return -1;
        }
    } else {
        // Put back the {
        *pDest = *pSrc;
    }

    return srcPos;
}

bool ODBCEscapeClauseProcessor::parseFunctionArguments(
    const std::string &sql, size_t &i, std::vector<std::string> &out,
    RS_STMT_INFO *pStmt, int numOfParamMarkers, int &paramNumber, int depth) {

    RS_LOG_TRACE("RSUTIL",
                 "parseFunctionArguments: Starting at position %zu, sql "
                 "length=%zu, depth=%d",
                 i, sql.length(), depth);

    // Initialize parser context with SQL string, current position, and output
    // vector
    ParserContext ctx(sql, i, out);

    // Define utility functions for argument management during parsing

    // Helper: start a new argument - adds empty string to arguments vector
    auto startNewArgument = [](ParserContext &ctx) {
        ctx.arguments.emplace_back("");
    };

    // Helper: finalize current argument - trims whitespace and removes empty
    // arguments
    auto finalizeArgument = [](ParserContext &ctx) {
        if (!ctx.arguments.empty()) {
            std::string &lastArg = ctx.arguments.back();
            // Trim all types of whitespace from both ends
            lastArg.erase(0, lastArg.find_first_not_of(" \t\n\r\f\v"));
            lastArg.erase(lastArg.find_last_not_of(" \t\n\r\f\v") + 1);

            // Remove empty arguments to avoid returning blank strings
            if (lastArg.empty()) {
                ctx.arguments.pop_back();
            }
        }
    };

    // Validate function has opening parenthesis and initialize first argument

    // Skip initial whitespace and find opening parenthesis
    ctx.pos = skipWhitespace(ctx.sql, ctx.pos);
    // Check if we've unexpectedly reached end of string
    if (ctx.pos >= ctx.length) {
        RS_LOG_ERROR(
            "RSUTIL",
            "parseFunctionArguments: Unexpected end of string at position %zu",
            ctx.pos);
        return false; // ERROR - malformed escape clause
    }

    if (ctx.sql[ctx.pos] != '(') {
        RS_LOG_ERROR("RSUTIL",
                     "parseFunctionArguments: Expected '(' but found '%c' at "
                     "position %zu",
                     ctx.sql[ctx.pos], ctx.pos);
        return false; // ERROR - malformed function call
    }

    RS_LOG_TRACE(
        "RSUTIL",
        "parseFunctionArguments: Found opening parenthesis at position %zu",
        ctx.pos);
    ctx.pos++;             // consume '(' - advance past opening parenthesis
    startNewArgument(ctx); // Initialize first argument container

    // Main parsing loop: process each character based on current parser state
    // States: NORMAL, IN_SINGLE_QUOTE, IN_DOUBLE_QUOTE, IN_SINGLE_LINE_COMMENT,
    // IN_MULTI_LINE_COMMENT
    while (ctx.pos < ctx.length) {
        char c = ctx.sql[ctx.pos];
        char next = (ctx.pos + 1 < ctx.length) ? ctx.sql[ctx.pos + 1] : '\0';

        switch (ctx.state) {
        // NORMAL STATE: Default parsing state
        // Handles most characters and state transitions to quoted strings,
        // comments, nested structures, argument separators, and parameter
        // markers
        case ParseState::NORMAL:
            if (c == '\'') {
                ctx.state = ParseState::IN_SINGLE_QUOTE;
                if (!ctx.arguments.empty()) {
                    ctx.arguments.back() += c;
                }
                ctx.pos++;
            } else if (c == '"') {
                ctx.state = ParseState::IN_DOUBLE_QUOTE;
                if (!ctx.arguments.empty()) {
                    ctx.arguments.back() += c;
                }
                ctx.pos++;
            } else if (c == '-' && next == '-') {
                ctx.state = ParseState::IN_SINGLE_LINE_COMMENT;
                ctx.pos += 2;
            } else if (c == '/' && next == '*') {
                ctx.state = ParseState::IN_MULTI_LINE_COMMENT;
                ctx.pos += 2;
            } else if (c == '(') {
                ctx.parenthesesDepth++;
                if (!ctx.arguments.empty()) {
                    ctx.arguments.back() += c;
                }
                ctx.pos++;
            } else if (c == '{') {
                // Handle nested escape sequences at top level
                if (ctx.braceDepth == 0 && ctx.parenthesesDepth == 0 &&
                    pStmt != nullptr) {
                    RS_LOG_TRACE("RSUTIL",
                                 "parseFunctionArguments: Processing nested "
                                 "escape sequence at position %zu",
                                 ctx.pos);
                    std::vector<char> tempBuffer(
                        SHORT_STR_DATA); // Temporary buffer for
                                         // processing
                    char *pSrc = const_cast<char *>(ctx.sql.c_str() + ctx.pos);
                    char *pDest = tempBuffer.data();
                    // Call the escape clause processor with current pos as 0,
                    // depth passed from caller
                    int tempParseError = FALSE;
                    int tempParamNumber = paramNumber;
                    int newPos = replaceODBCEscapeClause(
                        pStmt, &pDest, tempBuffer.data(), tempBuffer.size(),
                        &pSrc, ctx.length - ctx.pos, 0, numOfParamMarkers,
                        tempParamNumber, depth + 1, tempParseError);
                    paramNumber = tempParamNumber;
                    if (newPos > 0 && !tempParseError) {
                        // Advance pDest past last processed character and
                        // null-terminate replaceODBCEscapeClause returns new
                        // source position and updates pDest
                        pDest++;
                        *pDest = '\0';
                        if (!ctx.arguments.empty()) {
                            ctx.arguments.back() += std::string(tempBuffer.data());
                        }
                        // Update position - if pSrc points AT the '}', we need
                        // to skip it
                        if (pSrc && *pSrc == '}') {
                            ctx.pos = pSrc - ctx.sql.c_str() + 1;
                        }
                        RS_LOG_TRACE("RSUTIL",
                                     "parseFunctionArguments: Nested escape "
                                     "processed, new position=%zu, result='%s'",
                                     ctx.pos, tempBuffer.data());
                    } else {
                        // Processing failed, treat as regular brace
                        RS_LOG_TRACE("RSUTIL",
                                     "parseFunctionArguments: Nested escape "
                                     "processing failed at position %zu",
                                     ctx.pos);
                        return false;
                    }
                } else {
                    // Nested brace or no processing context, treat as regular
                    // character
                    ctx.braceDepth++;
                    if (!ctx.arguments.empty()) {
                        ctx.arguments.back() += c;
                    }
                    ctx.pos++;
                }
            } else if (c == ')') {
                if (ctx.parenthesesDepth == 0) {
                    // End of parameter list
                    finalizeArgument(ctx);
                    ctx.pos++; // consume ')'
                    i = ctx.pos;
                    RS_LOG_TRACE("RSUTIL",
                                 "parseFunctionArguments: Parsing complete, "
                                 "found %zu arguments",
                                 ctx.arguments.size());
                    return true;
                } else {
                    ctx.parenthesesDepth--;
                    if (!ctx.arguments.empty()) {
                        ctx.arguments.back() += c;
                    }
                    ctx.pos++;
                }
            } else if (c == '}') {
                if (ctx.braceDepth <= 0) {
                    // Unmatched closing brace - malformed input
                    RS_LOG_TRACE(
                        "RSUTIL",
                        "parseFunctionArguments: Unmatched '}' at position %zu",
                        ctx.pos);
                    return false;
                }
                ctx.braceDepth--;
                if (!ctx.arguments.empty()) {
                    ctx.arguments.back() += c;
                }
                ctx.pos++;
            } else if (c == ',' && ctx.parenthesesDepth == 0 &&
                       ctx.braceDepth == 0) {
                // Argument separator at top level
                finalizeArgument(ctx);
                RS_LOG_TRACE("RSUTIL",
                             "parseFunctionArguments: Found argument separator "
                             "at position %zu, starting new argument",
                             ctx.pos);
                ctx.pos++; // consume ','
                startNewArgument(ctx);
            } else if (c == '?' && numOfParamMarkers > 0) {
                // Parameter marker - convert to $n format
                paramNumber++;
                std::string paramMarker = "$" + std::to_string(paramNumber);
                if (!ctx.arguments.empty()) {
                    ctx.arguments.back() += paramMarker;
                }
                ctx.pos++;
            } else {
                if (!ctx.arguments.empty()) {
                    ctx.arguments.back() += c;
                }
                ctx.pos++;
            }
            break;
        // IN_SINGLE_QUOTE STATE: Handles character data within single quotes
        // Manages escaped quotes (doubled single quotes) and string termination
        case ParseState::IN_SINGLE_QUOTE:
            if (!ctx.arguments.empty()) {
                ctx.arguments.back() += c;
            }
            if ((c == '\\' && next == '\'') || (c == '\'' && next == '\'')) {
                // Handle both backslash-escaped and doubled quotes
                if (!ctx.arguments.empty()) {
                    ctx.arguments.back() += next;
                }
                ctx.pos += 2;
            } else if (c == '\'') {
                // End of string
                RS_LOG_TRACE("RSUTIL",
                             "parseFunctionArguments: Exiting quoted string at "
                             "position %zu",
                             ctx.pos);
                ctx.state = ParseState::AFTER_STRING;
                ctx.pos++;
            } else {
                ctx.pos++;
            }
            break;
        // IN_DOUBLE_QUOTE STATE: Handles identifiers within double quotes
        // Manages escaped quotes (doubled double quotes) and identifier
        // termination
        case ParseState::IN_DOUBLE_QUOTE:
            if (!ctx.arguments.empty()) {
                ctx.arguments.back() += c;
            }
            if (c == '"') {
                if (next == '"') {
                    // Escaped double quote (doubled quote)
                    if (!ctx.arguments.empty()) {
                        ctx.arguments.back() += next;
                    }
                    ctx.pos += 2;
                } else {
                    // End of quoted identifier - transition to AFTER_IDENTIFIER
                    // state
                    ctx.state = ParseState::AFTER_IDENTIFIER;
                    ctx.pos++;
                }
            } else {
                ctx.pos++;
            }
            break;
        // IN_SINGLE_LINE_COMMENT STATE: Handles SQL single-line comments (--)
        // Ignores all characters until newline or carriage return is
        // encountered
        case ParseState::IN_SINGLE_LINE_COMMENT:
            if (c == '\n' || c == '\r') {
                ctx.state = ParseState::NORMAL;
            }
            ctx.pos++;
            break;
        // IN_MULTI_LINE_COMMENT STATE: Handles SQL multi-line comments (/* ...
        // */) Ignores all characters until comment terminator (*/) is found
        case ParseState::IN_MULTI_LINE_COMMENT:
            if (c == '*' && next == '/') {
                ctx.state = ParseState::NORMAL;
                ctx.pos += 2;
            } else {
                ctx.pos++;
            }
            break;
        // AFTER_STRING and AFTER_IDENTIFIER STATES: Post-token validation
        // states Validates proper token separation after completing string
        // literals or identifiers Ensures tokens are properly separated by
        // allowed operators/punctuation
        case ParseState::AFTER_STRING:
        case ParseState::AFTER_IDENTIFIER:
            ctx.state = ParseState::NORMAL;
            // Don't increment pos here, let NORMAL state handle the character
            break;
        default:
            // Invalid parser state. Shouldn't reach here
            RS_LOG_TRACE("RSUTIL",
                         "parseFunctionArguments: Invalid parser state %d at "
                         "position %zu",
                         (int)ctx.state, ctx.pos);
            return false;
        }
    }
    RS_LOG_TRACE("RSUTIL",
                 "parseFunctionArguments: Closing ')' not found at position "
                 "%zu (depth=%d, parsed %zu args)",
                 ctx.pos, depth, ctx.arguments.size());
    // If we exit the loop without finding ')', parsing has failed
    return false;
}

int ODBCEscapeClauseProcessor::processEscapeFunctionCall(
    RS_STMT_INFO *pStmt, char **ppDest, char *pDestStart, int iDestBufLen,
    char **ppSrc, size_t cbLen, int i, int numOfParamMarkers, int &paramNumber,
    char *pToken, int iTokenLen, int depth, int &parseError) {
    char *pDest = *ppDest;
    char *pSrc = *ppSrc;

    // Save original position for rollback in case of parsing failure
    int originalI = i;

    // Extract the ODBC function name from escape sequence (e.g., "UCASE" from
    // "{fn UCASE(...)}")
    char fnNameDelimiterList[3] = {'(', '}', '\0'};
    pToken = getNextTokenForODBCEscapeClause(&pSrc, cbLen, &i,
                                             (char *)fnNameDelimiterList);

    if (pToken == pSrc) {
        return i; // No function name found, return unchanged
    }

    // Convert function name to string and normalize to uppercase for comparison
    iTokenLen = (int)(pSrc - pToken);
    std::string functionName(pToken, iTokenLen);
    std::transform(functionName.begin(), functionName.end(),
                   functionName.begin(), ::toupper);
    const std::string &FN = functionName;
    int newI = i;
    // Parse function arguments handling nested escape sequences and parameter
    // markers
    std::vector<std::string> arguments;
    std::string sqlToParse(pSrc, cbLen - i);
    size_t localPos = 0;
    if (FN != "CURRENT_TIME" && FN != "CURRENT_TIMESTAMP") {
        // These SQL standard functions have no parentheses
        // Don't call parseFunctionArguments - leave arguments empty
        bool parseSuccess =
            parseFunctionArguments(sqlToParse, localPos, arguments, pStmt,
                                   numOfParamMarkers, paramNumber, depth);

        if (!parseSuccess) {
            // Parsing failed - signal error and return to preserve original
            // escape clause
            parseError = TRUE;
            RS_LOG_DEBUG("RSUTIL", "Function argument parsing failed - will "
                                   "preserve original escape clause");
            return -1;
        }
        // Update source buffer position after successful argument parsing
        newI = i + localPos;
        pSrc += localPos;
    }

    // Setup variables for building Redshift-native function call
    std::string result;
    const size_t argumentsSize = arguments.size();

    // Helper: Access function argument by index
    auto getArg = [](const std::vector<std::string> &arguments,
                     size_t i) -> const std::string & { return arguments[i]; };

    // Helper: Validate argument count is within expected range
    auto isArgCountInRange = [](size_t argCount, size_t lo, size_t hi) -> bool {
        return (argCount >= lo && argCount <= hi);
    };

    // Helper: Build standard function call (used when args don't match or
    // function unsupported)
    auto buildStandardFunction =
        [](std::string &outResult, const std::string &funcName,
           const std::vector<std::string> &args, size_t argCount) {
            outResult += funcName + "(";
            for (size_t i = 0; i < argCount; ++i) {
                if (i > 0)
                    outResult += ", ";
                outResult += args[i];
            }
            outResult += ")";
        };
    // Helper: for type casting
    auto cast_as = [](const std::string &expr,
                      const std::string &type) -> std::string {
        return "CAST(" + expr + " AS " + type + ")";
    };

    // Map ODBC function names to Redshift-native equivalents
    if (FN == "LCASE" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "LOWER(" + getArg(arguments, 0) + ")";
    } else if (FN == "UCASE" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "UPPER(" + getArg(arguments, 0) + ")";
    } else if (FN == "LOG" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "LN(" + getArg(arguments, 0) + ")";
    } else if (FN == "LOG10" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "LOG(10," + getArg(arguments, 0) + ")";
    } else if (FN == "RAND") {
        result += "RANDOM()";
    } else if (FN == "TRUNCATE" && isArgCountInRange(argumentsSize, 1, 2)) {
        result += "TRUNC(" + getArg(arguments, 0);
        if (argumentsSize == 2)
            result += "," + getArg(arguments, 1);
        result += ")";
    } else if (FN == "CURRENT_DATE" || FN == "CURDATE") {
        result += "GETDATE()";
    } else if (FN == "CURTIME") {
        result += "LOCALTIME";
    } else if (FN == "DATABASE") {
        result += "CURRENT_DATABASE()";
    } else if (FN == "IFNULL" && isArgCountInRange(argumentsSize, 2, 2)) {
        result += "COALESCE(" + getArg(arguments, 0) + "," +
                  getArg(arguments, 1) + ")";
    } else if (FN == "USER") {
        result += "USER";
    } else if (FN == "CURRENT_TIME") {
        // CURRENT_TIME has no parentheses in SQL standard
        result += "CURRENT_TIME";
    } else if (FN == "CURRENT_TIMESTAMP") {
        // CURRENT_TIMESTAMP has no parentheses in SQL standard
        result += "CURRENT_TIMESTAMP";
    } else if (FN == "NOW") {
        result += "NOW()";
    }
    // Mathematical functions
    else if (FN == "ACOS" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "ACOS(" + getArg(arguments, 0) + ")";
    } else if (FN == "ASIN" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "ASIN(" + getArg(arguments, 0) + ")";
    } else if (FN == "ATAN" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "ATAN(" + getArg(arguments, 0) + ")";
    } else if (FN == "ATAN2" && isArgCountInRange(argumentsSize, 2, 2)) {
        result +=
            "ATAN2(" + getArg(arguments, 0) + "," + getArg(arguments, 1) + ")";
    } else if (FN == "COS" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "COS(" + getArg(arguments, 0) + ")";
    } else if (FN == "COT" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "COT(" + getArg(arguments, 0) + ")";
    } else if (FN == "SIN" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "SIN(" + getArg(arguments, 0) + ")";
    } else if (FN == "TAN" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "TAN(" + getArg(arguments, 0) + ")";
    } else if (FN == "DEGREES" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "DEGREES(" + getArg(arguments, 0) + ")";
    } else if (FN == "RADIANS" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "RADIANS(" + getArg(arguments, 0) + ")";
    } else if (FN == "EXP" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "EXP(" + getArg(arguments, 0) + ")";
    } else if (FN == "SIGN" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "SIGN(" + getArg(arguments, 0) + ")";
    } else if (FN == "PI") {
        result += "PI()";
    } else if (FN == "ABS" && isArgCountInRange(argumentsSize, 1, 1)) {
        result +=
            "ABS(" + cast_as(getArg(arguments, 0), "DOUBLE PRECISION") + ")";
    } else if (FN == "CEILING" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "CEIL(" + getArg(arguments, 0) + ")";
    } else if (FN == "FLOOR" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "FLOOR(" + getArg(arguments, 0) + ")";
    } else if (FN == "MOD" && isArgCountInRange(argumentsSize, 2, 2)) {
        result += "MOD(" + cast_as(getArg(arguments, 0), "NUMERIC") + "," +
                  cast_as(getArg(arguments, 1), "NUMERIC") + ")";
    } else if (FN == "POWER" && isArgCountInRange(argumentsSize, 2, 2)) {
        result +=
            "POWER(" + getArg(arguments, 0) + "," + getArg(arguments, 1) + ")";
    } else if (FN == "ROUND" && isArgCountInRange(argumentsSize, 1, 2)) {
        if (argumentsSize == 1) {
            result += "ROUND(" + getArg(arguments, 0) + ")";
        } else {
            result += "ROUND(" + getArg(arguments, 0) + "," +
                      getArg(arguments, 1) + ")";
        }
    } else if (FN == "SQRT" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "SQRT(" + getArg(arguments, 0) + ")";
    }
    // DATE_PART wrappers
    else if (FN == "DAYOFMONTH" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "DATE_PART('d', " +
                  cast_as(getArg(arguments, 0), "TIMESTAMP") + ")";
    } else if (FN == "DAYOFWEEK" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "DATE_PART('dow', " +
                  cast_as(getArg(arguments, 0), "TIMESTAMP") + ") + 1";
    } else if (FN == "DAYOFYEAR" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "DATE_PART('doy', " +
                  cast_as(getArg(arguments, 0), "TIMESTAMP") + ")";
    } else if (FN == "HOUR" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "DATE_PART('h', " +
                  cast_as(getArg(arguments, 0), "TIMESTAMP") + ")";
    } else if (FN == "MINUTE" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "DATE_PART('m', " +
                  cast_as(getArg(arguments, 0), "TIMESTAMP") + ")";
    } else if (FN == "MONTH" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "DATE_PART('mon', " +
                  cast_as(getArg(arguments, 0), "TIMESTAMP") + ")";
    } else if (FN == "QUARTER" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "DATE_PART('qtr', " +
                  cast_as(getArg(arguments, 0), "TIMESTAMP") + ")";
    } else if (FN == "SECOND" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "DATE_PART('s', " +
                  cast_as(getArg(arguments, 0), "TIMESTAMP") + ")";
    } else if (FN == "WEEK" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "DATE_PART('w', " +
                  cast_as(getArg(arguments, 0), "TIMESTAMP") + ")";
    } else if (FN == "YEAR" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "DATE_PART('y', " +
                  cast_as(getArg(arguments, 0), "TIMESTAMP") + ")";
    }
    // Type conversion
    else if (FN == "CONVERT" && isArgCountInRange(argumentsSize, 2, 2)) {
        const char *mapped = lookupSqlType(getArg(arguments, 1));
        if (!mapped) {
            buildStandardFunction(result, FN, arguments, argumentsSize);
        } else {
            result += "CAST(" + getArg(arguments, 0) + " AS " + mapped + ")";
        }
    }
    // Timestamp functions
    else if (FN == "TIMESTAMPADD" && isArgCountInRange(argumentsSize, 3, 3)) {
        const char *part = lookupIntervalToken(getArg(arguments, 0));
        if (!part) {
            buildStandardFunction(result, FN, arguments, argumentsSize);
        } else {
            result += "DATEADD(" + std::string(part) + ", " +
                      cast_as(getArg(arguments, 1), "INTEGER") + ", " +
                      cast_as(getArg(arguments, 2), "TIMESTAMP") + ")";
        }
    } else if (FN == "TIMESTAMPDIFF" &&
               isArgCountInRange(argumentsSize, 3, 3)) {
        const char *part = lookupIntervalToken(getArg(arguments, 0));
        if (!part) {
            buildStandardFunction(result, FN, arguments, argumentsSize);
        } else {
            result += "DATEDIFF(" + std::string(part) + ", " +
                      cast_as(getArg(arguments, 1), "TIMESTAMP") + ", " +
                      cast_as(getArg(arguments, 2), "TIMESTAMP") + ")";
        }
    }
    // String functions
    else if (FN == "LOCATE" && isArgCountInRange(argumentsSize, 2, 3)) {
        if (argumentsSize == 2) {
            result += "POSITION(" + getArg(arguments, 0) + " IN " +
                      getArg(arguments, 1) + ")";
        } else {
            result += "POSITION(" + getArg(arguments, 0) + " IN SUBSTRING(" +
                      getArg(arguments, 1) + " FROM " +
                      cast_as(getArg(arguments, 2), "INTEGER") + ")) + " +
                      cast_as(getArg(arguments, 2), "INTEGER") + " - 1";
        }
    } else if (FN == "INSERT" && isArgCountInRange(argumentsSize, 4, 4)) {
        result += "SUBSTRING(" + getArg(arguments, 0) + " FROM 1 FOR (" +
                  cast_as(getArg(arguments, 1), "INTEGER") + " - 1)) || " +
                  getArg(arguments, 3) + " || " + "SUBSTRING(" +
                  getArg(arguments, 0) + " FROM (" +
                  cast_as(getArg(arguments, 1), "INTEGER") + " + " +
                  cast_as(getArg(arguments, 2), "INTEGER") + "))";
    } else if (FN == "MONTHNAME" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "TRIM(TO_CHAR(" + cast_as(getArg(arguments, 0), "TIMESTAMP") +
                  ", 'Month'))";
    } else if (FN == "DAYNAME" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "TRIM(TO_CHAR(" + cast_as(getArg(arguments, 0), "TIMESTAMP") +
                  ", 'Day'))";
    } else if (FN == "SPACE" && isArgCountInRange(argumentsSize, 1, 1)) {
        result +=
            "REPEAT(' ', " + cast_as(getArg(arguments, 0), "INTEGER") + ")";
    } else if (FN == "SOUNDEX" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "SOUNDEX(" + getArg(arguments, 0) + ")";
    } else if (FN == "SUBSTRING" && isArgCountInRange(argumentsSize, 2, 3)) {
        if (argumentsSize == 2) {
            result += "SUBSTRING(" + getArg(arguments, 0) + " FROM " +
                      cast_as(getArg(arguments, 1), "INTEGER") + ")";
        } else {
            result += "SUBSTRING(" + getArg(arguments, 0) + " FROM " +
                      cast_as(getArg(arguments, 1), "INTEGER") + " FOR " +
                      cast_as(getArg(arguments, 2), "INTEGER") + ")";
        }
    } else if (FN == "CONCAT") {
        if (argumentsSize < 2) {
            buildStandardFunction(result, FN, arguments, argumentsSize);
        } else {
            result += getArg(arguments, 0);
            for (size_t i = 1; i < argumentsSize; ++i) {
                result += " || " + getArg(arguments, i);
            }
        }
    } else if (FN == "LEFT" && isArgCountInRange(argumentsSize, 2, 2)) {
        result += "SUBSTRING(" + getArg(arguments, 0) + " FROM 1 FOR " +
                  cast_as(getArg(arguments, 1), "INTEGER") + ")";
    } else if (FN == "RIGHT" && isArgCountInRange(argumentsSize, 2, 2)) {
        result += "SUBSTRING(" + getArg(arguments, 0) + " FROM (LENGTH(" +
                  getArg(arguments, 0) + ") - " +
                  cast_as(getArg(arguments, 1), "INTEGER") + " + 1))";
    } else if (FN == "LTRIM" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "LTRIM(" + getArg(arguments, 0) + ")";
    } else if (FN == "RTRIM" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "RTRIM(" + getArg(arguments, 0) + ")";
    } else if (FN == "LENGTH" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "LENGTH(" + getArg(arguments, 0) + ")";
    } else if (FN == "REPLACE" && isArgCountInRange(argumentsSize, 3, 3)) {
        result += "REPLACE(" + getArg(arguments, 0) + ", " +
                  getArg(arguments, 1) + ", " + getArg(arguments, 2) + ")";
    } else if (FN == "DIFFERENCE" && isArgCountInRange(argumentsSize, 2, 2)) {
        result += "DIFFERENCE(" + getArg(arguments, 0) + ", " +
                  getArg(arguments, 1) + ")";
    } else if (FN == "ASCII" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "ASCII(" + getArg(arguments, 0) + ")";
    } else if (FN == "BIT_LENGTH" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "BIT_LENGTH(" + getArg(arguments, 0) + ")";
    } else if (FN == "CHAR" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "CHR(" + getArg(arguments, 0) + ")";
    } else if (FN == "CHAR_LENGTH" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "CHAR_LENGTH(" + getArg(arguments, 0) + ")";
    } else if (FN == "CHARACTER_LENGTH" &&
               isArgCountInRange(argumentsSize, 1, 1)) {
        result += "CHARACTER_LENGTH(" + getArg(arguments, 0) + ")";
    } else if (FN == "OCTET_LENGTH" && isArgCountInRange(argumentsSize, 1, 1)) {
        result += "OCTET_LENGTH(" + getArg(arguments, 0) + ")";
    } else if (FN == "POSITION" && isArgCountInRange(argumentsSize, 2, 2)) {
        // POSITION(substring, string) -> POSITION(substring IN string)
        result += "POSITION(" + getArg(arguments, 0) + " IN " +
                  getArg(arguments, 1) + ")";
    } else if (FN == "REPEAT" && isArgCountInRange(argumentsSize, 2, 2)) {
        result +=
            "REPEAT(" + getArg(arguments, 0) + "," + getArg(arguments, 1) + ")";
    } else {
        // Fallback: Preserve the function call as-is when either:
        // 1. The ODBC function name is not recognized/supported by the driver,
        // OR
        // 2. The argument count doesn't match the expected signature for a
        // known function This allows applications to use native Redshift
        // functions directly within ODBC escape syntax
        buildStandardFunction(result, FN, arguments, argumentsSize);
    }

    // Validate destination buffer has enough space for the converted result
    int occupied = (pDest - pDestStart);
    size_t available = iDestBufLen - occupied;

    // Ensure we have space for result + null terminator
    if (result.length() + 1 <= available) {
        // Use snprintf for automatic bounds checking and null termination
        int written = snprintf(pDest, available, "%s", result.c_str());

        if (written >= 0 && (size_t)written < available) {
            // Success - snprintf wrote the string and null terminator
            *ppDest = pDest + written;
            *ppSrc = pSrc;
            RS_LOG_TRACE(
                "RSUTIL",
                "Query after modification from processEscapeFunctionCall = %s",
                pDest);
            return newI;
        } else {
            // Buffer overflow prevented
            parseError = TRUE;
            RS_LOG_ERROR("RSUTIL",
                         "Buffer overflow prevented: snprintf returned %d, "
                         "available space %zu",
                         written, available);
            return originalI;
        }
    } else {
        // Buffer overflow prevented - set error flag and rollback
        parseError = TRUE;
        RS_LOG_ERROR("RSUTIL",
                     "Buffer overflow prevented: result length %zu exceeds "
                     "available space %zu",
                     result.length(), available);
        return originalI;
    }
}

int ODBCEscapeClauseProcessor::countParamMarkers(char *pData, size_t cbLen) {
    int numOfParamMarkers = 0;

    if (pData) {
        char *pTemp;

        if (INT_LEN(cbLen) == SQL_NTS)
            cbLen = strlen(pData);

        // Is any parameter marker?
        pTemp = (char *)memchr(pData, PARAM_MARKER, cbLen);

        if (pTemp != NULL) {
            int iQuote = 0;
            int iDoubleQuote = 0;
            int iComment = 0;
            int iSingleLineComment = 0;
            int i;

            pTemp = pData;

            for (i = 0; i < (int)cbLen; i++, pTemp++) {
                switch (*pTemp) {
                case PARAM_MARKER: {
                    if (!iQuote && !iDoubleQuote && !iComment &&
                        !iSingleLineComment)
                        numOfParamMarkers++;

                    break;
                }

                case SINGLE_QUOTE: {
                    int escapedQuote =
                        ((pTemp != pData) && (*(pTemp - 1) == '\\'));

                    if (!escapedQuote) {
                        if (iQuote)
                            iQuote--;
                        else
                            iQuote++;
                    }

                    break;
                }

                case DOUBLE_QUOTE: {
                    if (iDoubleQuote)
                        iDoubleQuote--;
                    else
                        iDoubleQuote++;

                    break;
                }

                case STAR: {
                    if ((pTemp != pData) && (*(pTemp - 1) == SLASH))
                        iComment++;

                    break;
                }

                case SLASH: {
                    if ((pTemp != pData) && (*(pTemp - 1) == STAR) && iComment)
                        iComment--;

                    break;
                }

                case DASH: // Single line comment
                {
                    if ((pTemp != pData) && (*(pTemp - 1) == DASH))
                        iSingleLineComment++;

                    break;
                }

                case NEW_LINE: {
                    if (iSingleLineComment)
                        iSingleLineComment--;

                    break;
                }

                default: {
                    // Do nothing
                    break;
                }
                } // Switch
            } // Loop
        } // Any param marker?
    }

    return numOfParamMarkers;
}

int ODBCEscapeClauseProcessor::countODBCEscapeClauses(RS_STMT_INFO *pStmt,
                                                      char *pData,
                                                      size_t cbLen) {
    int numOfEscapeClauses = 0;
    int scanForODBCEscapeClause = needToScanODBCEscapeClause(pStmt);

    if (pData && scanForODBCEscapeClause) {
        char *pTemp = pData;
        int iOffset = 0;

        if (INT_LEN(cbLen) == SQL_NTS)
            cbLen = strlen(pData);

        while (pTemp != NULL) {
            pTemp =
                (char *)memchr(pData + iOffset, ODBC_ESCAPE_CLAUSE_START_MARKER,
                               cbLen - iOffset);
            if (pTemp) {
                numOfEscapeClauses++;
                iOffset = (int)((pTemp - pData) + 1);
                if (iOffset >= (int)cbLen)
                    break;
            }
        }
    }

    return numOfEscapeClauses;
}
