/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#ifdef _MSC_VER
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#endif

#include "rsunicode.h"
#include "rsutil.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <locale>
#include <thread>
#include <cstdint>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#include <codecvt>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#include <sstream>


#if defined LINUX 

char g_utf8_len_data[128] =
{
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x80-0x8f */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x90-0x9f */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0xa0-0xaf */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0xb0-0xbf */
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0xc0-0xcf */
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0xd0-0xdf */
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, /* 0xe0-0xef */
    3,3,3,3,3,3,3,3,4,4,4,4,5,5,0,0  /* 0xf0-0xff */
};

unsigned char g_utf8_mask_data[6]   =  { 0x7f, 0x1f, 0x0f, 0x07, 0x03, 0x01 };
unsigned int  g_utf8_minval_data[6] =  { 0x0, 0x80, 0x800, 0x10000, 0x200000, 0x4000000 };

int unix_wchar_to_utf8_len(SQLWCHAR *pwStr, int cchLen);
int unix_wchar_to_utf8(SQLWCHAR *wszStr, int cchLen, char *szStr, int cbLen);
int unix_utf8_to_wchar_len(const char *szStr, int cbLen);
int unix_utf8_to_wchar(const char *szStr, int cbLen, SQLWCHAR *wszStr, int cchLen);

#endif // LINUX 


/*====================================================================================================================================================*/

// ============================================================================
// Internal helpers (may throw)
// ============================================================================

// Helpers for boundary-safe truncation (small & fast)
static inline size_t trim_utf8_boundary_backward(const std::string &s,
                                                 size_t toCopy) {
    // Ensure 'toCopy' ends at a UTF-8 code point boundary.
    if (toCopy == 0 || toCopy >= s.size())
        return toCopy;

    auto is_cont = [](unsigned char b) { return (b & 0xC0) == 0x80; };

    // Walk back over continuation bytes to find the lead byte.
    size_t i = toCopy;
    while (i > 0 && is_cont(static_cast<unsigned char>(s[i - 1])))
        --i;

    if (i == 0)
        return 0; // started inside a sequence; copy nothing.
    unsigned char lead = static_cast<unsigned char>(s[i - 1]);
    size_t need =
        (lead < 0x80)
            ? 1
            : ((lead & 0xE0) == 0xC0 ? 2 : ((lead & 0xF0) == 0xE0 ? 3 : 4));
    // If we ended right after the lead but don't have the full code point, drop
    // the lead too.
    if (i - 1 + need > toCopy)
        return i - 1;
    return toCopy; // already ends after a full code point
}

static inline bool is_high_surrogate(uint16_t u) {
    return u >= 0xD800 && u <= 0xDBFF;
}
static inline bool is_low_surrogate(uint16_t u) {
    return u >= 0xDC00 && u <= 0xDFFF;
} // only for completeness

static inline std::string _utf16_to_utf8_throw(const char16_t *begin,
                                               const char16_t *end) {
    thread_local std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,
                                             char16_t>* conv = nullptr;
    if (!conv) {
        conv = new std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,
                                             char16_t>();
    }
    return conv->to_bytes(begin, end);
}

static inline std::u16string _utf8_to_utf16_throw(const char *begin,
                                                  const char *end) {
    thread_local std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,
                                             char16_t>* conv = nullptr;
    if (!conv) {
        conv = new std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,
                                             char16_t>();
    }
    return conv->from_bytes(begin, end);
}

// UTF-32 conversion helpers
static inline std::string _utf32_to_utf8_throw(const char32_t *begin,
                                               const char32_t *end) {
    thread_local std::wstring_convert<std::codecvt_utf8<char32_t>,
                                             char32_t>* conv = nullptr;
    if (!conv) {
        conv = new std::wstring_convert<std::codecvt_utf8<char32_t>,
                                             char32_t>();
    }
    return conv->to_bytes(begin, end);
}

static inline std::u32string _utf8_to_utf32_throw(const char *begin,
                                                  const char *end) {
    thread_local std::wstring_convert<std::codecvt_utf8<char32_t>,
                                             char32_t>* conv = nullptr;
    if (!conv) {
        conv = new std::wstring_convert<std::codecvt_utf8<char32_t>,
                                             char32_t>();
    }
    return conv->from_bytes(begin, end);
}

static size_t wchar32_to_utf8_str(const SQLWCHAR *wszStr, int cchLen,
                                  std::string &szStr);
