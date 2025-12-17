/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#pragma once

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#endif

#ifndef kSQLWCHAR_SCAN_CAP
// Default safety cap: 1,048,576 code units (~2 MB for UTF-16, ~4 MB for UTF-32).
#define kSQLWCHAR_SCAN_CAP 1024 * 1024
#endif

#include <sql.h>
#include <sqlext.h>
#include <stdlib.h>

#include "rsodbc.h"
#include "rsutil.h"
#include <string>
#include <cstddef>
#include <cstring>
#include <type_traits>
#include <atomic>
#include <mutex>

// ============================================================================
// UTF-16 ↔ UTF-8 Conversion Functions
// ============================================================================

/**
 * Convert UTF-16 to UTF-8 with buffer size limit.
 * 
 * @param wszStr Input UTF-16 string
 * @param cchLen Input length in code units, or SQL_NTS for null-terminated
 * @param szStr Output buffer for UTF-8 string
 * @param bufferSize Output buffer size in bytes (including space for null terminator)
 * @param totalCharsNeeded [optional] Total bytes needed for full conversion (excluding terminator)
 * @return Number of UTF-8 bytes written (excluding null terminator), 0 on error
 */
size_t wchar16_to_utf8_char(const SQLWCHAR *wszStr, int cchLen, char *szStr,
                            int bufferSize, size_t *totalCharsNeeded = nullptr);

/**
 * Convert UTF-16 to UTF-8 using std::string.
 * 
 * @param wszStr Input UTF-16 string
 * @param cchLen Input length in code units, or SQL_NTS for null-terminated
 * @param szStr Output std::string for UTF-8 result
 * @return Number of UTF-8 bytes in output, 0 on error
 */
size_t wchar16_to_utf8_str(const SQLWCHAR *wszStr, int cchLen,
                           std::string &szStr);

/**
 * Convert UTF-8 to UTF-16 using std::u16string.
 * 
 * @param szStr Input UTF-8 string
 * @param cchLen Input length in bytes, or SQL_NTS for null-terminated
 * @param wszStr Output std::u16string for UTF-16 result
 * @return Number of UTF-16 code units in output, 0 on error or invalid input
 */
size_t char_utf8_to_utf16_str(const char *szStr, int cchLen,
                              std::u16string &wszStr);

/**
 * Calculate UTF-16 length needed for UTF-8 string.
 * 
 * @param szStr Input UTF-8 string
 * @param cchLen Input length in bytes, or SQL_NTS for null-terminated
 * @return Number of UTF-16 code units needed, 0 on error or invalid input
 */
size_t char_utf8_to_utf16_strlen(const char* szStr, int cchLen);

/**
 * Convert UTF-8 to UTF-16 with buffer size limit.
 * 
 * @param szStr Input UTF-8 string
 * @param cchLen Input length in bytes, or SQL_NTS for null-terminated
 * @param wszStr Output buffer for UTF-16 string
 * @param bufferSize Output buffer size in code units (including space for null terminator)
 * @param totalNeeded [optional] Total code units needed for full conversion (excluding terminator)
 * @return Number of UTF-16 code units written (excluding null terminator), 0 on error
 */
size_t char_utf8_to_utf16_wchar(const char *szStr, int cchLen,
                                SQLWCHAR *wszStr, int bufferSize, size_t* totalNeeded = nullptr);

// ============================================================================
// UTF-32 ↔ UTF-8 Conversion Functions
// ============================================================================

/**
 * Convert UTF-32 to UTF-8 with buffer size limit.
 * 
 * @param wszStr Input UTF-32 string
 * @param cchLen Input length in code units, or SQL_NTS for null-terminated
 * @param szStr Output buffer for UTF-8 string
 * @param bufferSize Output buffer size in bytes (including space for null terminator)
 * @param totalCharsNeeded [optional] Total bytes needed for full conversion (excluding terminator)
 * @return Number of UTF-8 bytes written (excluding null terminator), 0 on error
 */
size_t wchar32_to_utf8_char(const SQLWCHAR *wszStr, int cchLen, char *szStr,
                            int bufferSize, size_t *totalCharsNeeded = nullptr);

/**
 * Calculate UTF-32 length needed for UTF-8 string.
 * 
 * @param szStr Input UTF-8 string
 * @param cchLen Input length in bytes, or SQL_NTS for null-terminated
 * @return Number of UTF-32 code units needed, 0 on error or invalid input
 */
size_t char_utf8_to_utf32_strlen(const char* szStr, int cchLen);

