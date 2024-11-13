#include "iam/core/IAMUtils.h"
#include "common.h"

using namespace Redshift::IamSupport;

TEST(TEST_URL_VALIDATION_SUITE, test_happy_path){
    EXPECT_NO_THROW(IAMUtils::ValidateURL("https://www.website.com:5439/path/to/something?q=123"));
}

TEST(TEST_URL_VALIDATION_SUITE, test_no_www){
    EXPECT_NO_THROW(IAMUtils::ValidateURL("https://a.co"));
}


TEST(TEST_URL_VALIDATION_SUITE, test_no_www_empty_path){
    EXPECT_NO_THROW(IAMUtils::ValidateURL("https://a.co/"));
}

TEST(TEST_URL_VALIDATION_SUITE, test_no_www_with_query){
    EXPECT_NO_THROW(IAMUtils::ValidateURL("https://a.co?foo"));
}

TEST(TEST_URL_VALIDATION_SUITE, test_no_www_path_and_query){
    EXPECT_NO_THROW(IAMUtils::ValidateURL("https://a.co/?foo"));
}

TEST(TEST_URL_VALIDATION_SUITE, test_anchor){
    EXPECT_NO_THROW(IAMUtils::ValidateURL("https://www.website.com:5439/path/to/something#/anchor"));
}

TEST(TEST_URL_VALIDATION_SUITE, test_http){
    EXPECT_THROW(IAMUtils::ValidateURL("http://www.website.com:5439/path/to/something?q=123"), RsErrorException);
}

TEST(TEST_URL_VALIDATION_SUITE, test_unsafe_char){
    EXPECT_THROW(IAMUtils::ValidateURL("https://www.website.com:5439/path/to/something?{}|\^~[]`"), RsErrorException);
}

TEST(TEST_URL_VALIDATION_SUITE, test_inject_php){
    EXPECT_THROW(IAMUtils::ValidateURL("http://www.website.com:5439/path/to/something?q=123; phpinfo()"), RsErrorException);
}

TEST(TEST_URL_VALIDATION_SUITE, test_null){
    EXPECT_THROW(IAMUtils::ValidateURL(""), RsErrorException);
}

TEST(TEST_URL_VALIDATION_SUITE, test_query_inject_shell_command){
    EXPECT_THROW(IAMUtils::ValidateURL("https://demo_url?`ls`"), RsErrorException);
}

TEST(TEST_URL_VALIDATION_SUITE, test_inject_duplicate_schemes){
    EXPECT_THROW(IAMUtils::ValidateURL("https://https://https://https://https://"), RsErrorException);
}

TEST(TEST_URL_VALIDATION_SUITE, test_no_domain){
    EXPECT_THROW(IAMUtils::ValidateURL("https://"), RsErrorException);
}

TEST(TEST_URL_VALIDATION_SUITE, test_no_scheme){
    EXPECT_THROW(IAMUtils::ValidateURL("www.website.com"), RsErrorException);
}

TEST(TEST_URL_VALIDATION_SUITE, test_domain_inject_shell_command){
    EXPECT_THROW(IAMUtils::ValidateURL("https://www.`ls`.com"), RsErrorException);
}

TEST(TEST_URL_VALIDATION_SUITE, test_scheme_inject_shell_command){
    EXPECT_THROW(IAMUtils::ValidateURL("https`ls`://www.website.com"), RsErrorException);
}

TEST(TEST_URL_VALIDATION_SUITE, test_path_inject_shell_command){
    EXPECT_THROW(IAMUtils::ValidateURL("https://www.website.com:5439/path/`ls`/something?q=123"), RsErrorException);
}

TEST(TEST_URL_VALIDATION_SUITE, test_port_inject_shell_command){
    EXPECT_THROW(IAMUtils::ValidateURL("https://www.website.com:5439`ls`/path/to/something?q=something"), RsErrorException);
}

TEST(TEST_URL_VALIDATION_SUITE, test_incorrect_format){
    EXPECT_THROW(IAMUtils::ValidateURL("https://?this=that/path/to/something://www.website.com:2323/http://"), RsErrorException);
}

TEST(TEST_URL_VALIDATION_SUITE, test_userpwd){
    EXPECT_THROW(IAMUtils::ValidateURL("https://user:password@website.com:5439/path/to/something?query=q#/anchor"), RsErrorException);
}

TEST(TEST_URL_VALIDATION_SUITE, test_ipv6){
    EXPECT_THROW(IAMUtils::ValidateURL("http://[1080:0:0:0:8:800:200C:417A%eth0]/index.html"), RsErrorException);
}