static size_t char_utf8_to_utf32_wchar(const char *szStr, int cchLen,
                                       SQLWCHAR *wszStr, int bufferSize,
                                       size_t *totalNeeded = nullptr);

size_t wchar16_to_utf8_str(const SQLWCHAR *wszStr, int cchLen,
                           std::string &szStr) {
    if (!wszStr || (cchLen < 0 && cchLen != SQL_NTS))
        return 0;

    // View the input as UTF-16 code units
    const char16_t *begin = reinterpret_cast<const char16_t *>(wszStr);
    const char16_t *end = nullptr;

    if (cchLen == SQL_NTS) {
        // Caller promises NUL-termination
        end = begin + std::char_traits<char16_t>::length(begin);
    } else {
        end = begin + cchLen;
    }

    try {
        szStr = _utf16_to_utf8_throw(begin, end);
        return szStr.size(); // bytes
    } catch (const std::exception &e) {
        std::stringstream err;
        err << "UTF-16→UTF-8 conversion failed: " << e.what();
        RS_LOG_ERROR("RSUNICODE", "%s", err.str().c_str());
        szStr.clear();
        return 0;
    } catch (...) {
        RS_LOG_ERROR("RSUNICODE",
                     "Unknown error during UTF-16→UTF-8 conversion.");
        szStr.clear();
        return 0;
    }
}

size_t wchar16_to_utf8_char(const SQLWCHAR *wszStr, int cchLen, char *szStr,
                            int bufferSize, size_t *totalCharsNeeded) {
    if (!wszStr || !szStr || (cchLen < 0 && cchLen != SQL_NTS)) {
        return 0;
    }
    if (bufferSize < 0) {
        return 0; // invalid
    }

    std::string utf8;
    const size_t utf8StrSize = wchar16_to_utf8_str(wszStr, cchLen, utf8);

    if (totalCharsNeeded) {
        *totalCharsNeeded = utf8StrSize;
    }
    // If conversion failed (and we’re in no-throw mode), utf8StrSize==0 and
    // utf8 empty.

    // bufferSize is in *char elements*.
    if (bufferSize <= 0) {
        // No space to write anything, including terminator.
        return 0;
    }

    // Leave 1 for NUL
    const size_t capacityNoNul = static_cast<size_t>(bufferSize - 1);
    size_t toCopy = (std::min)(capacityNoNul, utf8.size());

    // If truncation occurred, snap to a UTF-8 boundary so output stays valid.
    if (toCopy < utf8.size()) {
        toCopy = trim_utf8_boundary_backward(utf8, toCopy);
    }

    if (toCopy) {
        std::memcpy(szStr, utf8.data(), toCopy);
    }
    szStr[toCopy] = '\0';

    if (toCopy < utf8StrSize) {
        RS_LOG_WARN("RSUNICODE",
                    "Unicode conversion truncated %u/%u bytes (UTF-8 "
                    "preserved, NUL-terminated)",
                    (unsigned)toCopy, (unsigned)utf8StrSize);
    }

    return toCopy; // bytes written, excluding NUL termination
}

size_t char_utf8_to_utf16_str(const char *szStr, int cchLen,
                              std::u16string &utf16) {
    if (!szStr || (cchLen < 0 && cchLen != SQL_NTS)) {
        return 0;
    }

    const char *begin = szStr;
    const char *end = nullptr;
    if (cchLen == SQL_NTS) {
        end = szStr + std::char_traits<char>::length(szStr);
    } else {
        end = szStr + cchLen;
    }

    try {
        utf16 = _utf8_to_utf16_throw(begin, end);
        return utf16.size(); // code units
    } catch (const std::exception &e) {
        std::stringstream err;
        err << "UTF-8→UTF-16 conversion failed: " << e.what();
        RS_LOG_ERROR("RSUNICODE", "%s", err.str().c_str());
        utf16.clear();
        return 0;
    } catch (...) {
        RS_LOG_ERROR("RSUNICODE",
                     "Unknown error during UTF-8→UTF-16 conversion.");
        utf16.clear();
        return 0;
    }
}

size_t char_utf8_to_utf16_strlen(const char *szStr, int cchLen) {
    if (!szStr || (cchLen < 0 && cchLen != SQL_NTS)) {
        return 0;
    }

    std::u16string u16;
    return char_utf8_to_utf16_str(szStr, cchLen, u16); // may be 0 on failure
}