// ============================================================================
// Generic SQLWCHAR ↔ UTF-8 Functions (auto-detect UTF-16/UTF-32)
// ============================================================================

/**
 * Convert SQLWCHAR to UTF-8 using std::string (auto-detects UTF-16/UTF-32).
 * 
 * @param wszStr Input SQLWCHAR string
 * @param cchLen Input length in code units, or SQL_NTS for null-terminated
 * @param szStr Output std::string for UTF-8 result
 * @param unicodeType Unicode encoding (SQL_DD_CP_UTF16/UTF32, or -1 for auto-detect)
 * @return Number of UTF-8 bytes in output, 0 on error
 */
size_t sqlwchar_to_utf8_str(const SQLWCHAR *wszStr, int cchLen, std::string &szStr, int unicodeType = -1);

/**
 * Convert SQLWCHAR to UTF-8 with buffer size limit (auto-detects UTF-16/UTF-32).
 * 
 * @param wszStr Input SQLWCHAR string
 * @param cchLen Input length in code units, or SQL_NTS for null-terminated
 * @param szStr Output buffer for UTF-8 string
 * @param bufferSize Output buffer size in bytes (including space for null terminator)
 * @param totalCharsNeeded [optional] Total bytes needed for full conversion (excluding terminator)
 * @param unicodeType Unicode encoding (SQL_DD_CP_UTF16/UTF32, or -1 for auto-detect)
 * @return Number of UTF-8 bytes written (excluding null terminator), 0 on error
 */
size_t sqlwchar_to_utf8_char(const SQLWCHAR *wszStr, int cchLen, char *szStr, int bufferSize, size_t *totalCharsNeeded = nullptr, int unicodeType = -1);

/**
 * Convert UTF-8 to SQLWCHAR with buffer size limit (auto-detects UTF-16/UTF-32).
 * 
 * @param szStr Input UTF-8 string
 * @param cchLen Input length in bytes, or SQL_NTS for null-terminated
 * @param wszStr Output buffer for SQLWCHAR string
 * @param bufferSize Output buffer size in code units (including space for null terminator)
 * @param totalCharsNeeded [optional] Total code units needed for full conversion (excluding terminator)
 * @param unicodeType Unicode encoding (SQL_DD_CP_UTF16/UTF32, or -1 for auto-detect)
 * @return Number of code units written (excluding null terminator), 0 on error
 */
size_t utf8_to_sqlwchar_str(const char *szStr, int cchLen, SQLWCHAR *wszStr, int bufferSize, size_t *totalCharsNeeded = nullptr, int unicodeType = -1);

/**
 * Calculate SQLWCHAR length needed for UTF-8 string (auto-detects UTF-16/UTF-32).
 * 
 * @param szStr Input UTF-8 string
 * @param cchLen Input length in bytes, or SQL_NTS for null-terminated
 * @param unicodeType Unicode encoding (SQL_DD_CP_UTF16/UTF32, or -1 for auto-detect)
 * @return Number of code units needed, 0 on error or invalid input
 */
size_t utf8_to_sqlwchar_strlen(const char *szStr, int cchLen, int unicodeType = -1);

/**
 * Convert UTF-8 to SQLWCHAR with automatic memory allocation.
 * 
 * @param szStr Input UTF-8 string
 * @param cchLen Input length in bytes, or SQL_NTS for null-terminated
 * @param out_wchar Output pointer to allocated SQLWCHAR buffer (caller must free with rs_free)
 * @param unicodeType Unicode encoding (SQL_DD_CP_UTF16/UTF32, or -1 for auto-detect)
 * @return Number of code units in allocated buffer (excluding null terminator)
 * @return 0 if szStr is NULL, out_wchar is NULL, conversion fails, or allocation fails
 */
size_t utf8_to_sqlwchar_alloc(const char *szStr, int cchLen, SQLWCHAR **out_wchar, int unicodeType = -1);

/**
 * Convert SQLWCHAR to UTF-8 with automatic memory allocation.
 * 
 * @param wszStr Input SQLWCHAR string
 * @param cchLen Input length in code units, or SQL_NTS for null-terminated
 * @param out_utf8 Output pointer to allocated UTF-8 buffer (caller must free with rs_free)
 * @param unicodeType Unicode encoding (SQL_DD_CP_UTF16/UTF32, or -1 for auto-detect)
 * @return Number of bytes in allocated buffer (excluding null terminator)
 * @return 0 if wszStr is NULL, out_utf8 is NULL, conversion fails, or allocation fails
 */
