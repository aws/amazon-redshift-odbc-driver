// iam_curl_cafile_test.cpp
//
// Unit tests for IAMCurlHttpClient::ResolveCaFile, which determines the CA file
// path handed to libcurl's CURLOPT_CAINFO option.
//
// The resolution has one subtlety worth pinning: when no CA file is configured
// it falls back to IAMUtils::GetDefaultCaFile(), which returns an rs_wstring.
// libcurl's curl_easy_setopt is variadic and reads the argument as a const
// char*, so the wide string must be converted to UTF-8 first. Passing the wide
// string directly would make libcurl read only up to the first embedded zero
// byte, truncating the path. ResolveCaFile centralizes that conversion so it can
// be verified without a live network connection.
//
// The client and its curl dependency are non-Windows only (guarded by
// #if !defined(_WIN32) in IAMCurlHttpClient.{h,cpp}), so these tests are too.

#include "common.h"

#ifndef _WIN32

#include "iam/core/IAMUtils.h"
#include "iam/http/IAMCurlHttpClient.h"
#include "iam/rs_iam_support.h"

#include <cstring>

using namespace Redshift::IamSupport;

// With no CA file configured, ResolveCaFile returns the driver default converted
// to UTF-8: a complete path, not a truncated fragment.
TEST(IAM_CURL_CAFILE_TEST_SUITE, ResolveCaFile_EmptyInput_ReturnsFullDefaultPath) {
    const rs_string resolved = IAMCurlHttpClient::ResolveCaFile("");

    // A wide-to-narrow reinterpretation of the path would collapse to its first
    // character (e.g. a leading '/'); the resolved path must be the full string.
    EXPECT_GT(resolved.size(), 1u)
        << "Resolved default CA path looks truncated; got: '" << resolved << "'";

    // It must equal the properly converted default and name the root cert file.
    const rs_string expected =
        IAMUtils::convertToUTF8(IAMUtils::GetDefaultCaFile());
    EXPECT_EQ(resolved, expected);
    EXPECT_NE(resolved.rfind("root.crt"), std::string::npos)
        << "Resolved default CA path should end in the root cert file name; got: '"
        << resolved << "'";
}

// A caller-supplied CA file path is passed through unchanged.
TEST(IAM_CURL_CAFILE_TEST_SUITE, ResolveCaFile_ExplicitPath_ReturnedVerbatim) {
    const rs_string caFile = "/opt/amazon/redshiftodbcx64/root.crt";
    EXPECT_EQ(IAMCurlHttpClient::ResolveCaFile(caFile), caFile);
}

// Constructing the client with no CA file configured drives the actual fallback
// path -- ResolveCaFile("") -> curl_easy_setopt(CURLOPT_CAINFO) -- so this covers
// the constructor/curl boundary, not just the helper in isolation. The
// constructor only initializes the handle and sets options (no network), so this
// stays a unit test. libcurl exposes no getter for CAINFO and a real TLS request
// needs a live endpoint, so the resolved value itself is asserted by the
// ResolveCaFile tests above; here we assert the fallback branch executes cleanly.
TEST(IAM_CURL_CAFILE_TEST_SUITE, ConstructWithEmptyCaFile_ReachesCurlBoundary) {
    HttpClientConfig config; // m_caFile is empty by default
    ASSERT_TRUE(config.m_caFile.empty());
    EXPECT_NO_THROW({ IAMCurlHttpClient client(config); });
}

// Confirms why ResolveCaFile converts the wide default rather than handing its
// bytes to a const char* API directly: reinterpreting the wide buffer as a
// narrow C string stops at the first zero byte, because the extra bytes of a
// multi-byte wchar_t are zero for ASCII code points. convertToUTF8 preserves the
// full path. Independent of whether the default path is absolute (installed
// driver) or relative (test process).
TEST(IAM_CURL_CAFILE_TEST_SUITE, WideDefaultPath_ReinterpretedAsNarrow_Truncates) {
    const rs_wstring wideDefault = IAMUtils::GetDefaultCaFile();
    ASSERT_GT(wideDefault.size(), 1u)
        << "Default CA path should be more than one character";

    // sizeof(wchar_t) is 4 on Linux/macOS (2 on Windows, which is excluded).
    ASSERT_GT(sizeof(wchar_t), 1u);

    // Reinterpreting the wide buffer as char* stops at the first zero byte. On
    // little-endian targets (all the driver's non-Windows platforms) that is the
    // byte right after the first character; on big-endian it would stop even
    // sooner. Either way the narrow view is far shorter than the real path --
    // assert that truncation rather than an endian-specific value.
    const char *asNarrow = reinterpret_cast<const char *>(wideDefault.c_str());
    const rs_string converted = IAMUtils::convertToUTF8(wideDefault);

    EXPECT_LT(std::strlen(asNarrow), wideDefault.size())
        << "Wide path read as a narrow string should truncate";
    EXPECT_LT(std::strlen(asNarrow), converted.size())
        << "Narrow reinterpretation must be shorter than the UTF-8 path";

    // The conversion preserves the full path. For an ASCII path the UTF-8 byte
    // count equals the code-point count; a non-ASCII component only adds bytes,
    // so the converted length is never shorter than the wide length.
    EXPECT_GT(converted.size(), 1u);
    EXPECT_GE(converted.size(), wideDefault.size())
        << "convertToUTF8 must not drop characters from the path";
}

#endif // !_WIN32