size_t char_utf8_to_utf16_wchar(const char *szStr, int cchLen, SQLWCHAR *wszStr,
                                int bufferSize, size_t *totalCharsNeeded) {
    if (!szStr || !wszStr || (cchLen < 0 && cchLen != SQL_NTS)) {
        return 0;
    }

    if (bufferSize < 0) {
        return 0; // invalid
    }

    std::u16string u16;
    const size_t totalUnits =
        char_utf8_to_utf16_str(szStr, cchLen, u16); // may be 0 on failure

    // Return total characters needed if requested
    if (totalCharsNeeded) {
        *totalCharsNeeded = totalUnits;
    }

    if (bufferSize <= 0) {
        // No space to write anything, including terminator.
        return 0;
    }

    // Leave 1 for NUL (u'\0')
    const size_t capacityNoNul = static_cast<size_t>(bufferSize - 1);
    size_t toCopyUnits = (std::min)(capacityNoNul, u16.size());

    // If truncating, avoid leaving a dangling high surrogate at the end.
    if (toCopyUnits < u16.size() && toCopyUnits > 0 &&
        is_high_surrogate(u16[toCopyUnits - 1])) {
        --toCopyUnits;
    }

    if (toCopyUnits) {
        // Copy as bytes
        std::memcpy(static_cast<void *>(wszStr),
                    static_cast<const void *>(u16.data()),
                    toCopyUnits * sizeof(char16_t));
    }

    // NUL-terminate if we have at least 1 slot (we do, bufferSize>0)
    reinterpret_cast<char16_t *>(wszStr)[toCopyUnits] = u'\0';

    if (toCopyUnits < totalUnits) {
        RS_LOG_WARN(
            "RSUNICODE",
            "Unicode conversion truncated %u/%u UTF-16 units (%u/%u bytes)",
            (unsigned)toCopyUnits, (unsigned)totalUnits,
            (unsigned)(toCopyUnits * sizeof(char16_t)),
            (unsigned)(totalUnits * sizeof(char16_t)));
    }

    return toCopyUnits; // elements written, excluding terminator
}

// ============================================================================
// UTF-32 specific implementations
// ============================================================================

size_t wchar32_to_utf8_str(const SQLWCHAR* wszStr, int cchLen, std::string& szStr) {
    if (!wszStr || (cchLen < 0 && cchLen != SQL_NTS)) return 0;

    const char32_t* begin = reinterpret_cast<const char32_t*>(wszStr);
    const char32_t* end = nullptr;

    if (cchLen == SQL_NTS) {
        end = begin + std::char_traits<char32_t>::length(begin);
    } else {
        end = begin + cchLen;
    }

    try {
        szStr = _utf32_to_utf8_throw(begin, end);
        return szStr.size();
    } catch (const std::exception& e) {
        std::stringstream err;
        err << "UTF-32→UTF-8 conversion failed: " << e.what();
        RS_LOG_ERROR("RSUNICODE", "%s", err.str().c_str());
        szStr.clear();
        return 0;
    } catch (...) {
        RS_LOG_ERROR("RSUNICODE", "Unknown error during UTF-32→UTF-8 conversion.");
        szStr.clear();
        return 0;
    }
}

size_t wchar32_to_utf8_char(const SQLWCHAR* wszStr, int cchLen, char* szStr, int bufferSize, size_t *totalCharsNeeded) {
    if (!wszStr || !szStr || (cchLen < 0 && cchLen != SQL_NTS)) return 0;
    if (bufferSize < 0) return 0; // invalid

    std::string utf8;
    size_t utf8StrSize = wchar32_to_utf8_str(wszStr, cchLen, utf8);
    if (totalCharsNeeded) {
        *totalCharsNeeded = utf8StrSize;
    }
    if (bufferSize <= 0) {
        // totalCharsNeeded is set, but no space to write anything
        return 0;
    }
    
    size_t toCopy = (std::min)(static_cast<size_t>(bufferSize - 1), utf8.size());
    if (toCopy < utf8.size()) {
        toCopy = trim_utf8_boundary_backward(utf8, toCopy);
    }
    
    if (toCopy) {
        std::memcpy(szStr, utf8.data(), toCopy);
    }
    szStr[toCopy] = '\0';

    if (toCopy < utf8StrSize) {
        RS_LOG_WARN("RSUNICODE",
            "Unicode conversion truncated %u/%u bytes (UTF-8 preserved, NUL-terminated)",
            (unsigned)toCopy, (unsigned)utf8StrSize);
    }
    return toCopy;
}