size_t sqlwchar_to_utf8_alloc(const SQLWCHAR *wszStr, int cchLen, char **out_utf8, int unicodeType = -1);

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Sentinel value indicating no per-thread Unicode type override is set.
 */
constexpr int kUnicodeUnset = -1;

/**
 * Get reference to process-wide default Unicode type.
 *
 * @return Reference to atomic integer storing the default (SQL_DD_CP_UTF16 or
 * SQL_DD_CP_UTF32)
 * @note Thread-safe. Initialized to SQL_DD_CP_UTF16.
 * Initialized to SQL_DD_CP_UTF16, but may be changed at runtime,
 * e.g. by set_process_unicode_type() or by get_app_unicode_type()
 * when running under iODBC.
 */
inline std::atomic<int> &g_unicode_default() {
    static std::atomic<int> v{SQL_DD_CP_UTF16};
    return v;
}

/**
 * Get reference to thread-local Unicode type override.
 *
 * @return Reference to thread-local integer (kUnicodeUnset if no override set)
 * @note DLL-safe: uses plain int with no destructor.
 */
inline int &tls_unicode_ref() {
    static thread_local int tls = kUnicodeUnset;
    return tls;
}

/**
 * Get the active Unicode type for the current thread.
 *
 * Order of precedence:
 *  1. Per-thread override (if set via tls_unicode_ref()).
 *  2. Process-wide default, which is initialized to SQL_DD_CP_UTF16.
 *     On first use, if the Driver Manager is detected as iODBC,
 *     the default is upgraded to SQL_DD_CP_UTF32 (unless already overridden).
 *
 * @return SQL_DD_CP_UTF16 or SQL_DD_CP_UTF32.
 */
inline int get_app_unicode_type() {
    static std::once_flag flag;
    std::call_once(flag, []() {
        if (isIODBC()) {
            // Only change it if nobody has overridden it yet
            int expected = SQL_DD_CP_UTF16;
            int desired = SQL_DD_CP_UTF32;
            g_unicode_default().compare_exchange_strong(
                expected, desired, std::memory_order_relaxed);
        }
    });

    int tls = tls_unicode_ref();
    return (tls != kUnicodeUnset)
               ? tls
               : g_unicode_default().load(std::memory_order_relaxed);
}

/**
 * Set the process-wide default Unicode type.
 * 
 * @param v Unicode type (SQL_DD_CP_UTF16 or SQL_DD_CP_UTF32)
 * @note Thread-safe. Affects all threads without per-thread overrides.
 */
inline void set_process_unicode_type(int v) {
  g_unicode_default().store(v, std::memory_order_relaxed);
}


/**
 * Calculate SQLWCHAR string length with safety cap.
 * 
 * @param wszStr Input SQLWCHAR string
 * @param cap Maximum code units to scan (default: 1,048,576)
 * @param unicodeType Unicode encoding (SQL_DD_CP_UTF16/UTF32, or -1 for auto-detect)
 * @return Number of code units before null terminator (capped at `cap`)
 * @note If result equals cap, null terminator was not found within the limit
 */
std::size_t sqlwcsnlen_cap(const SQLWCHAR *wszStr,
                           std::size_t cap = kSQLWCHAR_SCAN_CAP, int unicodeType = -1);

/**
 * Get the size of SQLWCHAR for the current Unicode type.
 * 
 * @param unicodeType Unicode encoding (SQL_DD_CP_UTF16/UTF32, or -1 for auto-detect)
 * @return 2 for UTF-16, 4 for UTF-32. It never returns any other value.
 * @note UnixODBC and Windows use 2 bytes, iODBC uses 4 bytes
 */
std::size_t sizeofSQLWCHAR(int unicodeType = -1);

// ============================================================================
// DEPRECATED FUNCTIONS - Use the functions above instead
// ============================================================================

size_t wchar_to_utf8(SQLWCHAR *wszStr,size_t cchLen,char *szStr,size_t cbLen);
size_t utf8_to_wchar(const char *szStr,size_t cbLen,SQLWCHAR *wszStr,size_t cchLen);
unsigned char *convertWcharToUtf8(SQLWCHAR *wData, size_t cchLen);
SQLWCHAR *convertUtf8ToWchar(unsigned char *szData, size_t cbLen);
size_t calculate_utf8_len(SQLWCHAR* wszStr, size_t cchLen);
size_t calculate_wchar_len(const char* szStr, size_t cbLen, size_t *pcchLen);