size_t char_utf8_to_utf32_strlen(const char *szStr, int cchLen) {
    if (!szStr || (cchLen < 0 && cchLen != SQL_NTS)) {
        return 0;
    }
    const char *begin = szStr;
    const char *end =
        (cchLen == SQL_NTS) ? szStr + strlen(szStr) : szStr + cchLen;
    try {
        std::u32string u32 = _utf8_to_utf32_throw(begin, end);
        return u32.size();
    } catch (const std::exception &e) {
        std::stringstream err;
        err << "UTF-8→UTF-32 strlen failed: " << e.what();
        RS_LOG_ERROR("RSUNICODE", "%s", err.str().c_str());
        return 0;
    } catch (...) {
        RS_LOG_ERROR("RSUNICODE",
                     "Unknown error during UTF-8→UTF-32 strlen calculation.");
        return 0;
    }
}

size_t char_utf8_to_utf32_wchar(const char* szStr, int cchLen, SQLWCHAR* wszStr, int bufferSize, size_t* totalCharsNeeded) {
    if (!szStr || !wszStr || (cchLen < 0 && cchLen != SQL_NTS)) {
        return 0;
    }
    if (bufferSize < 0) {
        return 0; // invalid
    }

    const char* begin = szStr;
    const char* end = nullptr;
    if (cchLen == SQL_NTS) {
        end = szStr + std::char_traits<char>::length(szStr);
    } else {
        end = szStr + cchLen;
    }

    try {
        std::u32string u32 = _utf8_to_utf32_throw(begin, end);
        
        if (totalCharsNeeded) {
            *totalCharsNeeded = u32.size();
        }
        if (bufferSize <= 0) {
            // totalCharsNeeded is set, but no space to write anything
            return 0;
        }
        const size_t capacityNoNul = static_cast<size_t>(bufferSize - 1);
        size_t toCopyUnits = (std::min)(capacityNoNul, u32.size());

        if (toCopyUnits) {
            std::memcpy(static_cast<void*>(wszStr),
                        static_cast<const void*>(u32.data()),
                        toCopyUnits * sizeof(char32_t));
        }

        reinterpret_cast<char32_t*>(wszStr)[toCopyUnits] = U'\0';
        return toCopyUnits;
    } catch (const std::exception& e) {
        std::stringstream err;
        err << "UTF-8→UTF-32 copy failed: " << e.what();
        RS_LOG_ERROR("RSUNICODE", "%s", err.str().c_str());
        return 0;
    } catch (...) {
        RS_LOG_ERROR("RSUNICODE", "Unknown error during UTF-8→UTF-32 copy.");
        return 0;
    }
}

// ============================================================================
// Helper function implementations
// ============================================================================

std::size_t sqlwcsnlen_cap(const SQLWCHAR *wszStr, std::size_t cap, int unicodeType) {
    if (!wszStr || cap == 0)
        return 0;
    if(unicodeType == -1) unicodeType = get_app_unicode_type();
    const unsigned char* p = reinterpret_cast<const unsigned char*>(wszStr);

    if (unicodeType == SQL_DD_CP_UTF16) {
        // count char16_t code units up to cap or NUL
        std::size_t i = 0;
        for (; i < cap; ++i) {
            char16_t cu;
            std::memcpy(&cu, p + i * sizeof(char16_t), sizeof(char16_t));
            if (cu == 0) break;
        }
        return i;
    } else if (unicodeType == SQL_DD_CP_UTF32) {
        // count char32_t code units up to cap or NUL
        std::size_t i = 0;
        for (; i < cap; ++i) {
            char32_t cu;
            std::memcpy(&cu, p + i * sizeof(char32_t), sizeof(char32_t));
            if (cu == 0) break;
        }
        return i;
    }
    // Unknown mode: log error and return 0
    RS_LOG_ERROR("RSUNICODE", "Unknown Unicode type %d in sqlwcsnlen_cap", unicodeType);
    return 0;
}

std::size_t sizeofSQLWCHAR(int unicodeType) {
    if (unicodeType == -1) unicodeType = get_app_unicode_type();
    if (unicodeType == SQL_DD_CP_UTF16) {
        return sizeof(char16_t);
    } else if (unicodeType == SQL_DD_CP_UTF32) {
        return sizeof(char32_t);
    }
    // Unknown mode: log warning and use default
    RS_LOG_WARN("RSUNICODE", "Unknown Unicode type %d in sizeofSQLWCHAR, defaulting to UTF-16", unicodeType);
    return sizeof(char16_t);
}

// ============================================================================
// Generic SQLWCHAR functions
// ============================================================================
size_t sqlwchar_to_utf8_str(const SQLWCHAR* wszStr, int cchLen, std::string& szStr, int unicodeType) {
    if (unicodeType == -1) {
        unicodeType = get_app_unicode_type();
    }
    
    switch (unicodeType) {
        case SQL_DD_CP_UTF16:
            return wchar16_to_utf8_str(wszStr, cchLen, szStr);
        case SQL_DD_CP_UTF32:
            return wchar32_to_utf8_str(wszStr, cchLen, szStr);
        default:
            RS_LOG_WARN("RSUNICODE", "Unknown unicode type %d, defaulting to UTF-16", unicodeType);
            return wchar16_to_utf8_str(wszStr, cchLen, szStr);
    }
}

size_t sqlwchar_to_utf8_char(const SQLWCHAR* wszStr, int cchLen, char* szStr, int bufferSize, size_t *totalCharsNeeded, int unicodeType) {
    if (unicodeType == -1) {
        unicodeType = get_app_unicode_type();
    }
    
    switch (unicodeType) {
        case SQL_DD_CP_UTF16:
            return wchar16_to_utf8_char(wszStr, cchLen, szStr, bufferSize, totalCharsNeeded);
        case SQL_DD_CP_UTF32:
            return wchar32_to_utf8_char(wszStr, cchLen, szStr, bufferSize, totalCharsNeeded);
        default:
            RS_LOG_WARN("RSUNICODE", "Unknown unicode type %d, defaulting to UTF-16", unicodeType);
            return wchar16_to_utf8_char(wszStr, cchLen, szStr, bufferSize, totalCharsNeeded);
    }
}

size_t utf8_to_sqlwchar_str(const char* szStr, int cchLen, SQLWCHAR* wszStr, int bufferSize, size_t *totalCharsNeeded, int unicodeType) {
    if (unicodeType == -1) {
        unicodeType = get_app_unicode_type();
    }
    
    switch (unicodeType) {
        case SQL_DD_CP_UTF16:
            return char_utf8_to_utf16_wchar(szStr, cchLen, wszStr, bufferSize, totalCharsNeeded);
        case SQL_DD_CP_UTF32:
            return char_utf8_to_utf32_wchar(szStr, cchLen, wszStr, bufferSize, totalCharsNeeded);
        default:
            RS_LOG_WARN("RSUNICODE", "Unknown unicode type %d, defaulting to UTF-16", unicodeType);
            return char_utf8_to_utf16_wchar(szStr, cchLen, wszStr, bufferSize, totalCharsNeeded);
    }
}

size_t utf8_to_sqlwchar_strlen(const char* szStr, int cchLen, int unicodeType) {
    if (unicodeType == -1) {
        unicodeType = get_app_unicode_type();
    }
    
    switch (unicodeType) {
        case SQL_DD_CP_UTF16:
            return char_utf8_to_utf16_strlen(szStr, cchLen);
        case SQL_DD_CP_UTF32:
            return char_utf8_to_utf32_strlen(szStr, cchLen);
        default:
            RS_LOG_WARN("RSUNICODE", "Unknown unicode type %d, defaulting to UTF-16", unicodeType);
            return char_utf8_to_utf16_strlen(szStr, cchLen);
    }
}

size_t utf8_to_sqlwchar_alloc(const char* szStr, int cchLen, SQLWCHAR** out_wchar, int unicodeType) {
    if (!szStr || !out_wchar) return 0;
    
    if (unicodeType == -1) {
        unicodeType = get_app_unicode_type();
    }
    
    size_t charCount = utf8_to_sqlwchar_strlen(szStr, cchLen, unicodeType);
    if (charCount == 0) {
        *out_wchar = NULL;
        return 0;
    }
    
    // Check for potential overflow before allocation
    size_t unitSize = sizeofSQLWCHAR(unicodeType);
    if (charCount > SIZE_MAX - 1 || (charCount + 1) > SIZE_MAX / unitSize) {
        RS_LOG_ERROR("RSUNICODE", "Integer overflow in utf8_to_sqlwchar_alloc: charCount=%zu", charCount);
        *out_wchar = NULL;
        return 0;
    }
    
    size_t bytesToAllocate = (charCount + 1) * unitSize;
    *out_wchar = (SQLWCHAR*)rs_malloc(bytesToAllocate);
    if (!*out_wchar) return 0;
    
    // Zero-initialize the allocated memory
    memset(*out_wchar, 0, bytesToAllocate);
    
    size_t result = utf8_to_sqlwchar_str(szStr, cchLen, *out_wchar, charCount + 1, nullptr, unicodeType);
    if (result == 0) {
        rs_free(*out_wchar);
        *out_wchar = NULL;
    }
    
    return result;
}

size_t sqlwchar_to_utf8_alloc(const SQLWCHAR* wszStr, int cchLen, char** out_utf8, int unicodeType) {
    if (!wszStr || !out_utf8) return 0;
    
    if (unicodeType == -1) {
        unicodeType = get_app_unicode_type();
    }
    
    std::string utf8;
    size_t byteCount = sqlwchar_to_utf8_str(wszStr, cchLen, utf8, unicodeType);
    if (byteCount == 0) {
        *out_utf8 = NULL;
        return 0;
    }
    
    *out_utf8 = (char*)rs_malloc(byteCount + 1);
    if (!*out_utf8) return 0;
    
    memcpy(*out_utf8, utf8.data(), byteCount);
    (*out_utf8)[byteCount] = '\0';
    
    return byteCount;
}

/**
 * @deprecated Use wchar16_to_utf8_char or sqlwchar_to_utf8_char instead
 */
size_t wchar_to_utf8(SQLWCHAR *wszStr,size_t cchLen,char *szStr,size_t cbLen)
{
    size_t len = 0;
    
    if(wszStr && wszStr[0] != '\0')
    {
#ifdef WIN32
        len = (size_t) WideCharToMultiByte(CP_UTF8, 0, wszStr, (INT_LEN(cchLen) == SQL_NTS) ? -1 : (int)cchLen, szStr,  
                                        (int) cbLen, NULL,NULL);
#endif
#if defined LINUX 
        if(((int)cchLen) == SQL_NTS && wszStr)
            cchLen = unix_wchar_to_utf8_len(wszStr, cchLen) + 1;

        len = unix_wchar_to_utf8(wszStr,cchLen,szStr,cbLen);
#endif
    }

    if(szStr)
    {
        if(len < cbLen)
            szStr[len] = '\0';
    }

    return len;
}

/*====================================================================================================================================================*/

/**
 * Convert UTF-8 to SQLWCHAR.
 * @deprecated Use char_utf8_to_utf16_wchar or utf8_to_sqlwchar_str instead
 */
size_t utf8_to_wchar(const char *szStr,size_t cbLen,SQLWCHAR *wszStr,size_t cchLen)
{
    size_t len =  0;
    
    if(szStr && szStr[0] != '\0')
    {
#ifdef WIN32
        len = (size_t) MultiByteToWideChar(CP_UTF8,  0, szStr,  (INT_LEN(cbLen) == SQL_NTS ) ? -1 : (int) cbLen, wszStr, (int) cchLen);
#endif
#if defined LINUX 
        if(cbLen == SQL_NTS && szStr)
             cbLen = strlen(szStr) + 1;

        len = unix_utf8_to_wchar(szStr,cbLen,wszStr,cchLen);
#endif
    }

    if(wszStr)
    {
        if(len < cchLen)
            wszStr[len] = L'\0';
    }

    return len;
}

/*====================================================================================================================================================*/

/**
 * Allocate a buffer and convert SQLWCHAR to UTF-8.
 * @deprecated Use sqlwchar_to_utf8_alloc instead
 */
unsigned char *convertWcharToUtf8(SQLWCHAR *wData, size_t cchLen)
{
    unsigned char *szData = NULL;

    if(wData != NULL)
    {
        size_t cbLen;

        cbLen = calculate_utf8_len(wData, cchLen);

        szData = (cbLen) ? (unsigned char *) rs_malloc(cbLen) : NULL;

        wchar_to_utf8(wData, cchLen, (char *)szData, cbLen);
    }

    return szData;
}

/*====================================================================================================================================================*/

/**
 * Allocate a buffer and convert UTF-8 to SQLWCHAR.
 * @deprecated Use utf8_to_sqlwchar_alloc instead
 */
SQLWCHAR *convertUtf8ToWchar(unsigned char *szData, size_t cbLen)
{
    SQLWCHAR *wData = NULL;

    if(szData != NULL)
    {
        size_t cchLen;
        size_t len = calculate_wchar_len((char *)szData, cbLen, &cchLen);

        wData = (len) ?  (SQLWCHAR *) rs_malloc(len) : NULL;

        utf8_to_wchar((char *)szData, cbLen, wData, cchLen);
    }

    return wData;
}

/*====================================================================================================================================================*/

/**
 * Calculate UTF-8 length needed for SQLWCHAR string.
 * @deprecated Use utf8_to_sqlwchar_alloc instead
 */
size_t calculate_utf8_len(SQLWCHAR* wszStr, size_t cchLen)
{
    size_t len;

    if(wszStr)
    {
#ifdef WIN32
        len = (cchLen == SQL_NTS) ? wcslen(wszStr) : ( (INT_LEN(cchLen) == SQL_NULL_DATA) ? 0 : cchLen);

        if(len != 0)
        {
            size_t len1 = (size_t) WideCharToMultiByte(CP_UTF8, 0, wszStr, (INT_LEN(cchLen) == SQL_NTS) ? -1 : (int)cchLen, NULL,  
                                                        0, NULL,NULL);

            if(len1 > len)
                len = len1;
        }

#endif
#if defined LINUX 
        len = (((int)cchLen) == SQL_NTS) ? unix_wchar_to_utf8_len(wszStr, SQL_NTS) : ( (cchLen == SQL_NULL_DATA) ? 0 : cchLen);
#endif

        len++; // +1 for '\0'
    }
    else
        len = 0;

    return len;
}

/*====================================================================================================================================================*/

/**
 * Calculate SQLWCHAR length needed for UTF-8 string.
 * @deprecated Use utf8_to_sqlwchar_alloc instead
 */
size_t calculate_wchar_len(const char* szStr, size_t cbLen, size_t *pcchLen)
{
    size_t len;

    if(szStr)
    {
        len = (INT_LEN(cbLen) == SQL_NTS) ? strlen(szStr) : cbLen;
        len++; // +1 for '\0'
        if(pcchLen)
            *pcchLen = len;
        len *= sizeofSQLWCHAR();
    }
    else
    {
        len = 0;
        if(pcchLen)
            *pcchLen = len;
    }

    return len;
}

/*====================================================================================================================================================*/

#if defined LINUX 

/**
 * Calculate UTF-8 length needed for SQLWCHAR string (Linux implementation).
 * @deprecated Unused
 */
int unix_wchar_to_utf8_len(SQLWCHAR *pwStr, int cchLen)
{
    int iLen = 0;

    if(pwStr)
    {
        if(cchLen == SQL_NTS)
        {
            for (iLen = 0; *pwStr; pwStr++, iLen++)
            {
                 if (*pwStr >= 0x80)
                 {
                     iLen++;
                     if (*pwStr >= 0x800) iLen++;
                 }
            } // Loop
        }
        else
        {
            for (iLen = 0; cchLen; cchLen--, pwStr++, iLen++)
            {
                 if (*pwStr >= 0x80)
                 {
                     iLen++;
                     if (*pwStr >= 0x800) iLen++;
                 }
            } // Loop
        }
    }

    return iLen;
}

/*====================================================================================================================================================*/

/**
 * Convert SQLWCHAR to UTF-8 (Linux implementation).
 * @deprecated Unused
 */
int unix_wchar_to_utf8(SQLWCHAR *wszStr, int cchLen, char *szStr, int cbLen)
{
    char *pStart = szStr;

    if (!cbLen)
        return unix_wchar_to_utf8_len(wszStr, cchLen);

    if(wszStr)
    {
        for (; cchLen; cchLen--, wszStr++)
        {
             SQLWCHAR wch = *wszStr;

             if (wch < 0x80)  /* 1 byte */
             {
                 if (!cbLen--)
                     return 0;

                 *szStr++ = (char) wch;
                 continue;
             }

             if (wch < 0x800)  /* 2 bytes */
             {
                 if ((cbLen -= 2) < 0)
                     return 0;

                 szStr[1] = 0x80 | (wch & 0x3f);
                 wch >>= 6;
                 szStr[0] = 0xc0 | wch;
                 szStr += 2;
                 continue;
             }

             /*  3 bytes */
            if ((cbLen -= 3) < 0) return 0;
            szStr[2] = 0x80 | (wch & 0x3f);
            wch >>= 6;
            szStr[1] = 0x80 | (wch & 0x3f);
            wch >>= 6;
            szStr[0] = 0xe0 | wch;
            szStr += 3;
        } // Loop
    }

    return (int)(szStr - pStart);
}

/*====================================================================================================================================================*/

/**
 * Calculate SQLWCHAR length needed for UTF-8 string (Linux implementation).
 * @deprecated Unused
 */
int unix_utf8_to_wchar_len(const char *szStr, int cbLen)
{
    int iLen;
    const char *pEnd = szStr + cbLen;

    for (iLen = 0; szStr < pEnd; iLen++)
    {
        char ch = *szStr++;

        if ((unsigned char)ch < 0xc0)
            continue;

        switch(g_utf8_len_data[((unsigned char)ch)-0x80])
        {
            case 5:
            {
                if (szStr >= pEnd)
                    return iLen;
                if ((ch = *szStr ^ 0x80) >= 0x40)
                    continue;
                szStr++;
            }

            // no break
            case 4:
            {
                if (szStr >= pEnd)
                    return iLen;
                if ((ch = *szStr ^ 0x80) >= 0x40)
                    continue;
                szStr++;
            }

            // no break
            case 3:
            {
                if (szStr >= pEnd)
                    return iLen;
                if ((ch = *szStr ^ 0x80) >= 0x40)
                    continue;
                szStr++;
            }

            // no break
            case 2:
            {
                if (szStr >= pEnd)
                    return iLen;
                if ((ch = *szStr ^ 0x80) >= 0x40)
                    continue;
                szStr++;
            }

            // no break
            case 1:
            {
                if (szStr >= pEnd)
                    return iLen;
                if ((ch = *szStr ^ 0x80) >= 0x40)
                    continue;
                szStr++;
            }

            // no break
            default:
            {
                break;
            }
        } // Switch
    } // Loop

    return iLen;
}

/*====================================================================================================================================================*/

/**
 * Convert UTF-8 to SQLWCHAR (Linux implementation).
 * @deprecated Unused
 */
int unix_utf8_to_wchar(const char *szStr, int cbLen, SQLWCHAR *wszStr, int cchLen)
{
    int iLen, iCount;
    int wch;
    const char *pEnd = szStr + cbLen;

    if (!cchLen)
        return unix_utf8_to_wchar_len(szStr, cbLen);

    for (iCount = cchLen; iCount && (szStr < pEnd); iCount--, wszStr++)
    {
        char ch = *szStr++;

        wch = 0;

        if ((unsigned char)ch < 0x80)  /* 7-bit ASCII */
        {
            wch = ch;
            *wszStr = wch;

            continue;
        }

        iLen = g_utf8_len_data[((unsigned char)ch)-0x80];
        wch = ch & g_utf8_mask_data[iLen];

        switch(iLen)
        {
            case 5:
            {
                if (szStr >= pEnd)
                    goto end;
                if ((ch = *szStr ^ 0x80) >= 0x40)
                    return 0;
                wch = (wch << 6) | ch;
                szStr++;
            }

            // no break
            case 4:
            {
                if (szStr >= pEnd)
                    goto end;
                if ((ch = *szStr ^ 0x80) >= 0x40)
                    return 0;
                wch = (wch << 6) | ch;
                szStr++;
            }

            // no break
            case 3:
            {
                if (szStr >= pEnd)
                    goto end;
                if ((ch = *szStr ^ 0x80) >= 0x40)
                    return 0;
                wch = (wch << 6) | ch;
                szStr++;
            }

            // no break
            case 2:
            {
                if (szStr >= pEnd)
                    goto end;
                if ((ch = *szStr ^ 0x80) >= 0x40)
                    return 0;
                wch = (wch << 6) | ch;
                szStr++;
            }

            // no break
            case 1:
            {
                if (szStr >= pEnd)
                    goto end;
                if ((ch = *szStr ^ 0x80) >= 0x40)
                    return 0;
                wch = (wch << 6) | ch;
                szStr++;
                if (wch < g_utf8_minval_data[iLen])
                    return 0;
                if (wch >= 0x10000)
                    return 0;
                *wszStr = wch;

                continue;
            }
        } // Switch
    } // Loop

    if (szStr < pEnd)
        return 0;
end:
    return cchLen - iCount;
}

#endif // LINUX

/*====================================================================================================================================================*/
