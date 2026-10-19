#include "common.h"
#include <rsdesc.h>
#include <rsodbc.h>
#include <rsutil.h>
#include "rsodbc_setup/rs_dsn_ssl_keys.h"
#include <sql.h>
#include <cstring>
#include <cstdint>
#include <new>
#include <string>
#include <vector>
#ifndef WIN32
#include <unistd.h>
#endif
#include "pq_tls_policy.h"

// Unit tests for connection property default values and parsing.
// This unit test is for testing the default value when databaseMetadataCurrentDbOnly flag is not included in DSN.

TEST(DEFAULT_VALUE_TEST_SUITE, test_DatabaseMetadataCurrentDbOnly) {

    RS_CONNECT_PROPS_INFO obj;
    EXPECT_TRUE(obj.iDatabaseMetadataCurrentDbOnly == 1);

}

TEST(DEFAULT_VALUE_TEST_SUITE, test_BoolsAsChar) {

    RS_CONNECT_PROPS_INFO obj;
    EXPECT_TRUE(obj.iBoolsAsChar == 0);

}

TEST(DEFAULT_VALUE_TEST_SUITE, test_UseDeclareFetch) {

    RS_CONNECT_PROPS_INFO obj;
    EXPECT_TRUE(obj.iUseDeclareFetch == 0);

}

TEST(DEFAULT_VALUE_TEST_SUITE, test_FetchSize) {

    RS_CONNECT_PROPS_INFO obj;
    EXPECT_TRUE(obj.iFetchSize == 0);

}

// ===========================================================================
// Key define correctness
// ===========================================================================

TEST(DEFAULT_VALUE_TEST_SUITE, test_RS_USE_DECLARE_FETCH_key) {
    EXPECT_STREQ(RS_USE_DECLARE_FETCH, "UseDeclareFetch");
}

TEST(DEFAULT_VALUE_TEST_SUITE, test_RS_FETCH_SIZE_key) {
    // RS_FETCH_SIZE is the connection property key name "Fetch"
    // (the parameter name customers use in connection strings and DSN entries).
    EXPECT_STREQ(RS_FETCH_SIZE, "Fetch");
}

TEST(DEFAULT_VALUE_TEST_SUITE, test_RS_DEFAULT_FETCH_SIZE_value) {
    EXPECT_EQ(RS_DEFAULT_FETCH_SIZE, 100);
}

// szSslMode and iEncryptionMethod must agree, or DSN and DSN-less connects get
// different defaults.

TEST(DEFAULT_VALUE_TEST_SUITE, test_SslMode_default_is_verify_full) {
    RS_CONNECT_PROPS_INFO obj;
    EXPECT_STREQ(obj.szSslMode, "verify-full")
        << "The default SSL mode must verify the server's hostname";
}

TEST(DEFAULT_VALUE_TEST_SUITE, test_EncryptionMethod_default_matches_SslMode) {
    RS_CONNECT_PROPS_INFO obj;
    // libpqConnect maps 0=disable, 1=verify-ca, 2=verify-full, 3=require.
    EXPECT_EQ(obj.iEncryptionMethod, 2)
        << "iEncryptionMethod must re-derive the same mode szSslMode carries, "
           "or the default differs between the DSN and DSN-less paths";
}

TEST(DEFAULT_VALUE_TEST_SUITE, test_RS_SSL_MODE_key) {
    EXPECT_STREQ(RS_SSL_MODE, "SSLMode");
}

// ===========================================================================
// Connection string parsing tests for UseDeclareFetch / Fetch
// Uses RS_CONN_INFO::parseConnectString which is the same path used
// by SQLDriverConnect.
// ===========================================================================

class UseDeclareFetchConnStringTest : public ::testing::Test {
protected:
    RS_ENV_INFO envInfo;
    RS_CONN_INFO *pConn;

    void SetUp() override {
        memset(&envInfo, 0, sizeof(RS_ENV_INFO));
        pConn = new RS_CONN_INFO(&envInfo);
        pConn->pConnectProps = new RS_CONNECT_PROPS_INFO();
        pConn->pConnAttr = new RS_CONN_ATTR_INFO();
    }

    void TearDown() override {
        delete pConn->pConnAttr;
        delete pConn->pConnectProps;
        delete pConn;
    }

    // Parse a connection string (non-DSN path)
    void parseConnStr(const char *connStr) {
        pConn->parseConnectString(const_cast<char *>(connStr), SQL_NTS, FALSE, FALSE);
    }
};

// The parser only records what it saw; resolveSslPolicy decides, tested below.

TEST_F(UseDeclareFetchConnStringTest, EncryptionMethod_is_recorded) {
    parseConnStr("Driver=Amazon Redshift ODBC Driver;EncryptionMethod=2;");
    ASSERT_TRUE(pConn->pConnectProps->connEncryptionMethod.has_value());
    EXPECT_EQ(*pConn->pConnectProps->connEncryptionMethod, 2);
}

TEST_F(UseDeclareFetchConnStringTest, EM_alias_is_recorded) {
    parseConnStr("Driver=Amazon Redshift ODBC Driver;EM=1;");
    ASSERT_TRUE(pConn->pConnectProps->connEncryptionMethod.has_value());
    EXPECT_EQ(*pConn->pConnectProps->connEncryptionMethod, 1);
}

// A typo still counts as a choice, like an unreadable SSLMode: default mode, strict.
TEST_F(UseDeclareFetchConnStringTest, A_nonnumeric_EncryptionMethod_is_unreadable_and_strict) {
    parseConnStr("Driver=Amazon Redshift ODBC Driver;EncryptionMethod=abc;");
    ASSERT_TRUE(pConn->pConnectProps->connEncryptionMethod.has_value());
    EXPECT_EQ(*pConn->pConnectProps->connEncryptionMethod, RS_ENCRYPTION_METHOD_UNREADABLE);

    RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
    EXPECT_STREQ(pConn->pConnectProps->szSslMode, "verify-full");
    EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, PQ_SSL_MISMATCH_STRICT);
}

// Only a whole number is a value; a numeric prefix must not turn SSL off.
TEST(EncryptionMethodReadTest, reads_only_a_whole_number) {
    const struct {
        const char *text;
        int expected;
    } cases[] = {
        {"0", 0}, {"1", 1}, {"2", 2}, {"3", 3}, {" 2", 2}, {"2 ", 2}, {" 2 ", 2},
        {"+2", 2}, {"-1", -1},
        {"0oops", RS_ENCRYPTION_METHOD_UNREADABLE},
        {"1x", RS_ENCRYPTION_METHOD_UNREADABLE},
        {"2.0", RS_ENCRYPTION_METHOD_UNREADABLE},
        {"0x1", RS_ENCRYPTION_METHOD_UNREADABLE},
        {"2 2", RS_ENCRYPTION_METHOD_UNREADABLE},
        {"abc", RS_ENCRYPTION_METHOD_UNREADABLE},
        {" ", RS_ENCRYPTION_METHOD_UNREADABLE},
        {"99999999999", RS_ENCRYPTION_METHOD_UNREADABLE},
        {"-99999999999", RS_ENCRYPTION_METHOD_UNREADABLE},
    };
    for (const auto &c : cases) {
        EXPECT_EQ(RS_CONN_INFO::readEncryptionMethod(c.text), c.expected)
            << "text: [" << c.text << "]";
    }
}

TEST_F(UseDeclareFetchConnStringTest, A_numeric_prefix_EncryptionMethod_is_unreadable_and_strict) {
    for (const char *value : {"0oops", "1x", "3abc"}) {
        pConn->pConnectProps->connEncryptionMethod.reset();
        parseConnStr((std::string("Driver=Amazon Redshift ODBC Driver;EncryptionMethod=") +
                      value + ";").c_str());
        ASSERT_TRUE(pConn->pConnectProps->connEncryptionMethod.has_value()) << value;
        EXPECT_EQ(*pConn->pConnectProps->connEncryptionMethod,
                  RS_ENCRYPTION_METHOD_UNREADABLE) << value;

        RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
        EXPECT_STREQ(pConn->pConnectProps->szSslMode, "verify-full") << value;
        EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, PQ_SSL_MISMATCH_STRICT) << value;
    }
}

TEST_F(UseDeclareFetchConnStringTest, A_blank_EncryptionMethod_records_nothing) {
    parseConnStr("Driver=Amazon Redshift ODBC Driver;EncryptionMethod=;");
    EXPECT_FALSE(pConn->pConnectProps->connEncryptionMethod.has_value());
}

TEST_F(UseDeclareFetchConnStringTest, No_mode_keyword_records_nothing) {
    parseConnStr("Driver=Amazon Redshift ODBC Driver;");
    EXPECT_FALSE(pConn->pConnectProps->connSslMode.has_value());
    EXPECT_FALSE(pConn->pConnectProps->connEncryptionMethod.has_value());
}

// A blank SSLMode is how a caller drops the DSN's mode, so presence is recorded.
TEST_F(UseDeclareFetchConnStringTest, An_empty_SSLMode_is_recorded_as_present) {
    parseConnStr("Driver=Amazon Redshift ODBC Driver;SSLMode=;");
    ASSERT_TRUE(pConn->pConnectProps->connSslMode.has_value());
    EXPECT_EQ(*pConn->pConnectProps->connSslMode, "");
}

// ===========================================================================
// resolveSslPolicy runs after both sources are read, so parse order can't matter.
// ===========================================================================

namespace {

struct SslResolveCase {
    const char *name;
    const char *dsnSslMode;      // nullptr = key absent
    int dsnEncryptionMethod;     // -1 = key absent
    const char *connSslMode;     // nullptr = key absent
    int connEncryptionMethod;    // -1 = key absent
    const char *expectedMode;
    int expectedSource;
    int expectedPolicy;
};

const SslResolveCase kSslResolveCases[] = {
    // Nothing named: the built-in default, which tolerates a mismatch.
    {"nothing named", nullptr, -1, nullptr, -1,
     "verify-full", RS_SSL_MODE_SOURCE_BUILT_IN_DEFAULT,
     PQ_SSL_MISMATCH_ALLOW_TEMPORARILY},

    // A single source names the mode.
    {"connection SSLMode", nullptr, -1, "verify-ca", -1,
     "verify-ca", RS_SSL_MODE_SOURCE_CONN_SSL_MODE, PQ_SSL_MISMATCH_STRICT},
    {"connection EncryptionMethod", nullptr, -1, nullptr, 2,
     "verify-full", RS_SSL_MODE_SOURCE_CONN_ENCRYPTION_METHOD,
     PQ_SSL_MISMATCH_STRICT},
    {"DSN SSLMode", "require", -1, nullptr, -1,
     "require", RS_SSL_MODE_SOURCE_DSN_SSL_MODE, PQ_SSL_MISMATCH_STRICT},
    {"DSN EncryptionMethod", nullptr, 3, nullptr, -1,
     "require", RS_SSL_MODE_SOURCE_DSN_ENCRYPTION_METHOD,
     PQ_SSL_MISMATCH_STRICT},

    // The connection string outranks the DSN, whichever key each one uses.
    {"connection EncryptionMethod beats DSN SSLMode", "verify-ca", -1, nullptr, 2,
     "verify-full", RS_SSL_MODE_SOURCE_CONN_ENCRYPTION_METHOD,
     PQ_SSL_MISMATCH_STRICT},
    {"connection SSLMode beats DSN SSLMode", "verify-ca", -1, "require", -1,
     "require", RS_SSL_MODE_SOURCE_CONN_SSL_MODE, PQ_SSL_MISMATCH_STRICT},
    {"SSLMode outranks EncryptionMethod in one source", nullptr, -1, "require", 2,
     "require", RS_SSL_MODE_SOURCE_CONN_SSL_MODE, PQ_SSL_MISMATCH_STRICT},

    // A blank connection SSLMode drops the DSN's SSLMode but not an EncryptionMethod.
    {"empty SSLMode drops a DSN SSLMode", "verify-ca", -1, "", -1,
     "verify-full", RS_SSL_MODE_SOURCE_BUILT_IN_DEFAULT,
     PQ_SSL_MISMATCH_ALLOW_TEMPORARILY},
    {"empty SSLMode keeps a DSN EncryptionMethod", nullptr, 3, "", -1,
     "require", RS_SSL_MODE_SOURCE_DSN_ENCRYPTION_METHOD,
     PQ_SSL_MISMATCH_STRICT},
    {"empty SSLMode keeps a connection EncryptionMethod", nullptr, -1, "", 3,
     "require", RS_SSL_MODE_SOURCE_CONN_ENCRYPTION_METHOD,
     PQ_SSL_MISMATCH_STRICT},

    // An unreadable value is skipped when picking the mode but still forces strict,
    // so a typo can't relax a connection.
    {"invalid SSLMode names no mode but stays strict", nullptr, -1,
     "verify-fulll", -1, "verify-full", RS_SSL_MODE_SOURCE_BUILT_IN_DEFAULT,
     PQ_SSL_MISMATCH_STRICT},
    {"invalid EncryptionMethod names no mode but stays strict", nullptr, -1,
     nullptr, 99, "verify-full", RS_SSL_MODE_SOURCE_BUILT_IN_DEFAULT,
     PQ_SSL_MISMATCH_STRICT},
    {"invalid connection SSLMode drops the DSN mode and stays strict", "require",
     -1, "nonsense", -1, "verify-full", RS_SSL_MODE_SOURCE_BUILT_IN_DEFAULT,
     PQ_SSL_MISMATCH_STRICT},
    {"invalid connection EncryptionMethod falls back to the DSN", "require", -1,
     nullptr, 99, "require", RS_SSL_MODE_SOURCE_DSN_SSL_MODE,
     PQ_SSL_MISMATCH_STRICT},

    // A DSN's own mode is honoured, including the verify-ca that older releases
    // wrote into every DSN, so upgrading never changes how an existing DSN connects.
    {"DSN SSLMode=verify-ca is kept", "verify-ca", -1, nullptr, -1,
     "verify-ca", RS_SSL_MODE_SOURCE_DSN_SSL_MODE, PQ_SSL_MISMATCH_STRICT},
    {"DSN SSLMode=verify-ca is kept in any case", "VERIFY-CA", -1, nullptr, -1,
     "verify-ca", RS_SSL_MODE_SOURCE_DSN_SSL_MODE, PQ_SSL_MISMATCH_STRICT},
    {"DSN EncryptionMethod=1 is kept", nullptr, 1, nullptr, -1,
     "verify-ca", RS_SSL_MODE_SOURCE_DSN_ENCRYPTION_METHOD, PQ_SSL_MISMATCH_STRICT},
    {"DSN SSLMode outranks DSN EncryptionMethod", "verify-ca", 3, nullptr, -1,
     "verify-ca", RS_SSL_MODE_SOURCE_DSN_SSL_MODE, PQ_SSL_MISMATCH_STRICT},
    {"connection verify-full overrides a DSN verify-ca", "verify-ca", 1,
     "verify-full", -1, "verify-full", RS_SSL_MODE_SOURCE_CONN_SSL_MODE,
     PQ_SSL_MISMATCH_STRICT},
    {"connection EncryptionMethod=1 is honoured", nullptr, -1, nullptr, 1,
     "verify-ca", RS_SSL_MODE_SOURCE_CONN_ENCRYPTION_METHOD,
     PQ_SSL_MISMATCH_STRICT},
    {"a DSN verify-full is honoured", "verify-full", -1,
     nullptr, -1, "verify-full", RS_SSL_MODE_SOURCE_DSN_SSL_MODE,
     PQ_SSL_MISMATCH_STRICT},

    // disable is a named mode, so it's strict too.
    {"EncryptionMethod 0 names disable", nullptr, -1, nullptr, 0,
     "disable", RS_SSL_MODE_SOURCE_CONN_ENCRYPTION_METHOD,
     PQ_SSL_MISMATCH_STRICT},
};

} // namespace

TEST(SslModeResolveTest, resolves_every_combination_of_sources) {
    for (const SslResolveCase &c : kSslResolveCases) {
        RS_CONNECT_PROPS_INFO props;
        if (c.dsnSslMode) {
            props.dsnSslMode = std::string(c.dsnSslMode);
        }
        if (c.dsnEncryptionMethod != -1) {
            props.dsnEncryptionMethod = c.dsnEncryptionMethod;
        }
        if (c.connSslMode) {
            props.connSslMode = std::string(c.connSslMode);
        }
        if (c.connEncryptionMethod != -1) {
            props.connEncryptionMethod = c.connEncryptionMethod;
        }

        RS_CONN_INFO::resolveSslPolicy(&props);

        EXPECT_STREQ(props.szSslMode, c.expectedMode) << "case: " << c.name;
        EXPECT_EQ(props.iSslModeSource, c.expectedSource) << "case: " << c.name;
        EXPECT_EQ(props.iSslMismatchPolicy, c.expectedPolicy) << "case: " << c.name;
    }
}

// Checks the resolver's output, not the table's expectations: a mismatch is
// only ever tolerated when the mode came from the built-in default.
TEST(SslModeResolveTest, only_the_built_in_default_tolerates_a_mismatch) {
    for (const SslResolveCase &c : kSslResolveCases) {
        RS_CONNECT_PROPS_INFO props;
        if (c.dsnSslMode) {
            props.dsnSslMode = std::string(c.dsnSslMode);
        }
        if (c.dsnEncryptionMethod != -1) {
            props.dsnEncryptionMethod = c.dsnEncryptionMethod;
        }
        if (c.connSslMode) {
            props.connSslMode = std::string(c.connSslMode);
        }
        if (c.connEncryptionMethod != -1) {
            props.connEncryptionMethod = c.connEncryptionMethod;
        }

        RS_CONN_INFO::resolveSslPolicy(&props);

        bool tolerated =
            props.iSslMismatchPolicy == PQ_SSL_MISMATCH_ALLOW_TEMPORARILY;
        bool defaulted =
            props.iSslModeSource == RS_SSL_MODE_SOURCE_BUILT_IN_DEFAULT;

        // Not the reverse: an unreadable value can land on the default and stay strict.
        if (tolerated) {
            EXPECT_TRUE(defaulted)
                << "case: " << c.name
                << " -- a mismatch was tolerated for a mode the caller named";
        }
    }
}

/**
 * @brief Frees what a fixture's connection allocated, as ParseConnectStringTest
 *        does: RS_CONNECT_PROPS_INFO and RS_ENV_INFO have no destructors.
 * @param envInfo The fixture's environment; its attributes are freed.
 * @param pConn The fixture's connection; deleted on return.
 */
static void freeTestConnection(RS_ENV_INFO &envInfo, RS_CONN_INFO *pConn) {
    RS_CONNECT_PROPS_INFO *props = pConn->pConnectProps;
    RsFree(props->pConnectStr);
    props->pConnectStr = NULL;
    RsFree(props->pInitializationString);
    props->pInitializationString = NULL;
    if (props->pIamProps) {
        RsFree(props->pIamProps->pszJwt);
        props->pIamProps->pszJwt = NULL;
        RsFree(props->pIamProps);
        props->pIamProps = NULL;
    }
    RsFree(props->pHttpsProps);
    props->pHttpsProps = NULL;
    RsFree(props->pTcpProxyProps);
    props->pTcpProxyProps = NULL;
    RsFree(pConn->pConnAttr->pTraceFile);
    pConn->pConnAttr->pTraceFile = NULL;

    delete pConn->pConnAttr;
    delete pConn->pConnectProps;
    delete pConn;

    delete envInfo.pEnvAttr;
    envInfo.pEnvAttr = NULL;
}

// DSNs are read by readMoreConnectPropsFromRegistry: from a temp odbc.ini (ODBCINI) on
// Linux/macOS, and from a real registry DSN on Windows.
class SslModeDsnTest : public ::testing::Test {
protected:
    RS_ENV_INFO envInfo;
    RS_CONN_INFO *pConn = NULL;
#ifdef WIN32
    std::string writtenDsn;
#else
    std::string workdir;
    std::string savedOdbcIni;
    bool hadOdbcIni = false;
#endif

    /** @brief Creates the connection and, off Windows, a temp directory for odbc.ini. */
    void SetUp() override {
        pConn = new RS_CONN_INFO(&envInfo);
        pConn->pConnectProps = new RS_CONNECT_PROPS_INFO();
        pConn->pConnAttr = new RS_CONN_ATTR_INFO();
#ifndef WIN32
        const char *existing = getenv(ODBCINI_ENV_VAR);
        hadOdbcIni = existing != NULL;
        if (hadOdbcIni) {
            savedOdbcIni = existing;
        }

        char templ[] = "/tmp/rsodbc_dsn_XXXXXX";
        ASSERT_NE(mkdtemp(templ), (char *)NULL);
        workdir = templ;
#endif
    }

    /** @brief Removes the DSN the test wrote, restores ODBCINI and frees the connection. */
    void TearDown() override {
#ifdef WIN32
        if (!writtenDsn.empty()) {
            SQLWritePrivateProfileString(writtenDsn.c_str(), NULL, NULL, ODBC_INI);
        }
#else
        if (hadOdbcIni) {
            setenv(ODBCINI_ENV_VAR, savedOdbcIni.c_str(), 1);
        } else {
            unsetenv(ODBCINI_ENV_VAR);
        }
        if (!workdir.empty()) {
            unlink((workdir + "/odbc.ini").c_str());
            rmdir(workdir.c_str());
        }
#endif
        freeTestConnection(envInfo, pConn);
    }

    /**
     * @brief Saves a DSN as the only copy of dsn, in the registry on Windows and in a
     *        temp odbc.ini elsewhere, then reads it into the connection.
     * @param dsn The DSN name.
     * @param body "[dsn]" then one key=value per line.
     */
    void readDsn(const char *dsn, const std::string &body) {
#ifdef WIN32
        SQLWritePrivateProfileString(dsn, NULL, NULL, ODBC_INI);
        writtenDsn = dsn;
        size_t start = 0;
        while (start < body.size()) {
            size_t end = body.find('\n', start);
            std::string line = body.substr(start, end == std::string::npos ? std::string::npos : end - start);
            start = end == std::string::npos ? body.size() : end + 1;
            size_t eq = line.find('=');
            if (line.empty() || line[0] == '[' || eq == std::string::npos) {
                continue;
            }
            ASSERT_TRUE(SQLWritePrivateProfileString(dsn, line.substr(0, eq).c_str(),
                                                     line.substr(eq + 1).c_str(), ODBC_INI))
                << "could not write " << line << " to registry DSN " << dsn;
        }
#else
        std::string path = workdir + "/odbc.ini";
        FILE *fp = fopen(path.c_str(), "w");
        ASSERT_NE(fp, (FILE *)NULL);
        fputs(body.c_str(), fp);
        fclose(fp);
        setenv(ODBCINI_ENV_VAR, path.c_str(), 1);
#endif
        strncpy(pConn->pConnectProps->szDSN, dsn,
                sizeof(pConn->pConnectProps->szDSN) - 1);
        pConn->readMoreConnectPropsFromRegistry(FALSE);
    }
};

TEST_F(SslModeDsnTest, a_DSN_EncryptionMethod_is_recorded) {
    readDsn("emdsn",
            "[emdsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
            "Database=d\nEncryptionMethod=2\n");

    ASSERT_TRUE(pConn->pConnectProps->dsnEncryptionMethod.has_value());
    EXPECT_EQ(*pConn->pConnectProps->dsnEncryptionMethod, 2);

    RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, PQ_SSL_MISMATCH_STRICT)
        << "A DSN naming the mode through EncryptionMethod must fail a mismatch, "
           "not warn about it";
}

TEST_F(SslModeDsnTest, a_DSN_SSLMode_is_recorded) {
    readDsn("modedsn",
            "[modedsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
            "Database=d\nSSLMode=require\n");

    ASSERT_TRUE(pConn->pConnectProps->dsnSslMode.has_value());
    EXPECT_EQ(*pConn->pConnectProps->dsnSslMode, "require");

    RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
    EXPECT_STREQ(pConn->pConnectProps->szSslMode, "require");
    EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, PQ_SSL_MISMATCH_STRICT)
        << "A DSN naming the mode outright is the caller's choice";
}

// DSNs made by older releases keep the mode they carry after an upgrade.
TEST_F(SslModeDsnTest, an_upgraded_DSN_keeps_its_mode) {
    const char *const bodies[] = {
        // The old Linux/macOS odbc.ini template.
        "[olddsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
        "Database=d\nSSLMode=verify-ca\n",
        // What the old Windows dialog saved.
        "[olddsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
        "Database=d\nSSLMode=verify-ca\nEncryptionMethod=1\n",
    };
    for (const char *body : bodies) {
        pConn->pConnectProps->dsnSslMode.reset();
        pConn->pConnectProps->dsnEncryptionMethod.reset();
        readDsn("olddsn", body);

        RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
        EXPECT_STREQ(pConn->pConnectProps->szSslMode, "verify-ca")
            << "an upgraded DSN must keep its mode: " << body;
        EXPECT_EQ(pConn->pConnectProps->iSslModeSource,
                  RS_SSL_MODE_SOURCE_DSN_SSL_MODE) << body;
        EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy,
                  PQ_SSL_MISMATCH_STRICT) << body;
    }
}

TEST_F(SslModeDsnTest, a_DSN_EncryptionMethod_with_trailing_text_is_unreadable) {
    readDsn("typodsn",
            "[typodsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
            "Database=d\nEncryptionMethod=0oops\n");

    ASSERT_TRUE(pConn->pConnectProps->dsnEncryptionMethod.has_value());
    EXPECT_EQ(*pConn->pConnectProps->dsnEncryptionMethod, RS_ENCRYPTION_METHOD_UNREADABLE);

    RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
    EXPECT_STREQ(pConn->pConnectProps->szSslMode, "verify-full")
        << "0oops must not be read as 0, which disables SSL";
    EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, PQ_SSL_MISMATCH_STRICT);
}

TEST_F(SslModeDsnTest, a_nonnumeric_DSN_EncryptionMethod_is_unreadable_and_strict) {
    readDsn("typodsn",
            "[typodsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
            "Database=d\nEncryptionMethod=abc\n");

    ASSERT_TRUE(pConn->pConnectProps->dsnEncryptionMethod.has_value());
    EXPECT_EQ(*pConn->pConnectProps->dsnEncryptionMethod, RS_ENCRYPTION_METHOD_UNREADABLE);

    RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
    EXPECT_STREQ(pConn->pConnectProps->szSslMode, "verify-full");
    EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, PQ_SSL_MISMATCH_STRICT);
}

TEST_F(SslModeDsnTest, a_DSN_with_no_mode_key_stays_defaulted) {
    readDsn("plaindsn",
            "[plaindsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
            "Database=d\n");

    EXPECT_FALSE(pConn->pConnectProps->dsnSslMode.has_value());
    EXPECT_FALSE(pConn->pConnectProps->dsnEncryptionMethod.has_value());

    RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
    EXPECT_STREQ(pConn->pConnectProps->szSslMode, "verify-full");
    EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy,
              PQ_SSL_MISMATCH_ALLOW_TEMPORARILY)
        << "A DSN made from the shipped template names no mode, so it must keep "
           "the transitional warning instead of hard-failing";
}

// A DSN EncryptionMethod that fills the read buffer may be cut off, so it is never
// read as a number; the boundary is one character short of the buffer.
// Windows returns nothing for a value longer than the buffer, so the key reads as unset.
TEST_F(SslModeDsnTest, a_DSN_EncryptionMethod_that_fills_the_buffer_is_unreadable) {
    const std::string padded = "0" + std::string(70, ' ') + "oops";
    readDsn("longdsn", "[longdsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
                       "Database=d\nEncryptionMethod=" + padded + "\n");
#ifdef WIN32
    EXPECT_FALSE(pConn->pConnectProps->dsnEncryptionMethod.has_value());
#else
    ASSERT_TRUE(pConn->pConnectProps->dsnEncryptionMethod.has_value());
    EXPECT_EQ(*pConn->pConnectProps->dsnEncryptionMethod, RS_ENCRYPTION_METHOD_UNREADABLE)
        << "a cut-off value read as 0 would turn SSL off";
#endif

    RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
    EXPECT_STREQ(pConn->pConnectProps->szSslMode, "verify-full");
#ifdef WIN32
    EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, PQ_SSL_MISMATCH_ALLOW_TEMPORARILY);
#else
    EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, PQ_SSL_MISMATCH_STRICT);
#endif
}

TEST_F(SslModeDsnTest, a_DSN_EncryptionMethod_at_the_buffer_boundary) {
    // MAX_NUMBER_BUF_LEN - 2 characters still fit, so the number is read.
    const std::string fits = std::string(MAX_NUMBER_BUF_LEN - 3, '0') + "3";
    readDsn("edgedsn", "[edgedsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
                       "Database=d\nEncryptionMethod=" + fits + "\n");
    ASSERT_TRUE(pConn->pConnectProps->dsnEncryptionMethod.has_value());
    EXPECT_EQ(*pConn->pConnectProps->dsnEncryptionMethod, 3) << "value: " << fits;

    // One more character fills the buffer, so it is no longer trusted.
    const std::string fills = std::string(MAX_NUMBER_BUF_LEN - 2, '0') + "3";
    pConn->pConnectProps->dsnEncryptionMethod.reset();
    readDsn("edgedsn", "[edgedsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
                       "Database=d\nEncryptionMethod=" + fills + "\n");
    ASSERT_TRUE(pConn->pConnectProps->dsnEncryptionMethod.has_value());
    EXPECT_EQ(*pConn->pConnectProps->dsnEncryptionMethod, RS_ENCRYPTION_METHOD_UNREADABLE)
        << "value: " << fills;
}

// A DSN SSLMode longer than the buffer is read as a terminated string that names no mode.
// Windows returns nothing for it, so the key reads as unset.
TEST_F(SslModeDsnTest, a_DSN_SSLMode_that_fills_the_buffer_is_unreadable) {
    const std::string padded = "disable" + std::string(3000, ' ') + "x";
    readDsn("longdsn", "[longdsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
                       "Database=d\nSSLMode=" + padded + "\n");
#ifdef WIN32
    EXPECT_FALSE(pConn->pConnectProps->dsnSslMode.has_value());
#else
    ASSERT_TRUE(pConn->pConnectProps->dsnSslMode.has_value());
    EXPECT_LT(pConn->pConnectProps->dsnSslMode->size(), (size_t)MAX_IDEN_LEN)
        << "the value was not terminated inside the buffer";
#endif

    RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
    EXPECT_STREQ(pConn->pConnectProps->szSslMode, "verify-full") << "a cut-off value must not disable SSL";
#ifdef WIN32
    EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, PQ_SSL_MISMATCH_ALLOW_TEMPORARILY);
#else
    EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, PQ_SSL_MISMATCH_STRICT);
#endif
}

// A blank DSN key names nothing, so the mode stays defaulted.
TEST_F(SslModeDsnTest, blank_DSN_SSL_keys_stay_defaulted) {
    readDsn("blankdsn", "[blankdsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\n"
                        "Database=d\nSSLMode=\nEncryptionMethod=\n");
    EXPECT_FALSE(pConn->pConnectProps->dsnSslMode.has_value());
    EXPECT_FALSE(pConn->pConnectProps->dsnEncryptionMethod.has_value());

    RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
    EXPECT_STREQ(pConn->pConnectProps->szSslMode, "verify-full");
    EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, PQ_SSL_MISMATCH_ALLOW_TEMPORARILY);
}

// Test Connection sends the DSN's SSL keys in a connection string; it must reach the
// same mode and policy as using the saved DSN. Keys are picked as setup.c does: the
// dialog holds the DSN value or its table default, and only named keys go out.
TEST_F(SslModeDsnTest, test_connection_matches_the_saved_dsn) {
    struct DsnKeys { const char *sslMode; const char *encryptionMethod; };
    const DsnKeys cases[] = {
        {nullptr, nullptr}, {"verify-ca", nullptr}, {nullptr, "1"},
        {"verify-ca", "1"}, {"verify-full", "2"}, {"require", nullptr},
        {"disable", nullptr}, {nullptr, "3"},
    };
    for (const DsnKeys &keys : cases) {
        std::string body = "[cmpdsn]\nDriver=Amazon Redshift ODBC Driver\nHostName=h\nDatabase=d\n";
        if (keys.sslMode) {
            body += std::string("SSLMode=") + keys.sslMode + "\n";
        }
        if (keys.encryptionMethod) {
            body += std::string("EncryptionMethod=") + keys.encryptionMethod + "\n";
        }
        const int sslModeNamed = keys.sslMode != nullptr;
        const int encryptionMethodNamed = keys.encryptionMethod != nullptr;
        std::string testConnStr = "Driver=Amazon Redshift ODBC Driver";
        if (rs_dsn_ssl_key_is_named(RS_DSN_SSL_MODE_KEY, sslModeNamed, encryptionMethodNamed)) {
            testConnStr += std::string(";SSLMode=") + (keys.sslMode ? keys.sslMode : "verify-full");
        }
        if (rs_dsn_ssl_key_is_named(RS_DSN_ENCRYPTION_METHOD_KEY, sslModeNamed, encryptionMethodNamed)) {
            testConnStr += std::string(";EncryptionMethod=") + (keys.encryptionMethod ? keys.encryptionMethod : "2");
        }
        const std::string label = std::string("SSLMode=") + (keys.sslMode ? keys.sslMode : "<none>") +
                                  " EncryptionMethod=" + (keys.encryptionMethod ? keys.encryptionMethod : "<none>");

        pConn->resetConnectProps();
        readDsn("cmpdsn", body);
        RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
        const std::string savedMode = pConn->pConnectProps->szSslMode;
        const int savedPolicy = pConn->pConnectProps->iSslMismatchPolicy;

        pConn->resetConnectProps();
        pConn->parseConnectString(const_cast<char *>(testConnStr.c_str()), SQL_NTS, FALSE, FALSE);
        RS_CONN_INFO::resolveSslPolicy(pConn->pConnectProps);
        EXPECT_EQ(std::string(pConn->pConnectProps->szSslMode), savedMode)
            << "Test Connection picked a different mode for " << label;
        EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, savedPolicy)
            << "Test Connection picked a different policy for " << label;
    }
}

// A reused HDBC must not keep the previous connection's SSL mode.

class SslModeResetTest : public ::testing::Test {
protected:
    RS_ENV_INFO envInfo;
    RS_CONN_INFO *pConn;

    /** @brief Creates the connection. */
    void SetUp() override {
        pConn = new RS_CONN_INFO(&envInfo);
        pConn->pConnectProps = new RS_CONNECT_PROPS_INFO();
        pConn->pConnAttr = new RS_CONN_ATTR_INFO();
    }

    /** @brief Frees the connection. */
    void TearDown() override { freeTestConnection(envInfo, pConn); }
};

TEST_F(SslModeResetTest, reset_clears_a_previous_connections_mode) {
    // Leave a disabled mode behind from both a DSN and a connection string.
    strncpy(pConn->pConnectProps->szSslMode, "disable",
            sizeof(pConn->pConnectProps->szSslMode) - 1);
    pConn->pConnectProps->dsnSslMode = std::string("disable");
    pConn->pConnectProps->connEncryptionMethod = 0;
    pConn->pConnectProps->iSslMismatchPolicy = PQ_SSL_MISMATCH_STRICT;

    pConn->resetConnectProps();

    EXPECT_STREQ(pConn->pConnectProps->szSslMode, "")
        << "A reused handle must not inherit the previous connection's mode";
    EXPECT_EQ(pConn->pConnectProps->iEncryptionMethod, 2)
        << "After the reset the re-derived mode must be verify-full";
    EXPECT_FALSE(pConn->pConnectProps->dsnSslMode.has_value());
    EXPECT_FALSE(pConn->pConnectProps->connSslMode.has_value());
    EXPECT_FALSE(pConn->pConnectProps->dsnEncryptionMethod.has_value());
    EXPECT_FALSE(pConn->pConnectProps->connEncryptionMethod.has_value());
    EXPECT_EQ(pConn->pConnectProps->iSslMismatchPolicy, PQ_SSL_MISMATCH_STRICT)
        << "Nothing has been resolved yet on a fresh handle, so the strict "
           "policy is what a mismatch would get";
}

TEST_F(UseDeclareFetchConnStringTest, UseDeclareFetch_default_is_zero) {
    parseConnStr("Driver=Amazon Redshift ODBC Driver;");
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 0);
}

TEST_F(UseDeclareFetchConnStringTest, UseDeclareFetch_enabled_with_1) {
    parseConnStr("UseDeclareFetch=1;");
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
}

TEST_F(UseDeclareFetchConnStringTest, UseDeclareFetch_enabled_with_true) {
    parseConnStr("UseDeclareFetch=true;");
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
}

TEST_F(UseDeclareFetchConnStringTest, UseDeclareFetch_disabled_with_0) {
    parseConnStr("UseDeclareFetch=0;");
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 0);
}

TEST_F(UseDeclareFetchConnStringTest, UseDeclareFetch_disabled_with_false) {
    parseConnStr("UseDeclareFetch=false;");
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 0);
}

TEST_F(UseDeclareFetchConnStringTest, UseDeclareFetch_short_alias_UDF) {
    parseConnStr("UDF=1;");
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
}

TEST_F(UseDeclareFetchConnStringTest, UseDeclareFetch_case_insensitive) {
    parseConnStr("usedeclarefetch=1;");
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
}

TEST_F(UseDeclareFetchConnStringTest, Fetch_positive_value) {
    parseConnStr("Fetch=20000;");
    EXPECT_EQ(pConn->pConnectProps->iFetchSize, 20000);
}

TEST_F(UseDeclareFetchConnStringTest, Fetch_zero_value) {
    parseConnStr("Fetch=0;");
    EXPECT_EQ(pConn->pConnectProps->iFetchSize, 0);
}

TEST_F(UseDeclareFetchConnStringTest, Fetch_negative_clamped_to_zero) {
    parseConnStr("Fetch=-100;");
    EXPECT_EQ(pConn->pConnectProps->iFetchSize, 0);
}

TEST_F(UseDeclareFetchConnStringTest, Fetch_combined_with_UseDeclareFetch) {
    parseConnStr("UseDeclareFetch=1;Fetch=5000;");
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iFetchSize, 5000);
}

TEST_F(UseDeclareFetchConnStringTest, Fetch_combined_with_other_properties) {
    parseConnStr("Server=myhost;Port=5439;Database=dev;UseDeclareFetch=1;Fetch=10000;");
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iFetchSize, 10000);
}

// ===========================================================================
// Mutual exclusivity tests: UseDeclareFetch disables SCR and CSC.
// ===========================================================================

TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_UseDeclareFetch_disables_StreamingCursorRows) {
    parseConnStr("UseDeclareFetch=1;StreamingCursorRows=5000;");
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iStreamingCursorRows, 0);
}

TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_UseDeclareFetch_disables_CscEnable) {
    parseConnStr("UseDeclareFetch=1;CscEnable=1;");
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iCscEnable, 0);
}

TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_default_fetch_size_applied) {
    parseConnStr("UseDeclareFetch=1;");
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iFetchSize, RS_DEFAULT_FETCH_SIZE);
}

TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_order_independent_SCR_before_UDF) {
    parseConnStr("StreamingCursorRows=5000;UseDeclareFetch=1;");
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iStreamingCursorRows, 0);
}

TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_order_independent_CSC_before_UDF) {
    parseConnStr("CscEnable=1;UseDeclareFetch=1;Fetch=1000;");
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iCscEnable, 0);
    EXPECT_EQ(pConn->pConnectProps->iFetchSize, 1000);
}

TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_negative_fetch_gets_default) {
    parseConnStr("UseDeclareFetch=1;Fetch=-50;");
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iFetchSize, RS_DEFAULT_FETCH_SIZE);
}

TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_UDF_disabled_does_not_affect_SCR) {
    parseConnStr("UseDeclareFetch=0;StreamingCursorRows=5000;");
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 0);
    EXPECT_EQ(pConn->pConnectProps->iStreamingCursorRows, 5000);
}

TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_disables_CSC_and_SCR_together) {
    parseConnStr("CscEnable=1;StreamingCursorRows=5000;UseDeclareFetch=1;");
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iCscEnable, 0);
    EXPECT_EQ(pConn->pConnectProps->iStreamingCursorRows, 0);
}

TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_UDFFirst_disables_CSC_and_SCR) {
    parseConnStr("UseDeclareFetch=1;CscEnable=1;StreamingCursorRows=5000;");
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iCscEnable, 0);
    EXPECT_EQ(pConn->pConnectProps->iStreamingCursorRows, 0);
}

TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_zero_fetch_gets_default) {
    parseConnStr("UseDeclareFetch=1;Fetch=0;");
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iFetchSize, RS_DEFAULT_FETCH_SIZE);
}

// UseDeclareFetch must NOT change FetchRefCursor. Refcursor auto-expansion is a
// connection-wide behavior that portal fetch leaves untouched; portal eligibility
// is decided per statement in the execution path, not by this flag. So enabling
// UseDeclareFetch preserves whatever FetchRefCursor the connection had.
TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_UseDeclareFetch_preserves_default_FetchRefCursor) {
    parseConnStr("UseDeclareFetch=1;");
    // Simulate the runtime default of FetchRefCursor=1 (set in resetConnectProps).
    pConn->pConnectProps->iFetchRefCursor = 1;
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iFetchRefCursor, 1);
}

TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_UseDeclareFetch_preserves_explicit_FetchRefCursor) {
    parseConnStr("FetchRefCursor=1;UseDeclareFetch=1;");
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 1);
    EXPECT_EQ(pConn->pConnectProps->iFetchRefCursor, 1);
}

TEST_F(UseDeclareFetchConnStringTest, MutualExclusivity_UDF_disabled_does_not_affect_FetchRefCursor) {
    parseConnStr("UseDeclareFetch=0;");
    pConn->pConnectProps->iFetchRefCursor = 1;
    applyUseDeclareFetchExclusivity(pConn->pConnectProps);
    EXPECT_EQ(pConn->pConnectProps->iUseDeclareFetch, 0);
    EXPECT_EQ(pConn->pConnectProps->iFetchRefCursor, 1);
}

TEST(DriverConnectInputCopy, CopiesSqlNtsInputVerbatim) {
    const SQLCHAR input[] =
        "Driver=Redshift;Database=dev;StreamingCursorRows=0;CustomKey=value;";
    SQLCHAR output[sizeof(input)] = {0};
    bool truncated = true;

    size_t length = RS_copyDriverConnectInput(input, SQL_NTS, output,
                                              sizeof(output), &truncated);

    EXPECT_EQ(strlen((const char *)input), length);
    EXPECT_FALSE(truncated);
    EXPECT_STREQ((const char *)input, (const char *)output);
}

TEST(DriverConnectInputCopy, CopiesExplicitLengthWithoutReadingPastInput) {
    const SQLCHAR input[] = {'A', '=', '1', ';'};
    SQLCHAR output[5] = {0};
    bool truncated = true;

    size_t length = RS_copyDriverConnectInput(input, sizeof(input), output,
                                              sizeof(output), &truncated);

    EXPECT_EQ(sizeof(input), length);
    EXPECT_FALSE(truncated);
    EXPECT_EQ(0, memcmp(input, output, sizeof(input)));
    EXPECT_EQ('\0', output[sizeof(input)]);
}

TEST(DriverConnectInputCopy, ReportsTruncationAndFullLength) {
    const SQLCHAR input[] = "ExtraKeyword=preserved;";
    SQLCHAR output[8] = {0};
    bool truncated = false;

    size_t length = RS_copyDriverConnectInput(input, SQL_NTS, output,
                                              sizeof(output), &truncated);

    EXPECT_EQ(strlen((const char *)input), length);
    EXPECT_TRUE(truncated);
    EXPECT_EQ(sizeof(output) - 1, strlen((const char *)output));
    EXPECT_EQ('\0', output[sizeof(output) - 1]);
}

TEST(DriverConnectInputCopy, NullOutputIsLengthInquiryWithoutTruncation) {
    const SQLCHAR input[] = "Database=dev;";
    bool truncated = true;

    size_t length =
        RS_copyDriverConnectInput(input, SQL_NTS, NULL, 0, &truncated);

    EXPECT_EQ(strlen((const char *)input), length);
    EXPECT_FALSE(truncated);
}

TEST(DriverConnectInputCopy, ZeroLengthOutputReportsLengthAndTruncation) {
    const SQLCHAR input[] = "Database=dev;";
    SQLCHAR output = 'x';
    bool truncated = false;

    size_t length =
        RS_copyDriverConnectInput(input, SQL_NTS, &output, 0, &truncated);

    EXPECT_EQ(strlen((const char *)input), length);
    EXPECT_TRUE(truncated);
    EXPECT_EQ('x', output);
}

TEST(DriverConnectInputCopy, NullInputYieldsEmptyOutput) {
    SQLCHAR output[4] = {'x', 'x', 'x', 'x'};
    bool truncated = true;

    size_t length = RS_copyDriverConnectInput(NULL, SQL_NTS, output,
                                              sizeof(output), &truncated);

    EXPECT_EQ(0u, length);
    EXPECT_FALSE(truncated);
    EXPECT_EQ('\0', output[0]);
}

TEST(DriverConnectInputCopy, NegativeLengthOtherThanSqlNtsCopiesNothing) {
    const SQLCHAR input[] = "DSN=a;";
    SQLCHAR output[8] = {'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x'};
    bool truncated = true;

    size_t length = RS_copyDriverConnectInput(input, (SQLSMALLINT)-2, output,
                                              sizeof(output), &truncated);

    EXPECT_EQ(0u, length);
    EXPECT_FALSE(truncated);
    EXPECT_EQ('\0', output[0]);
}

// Wide copy: SQLWCHAR units, exact bytes, lengths in characters.

static std::vector<SQLWCHAR> makeWide(const std::vector<unsigned int> &cps) {
    std::vector<SQLWCHAR> out(cps.begin(), cps.end());
    out.push_back(0);
    return out;
}

TEST(DriverConnectInputCopyW, CopiesSqlNtsInputVerbatim) {
    std::vector<SQLWCHAR> input =
        makeWide({'D', 'S', 'N', '=', 'a', ';', 'K', '=', 'v', ';'});
    std::vector<SQLWCHAR> output(input.size(), 0xFFFF);
    bool truncated = true;

    size_t length = RS_copyDriverConnectInputW(
        input.data(), SQL_NTS, output.data(), (SQLSMALLINT)output.size(),
        &truncated, SQL_DD_CP_UTF16);

    EXPECT_EQ(input.size() - 1, length);
    EXPECT_FALSE(truncated);
    EXPECT_EQ(0, memcmp(input.data(), output.data(),
                        input.size() * sizeof(SQLWCHAR)));
}

TEST(DriverConnectInputCopyW, LengthIsInCharactersNotUtf8Bytes) {
    // Each character below encodes to multiple UTF-8 bytes.
    std::vector<SQLWCHAR> input = makeWide({'U', '=', 0x00E9, 0x4E2D, ';'});
    std::vector<SQLWCHAR> output(input.size(), 0);
    bool truncated = true;

    size_t length = RS_copyDriverConnectInputW(
        input.data(), SQL_NTS, output.data(), (SQLSMALLINT)output.size(),
        &truncated, SQL_DD_CP_UTF16);

    EXPECT_EQ(5u, length);
    EXPECT_FALSE(truncated);
    EXPECT_EQ(0, memcmp(input.data(), output.data(),
                        input.size() * sizeof(SQLWCHAR)));
}

TEST(DriverConnectInputCopyW, CopiesExplicitLengthWithoutReadingPastInput) {
    const SQLWCHAR input[] = {'A', '=', '1', ';'};
    SQLWCHAR output[5] = {1, 1, 1, 1, 1};
    bool truncated = true;

    size_t length = RS_copyDriverConnectInputW(
        input, (SQLSMALLINT)(sizeof(input) / sizeof(input[0])), output,
        (SQLSMALLINT)(sizeof(output) / sizeof(output[0])), &truncated,
        SQL_DD_CP_UTF16);

    EXPECT_EQ(4u, length);
    EXPECT_FALSE(truncated);
    EXPECT_EQ(0, memcmp(input, output, sizeof(input)));
    EXPECT_EQ(0, output[4]);
}

TEST(DriverConnectInputCopyW, ReportsTruncationAndFullLength) {
    std::vector<SQLWCHAR> input =
        makeWide({'E', 'x', 't', 'r', 'a', '=', 'k', 'e', 'p', 't', ';'});
    SQLWCHAR output[4] = {0};
    bool truncated = false;

    size_t length = RS_copyDriverConnectInputW(
        input.data(), SQL_NTS, output,
        (SQLSMALLINT)(sizeof(output) / sizeof(output[0])), &truncated,
        SQL_DD_CP_UTF16);

    EXPECT_EQ(input.size() - 1, length);
    EXPECT_TRUE(truncated);
    EXPECT_EQ(0, memcmp(input.data(), output, 3 * sizeof(SQLWCHAR)));
    EXPECT_EQ(0, output[3]);
}

TEST(DriverConnectInputCopyW, NullOutputIsLengthInquiryWithoutTruncation) {
    std::vector<SQLWCHAR> input = makeWide({'D', 'B', '=', 'd', ';'});
    bool truncated = true;

    size_t length = RS_copyDriverConnectInputW(input.data(), SQL_NTS, NULL, 0,
                                               &truncated, SQL_DD_CP_UTF16);

    EXPECT_EQ(5u, length);
    EXPECT_FALSE(truncated);
}

TEST(DriverConnectInputCopyW, ZeroLengthOutputReportsLengthAndTruncation) {
    std::vector<SQLWCHAR> input = makeWide({'D', 'B', '=', 'd', ';'});
    SQLWCHAR output = 'x';
    bool truncated = false;

    size_t length = RS_copyDriverConnectInputW(input.data(), SQL_NTS, &output,
                                               0, &truncated, SQL_DD_CP_UTF16);

    EXPECT_EQ(5u, length);
    EXPECT_TRUE(truncated);
    EXPECT_EQ('x', output);
}

TEST(DriverConnectInputCopyW, NullInputYieldsEmptyOutput) {
    SQLWCHAR output[4] = {'x', 'x', 'x', 'x'};
    bool truncated = true;

    size_t length = RS_copyDriverConnectInputW(NULL, SQL_NTS, output, 4,
                                               &truncated, SQL_DD_CP_UTF16);

    EXPECT_EQ(0u, length);
    EXPECT_FALSE(truncated);
    EXPECT_EQ(0, output[0]);
}

TEST(DriverConnectInputCopyW, NegativeLengthOtherThanSqlNtsCopiesNothing) {
    std::vector<SQLWCHAR> input = makeWide({'D', 'S', 'N', '=', 'a', ';'});
    SQLWCHAR output[8] = {'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x'};
    bool truncated = true;

    size_t length = RS_copyDriverConnectInputW(
        input.data(), (SQLSMALLINT)-2, output,
        (SQLSMALLINT)(sizeof(output) / sizeof(output[0])), &truncated,
        SQL_DD_CP_UTF16);

    EXPECT_EQ(0u, length);
    EXPECT_FALSE(truncated);
    EXPECT_EQ(0, output[0]);
}

// Wide copy at the 4 byte unit width. Buffers are uint32_t vectors so the
// tests are independent of the compile time SQLWCHAR width and the storage
// satisfies the 4 byte alignment the copy primitive requires.

static std::vector<uint32_t> makeWide32(const std::vector<char32_t> &cps) {
    std::vector<uint32_t> out(cps.begin(), cps.end());
    out.push_back(0);
    return out;
}

static const SQLWCHAR *asSqlwchar(const std::vector<uint32_t> &v) {
    return reinterpret_cast<const SQLWCHAR *>(v.data());
}

static SQLWCHAR *asSqlwchar(std::vector<uint32_t> &v) {
    return reinterpret_cast<SQLWCHAR *>(v.data());
}

TEST(DriverConnectInputCopyW, Utf32SqlNtsLengthCountsFourByteUnits) {
    // Read in 2 byte units, this string ends after the first character
    // because the upper half of each 4 byte unit is zero.
    std::vector<uint32_t> input =
        makeWide32({'D', 'S', 'N', '=', 'a', ';', 'K', '=', 'v', ';'});
    std::vector<uint32_t> output(input.size(), 0xFFFFFFFFu);
    bool truncated = true;

    size_t length = RS_copyDriverConnectInputW(
        asSqlwchar(input), SQL_NTS, asSqlwchar(output),
        (SQLSMALLINT)output.size(), &truncated, SQL_DD_CP_UTF32);

    EXPECT_EQ(10u, length);
    EXPECT_FALSE(truncated);
    EXPECT_EQ(input, output);
}

TEST(DriverConnectInputCopyW, Utf32ExplicitLengthCopiesFourByteUnits) {
    std::vector<uint32_t> input = makeWide32({'A', '=', '1', ';'});
    std::vector<uint32_t> output(5, 0xFFFFFFFFu);
    bool truncated = true;

    size_t length = RS_copyDriverConnectInputW(
        asSqlwchar(input), (SQLSMALLINT)4, asSqlwchar(output), (SQLSMALLINT)5,
        &truncated, SQL_DD_CP_UTF32);

    EXPECT_EQ(4u, length);
    EXPECT_FALSE(truncated);
    // Four characters copied, then a full 4 byte terminator.
    EXPECT_EQ(input, output);
}

TEST(DriverConnectInputCopyW, Utf32TruncationTerminatesWithFourByteUnit) {
    std::vector<uint32_t> input =
        makeWide32({'E', 'x', 't', 'r', 'a', '=', 'k', 'e', 'p', 't', ';'});
    std::vector<uint32_t> output(4, 0xFFFFFFFFu);
    bool truncated = false;

    size_t length = RS_copyDriverConnectInputW(
        asSqlwchar(input), SQL_NTS, asSqlwchar(output), (SQLSMALLINT)4,
        &truncated, SQL_DD_CP_UTF32);

    EXPECT_EQ(11u, length);
    EXPECT_TRUE(truncated);
    const std::vector<uint32_t> expected = {'E', 'x', 't', 0};
    EXPECT_EQ(expected, output);
}

// Negative control: the same 4 byte unit bytes measured as 2 byte units stop
// at the zero high half of the first character, so a caller that passes the
// wrong width sees length 1 instead of the full string.
TEST(DriverConnectInputCopyW, Utf32BytesMeasuredAsUtf16StopAtFirstUnit) {
    std::vector<uint32_t> input =
        makeWide32({'D', 'S', 'N', '=', 'a', ';', 'K', '=', 'v', ';'});
    bool truncated = true;

    size_t length = RS_copyDriverConnectInputW(asSqlwchar(input), SQL_NTS,
                                               NULL, 0, &truncated,
                                               SQL_DD_CP_UTF16);

    EXPECT_EQ(1u, length);
    EXPECT_FALSE(truncated);
}

// SQLDriverConnectW diagnostics: each call starts with an empty list.

TEST(DriverConnectDiagnostics, WideLengthErrorReplacesPriorRecords) {
    SQLHENV env = SQL_NULL_HANDLE;
    SQLHDBC dbc = SQL_NULL_HANDLE;
    ASSERT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env));
    ASSERT_EQ(SQL_SUCCESS, SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION,
                                         (SQLPOINTER)SQL_OV_ODBC3, 0));
    ASSERT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc));

    std::vector<SQLWCHAR> input = makeWide({'D', 'S', 'N', '=', 'a', ';', 0});
    SQLWCHAR output[16] = {0};
    SQLSMALLINT outputLength = 0;

    // Two consecutive calls rejected before any connection attempt.
    for (int call = 0; call < 2; ++call) {
        EXPECT_EQ(SQL_ERROR,
                  SQLDriverConnectW(dbc, NULL, input.data(), (SQLSMALLINT)-999,
                                    output, 16, &outputLength,
                                    SQL_DRIVER_NOPROMPT));
    }

    SQLINTEGER recordCount = -1;
    ASSERT_EQ(SQL_SUCCESS,
              SQLGetDiagField(SQL_HANDLE_DBC, dbc, 0, SQL_DIAG_NUMBER,
                              &recordCount, SQL_IS_INTEGER, NULL));
    EXPECT_EQ(1, recordCount);

    SQLCHAR sqlState[6] = {0};
    SQLINTEGER nativeError = 0;
    SQLCHAR message[256] = {0};
    SQLSMALLINT messageLength = 0;
    ASSERT_EQ(SQL_SUCCESS,
              SQLGetDiagRec(SQL_HANDLE_DBC, dbc, 1, sqlState, &nativeError,
                            message, sizeof(message), &messageLength));
    EXPECT_STREQ("HY090", (const char *)sqlState);
    EXPECT_EQ(SQL_NO_DATA,
              SQLGetDiagRec(SQL_HANDLE_DBC, dbc, 2, sqlState, &nativeError,
                            message, sizeof(message), &messageLength));

    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
}

// RS_appendSuppliedCredentialsToConnStr emits only pre-authentication credentials.

TEST(SuppliedCredentialsConnStr, OmitsCredentialsResolvedDuringConnect) {
    RS_CONNECT_PROPS_INFO props;
    props.iUserKeyWordType = SHORT_NAME_KEYWORD;
    props.iPasswordKeyWordType = SHORT_NAME_KEYWORD;

    // Auth resolved the credentials; the application supplied none.
    strncpy(props.szUser, "IAMA:resolved_user", sizeof(props.szUser) - 1);
    strncpy(props.szPassword, "temporary-secret", sizeof(props.szPassword) - 1);
    props.szOrigUser[0] = '\0';
    props.szOrigPassword[0] = '\0';

    char out[1024];
    out[0] = '\0';
    RS_appendSuppliedCredentialsToConnStr(&props, out, sizeof(out));

    std::string result(out);
    EXPECT_EQ(result.find("UID="), std::string::npos);
    EXPECT_EQ(result.find("PWD="), std::string::npos);
    EXPECT_EQ(result.find("resolved_user"), std::string::npos);
    EXPECT_EQ(result.find("temporary-secret"), std::string::npos);
}

TEST(SuppliedCredentialsConnStr, EchoesCredentialsSuppliedByApplication) {
    RS_CONNECT_PROPS_INFO props;
    props.iUserKeyWordType = SHORT_NAME_KEYWORD;
    props.iPasswordKeyWordType = SHORT_NAME_KEYWORD;

    // Supplied and resolved values differ; only supplied ones are echoed.
    strncpy(props.szUser, "IAMA:resolved_user", sizeof(props.szUser) - 1);
    strncpy(props.szOrigUser, "appuser", sizeof(props.szOrigUser) - 1);
    strncpy(props.szPassword, "resolved-temp-secret", sizeof(props.szPassword) - 1);
    strncpy(props.szOrigPassword, "apppw", sizeof(props.szOrigPassword) - 1);

    char out[1024];
    out[0] = '\0';
    RS_appendSuppliedCredentialsToConnStr(&props, out, sizeof(out));

    std::string result(out);
    EXPECT_STREQ(out, "UID=appuser;PWD=apppw;");
    EXPECT_EQ(result.find("resolved_user"), std::string::npos);
    EXPECT_EQ(result.find("resolved-temp-secret"), std::string::npos);
}

TEST(SuppliedCredentialsConnStr, UsesLongKeywordFormWhenConfigured) {
    RS_CONNECT_PROPS_INFO props;
    props.iUserKeyWordType = SHORT_NAME_KEYWORD + 1;      // long-name form
    props.iPasswordKeyWordType = SHORT_NAME_KEYWORD + 1;  // long-name form

    strncpy(props.szOrigUser, "appuser", sizeof(props.szOrigUser) - 1);
    strncpy(props.szOrigPassword, "apppw", sizeof(props.szOrigPassword) - 1);

    char out[1024];
    out[0] = '\0';
    RS_appendSuppliedCredentialsToConnStr(&props, out, sizeof(out));

    EXPECT_STREQ(out, "LogonID=appuser;Password=apppw;");
}

TEST(SuppliedCredentialsConnStr, ResetScrubsRetainedCredentials) {
    RS_ENV_INFO env;
    RS_CONN_INFO conn(&env);
    RS_CONNECT_PROPS_INFO props;
    conn.pConnectProps = &props;

    const char user[] = "appuser";
    const char password[] = "app-durable-secret";
    strncpy(props.szOrigUser, user, sizeof(props.szOrigUser) - 1);
    strncpy(props.szOrigPassword, password, sizeof(props.szOrigPassword) - 1);
    strncpy(props.szPassword, password, sizeof(props.szPassword) - 1);

    conn.resetConnectProps();

    // Full buffers must be scrubbed, not just the first byte.
    for (size_t i = 0; i < sizeof(user); i++) {
        EXPECT_EQ('\0', props.szOrigUser[i]) << "szOrigUser byte " << i;
    }
    for (size_t i = 0; i < sizeof(password); i++) {
        EXPECT_EQ('\0', props.szOrigPassword[i]) << "szOrigPassword byte " << i;
        EXPECT_EQ('\0', props.szPassword[i]) << "szPassword byte " << i;
    }

    // props is stack-owned; detach it before conn goes out of scope.
    conn.pConnectProps = NULL;
}

TEST(SuppliedCredentialsConnStr, DestructorScrubsCredentials) {
    const char password[] = "app-durable-secret";
    alignas(RS_CONNECT_PROPS_INFO) unsigned char
        storage[sizeof(RS_CONNECT_PROPS_INFO)];

    RS_CONNECT_PROPS_INFO *props = new (storage) RS_CONNECT_PROPS_INFO();
    strncpy(props->szOrigPassword, password, sizeof(props->szOrigPassword) - 1);
    strncpy(props->szPassword, password, sizeof(props->szPassword) - 1);

    // Record the buffer offsets so the raw storage can be inspected afterwards.
    size_t origOffset =
        reinterpret_cast<unsigned char *>(props->szOrigPassword) - storage;
    size_t passwordOffset =
        reinterpret_cast<unsigned char *>(props->szPassword) - storage;

    props->~RS_CONNECT_PROPS_INFO();

    // The destructor must scrub the buffers on every teardown path.
    for (size_t i = 0; i < sizeof(password); i++) {
        EXPECT_EQ(0, storage[origOffset + i]) << "szOrigPassword byte " << i;
        EXPECT_EQ(0, storage[passwordOffset + i]) << "szPassword byte " << i;
    }
}

/*====================================================================================================================================================*/

// SSLMode/EncryptionMethod logic of the Windows DSN dialog, which builds on every platform.
namespace {

// Combo index equals the EncryptionMethod value.
const int EM_DISABLE = 0;
const int EM_VERIFY_CA = 1;
const int EM_VERIFY_FULL = 2;
const int EM_REQUIRE = 3;

// What the setup table reads for EncryptionMethod when the DSN lacks it.
const int EM_TABLE_DEFAULT = EM_VERIFY_FULL;

struct OpenCase {
    const char *name;
    bool sslModeNamed;
    const char *sslMode;
    int encryptionMethod;
    int expectedIndex;
};

const OpenCase kOpenCases[] = {
    {"names_neither_key", false, "verify-full", EM_TABLE_DEFAULT, EM_VERIFY_FULL},
    {"names_only_encryption_method_3", false, "verify-full", EM_REQUIRE, EM_REQUIRE},
    {"names_only_sslmode_require", true, "require", EM_TABLE_DEFAULT, EM_REQUIRE},
    {"names_only_sslmode_disable", true, "DISABLE", EM_TABLE_DEFAULT, EM_DISABLE},
    {"sslmode_outranks_encryption_method", true, "verify-full", EM_REQUIRE, EM_VERIFY_FULL},
    {"sslmode_the_combo_cannot_show", true, "prefer", EM_REQUIRE, EM_REQUIRE},
    // DSNs saved by older releases open on the mode they carry.
    {"old_dsn_encryption_method_1", false, "verify-full", EM_VERIFY_CA, EM_VERIFY_CA},
    {"old_dsn_sslmode_verify_ca", true, "verify-ca", EM_TABLE_DEFAULT, EM_VERIFY_CA},
    {"old_windows_dsn_with_both_keys", true, "verify-ca", EM_VERIFY_CA, EM_VERIFY_CA},
};

/**
 * @brief Prints the case name on failure instead of raw bytes.
 * @param c The case.
 * @param os Stream gtest prints to.
 */
void PrintTo(const OpenCase &c, std::ostream *os) { *os << c.name; }

class DsnSetupSslTabOpen : public ::testing::TestWithParam<OpenCase> {};

} // namespace

/*====================================================================================================================================================*/

// The SSL tab opens on the mode the DSN resolves to, so an untouched save
// doesn't change it.
TEST_P(DsnSetupSslTabOpen, shows_the_mode_the_dsn_resolves_to)
{
    const OpenCase &c = GetParam();

    EXPECT_EQ(rs_dsn_em_index_on_open(c.sslModeNamed, c.sslMode, c.encryptionMethod),
              c.expectedIndex)
        << c.name << ": SSLMode " << (c.sslModeNamed ? c.sslMode : "(absent)")
        << ", EncryptionMethod " << c.encryptionMethod;
}

INSTANTIATE_TEST_SUITE_P(DsnSetup, DsnSetupSslTabOpen, ::testing::ValuesIn(kOpenCases),
                         [](const ::testing::TestParamInfo<OpenCase> &info) {
                             return std::string(info.param.name);
                         });

/*====================================================================================================================================================*/

// Adding an SSLMode here would outrank EncryptionMethod=1 and change the mode.
TEST(DsnSetupSslKeys, a_single_key_dsn_keeps_only_its_own_key)
{
    const int sslModeNamed = 0;
    const int encryptionMethodNamed = 1;

    EXPECT_FALSE(rs_dsn_ssl_key_is_named(RS_DSN_SSL_MODE_KEY, sslModeNamed,
                                         encryptionMethodNamed))
        << "SSLMode was not in the DSN, so it must not be written or tested";
    EXPECT_TRUE(rs_dsn_ssl_key_is_named(RS_DSN_ENCRYPTION_METHOD_KEY, sslModeNamed,
                                        encryptionMethodNamed))
        << "EncryptionMethod was in the DSN, so it must be kept";
}

// Writing either key would pin verify-full and drop the mismatch allowance.
TEST(DsnSetupSslKeys, a_dsn_naming_neither_key_gains_neither)
{
    EXPECT_FALSE(rs_dsn_ssl_key_is_named(RS_DSN_SSL_MODE_KEY, 0, 0))
        << "an unnamed SSLMode must not be written or tested at its default";
    EXPECT_FALSE(rs_dsn_ssl_key_is_named(RS_DSN_ENCRYPTION_METHOD_KEY, 0, 0))
        << "an unnamed EncryptionMethod must not be written or tested at its default";
}

TEST(DsnSetupSslKeys, keys_are_matched_without_regard_to_case)
{
    EXPECT_FALSE(rs_dsn_ssl_key_is_named("sslmode", 0, 1))
        << "the setup table compares attribute names case-insensitively";
    EXPECT_FALSE(rs_dsn_ssl_key_is_named("ENCRYPTIONMETHOD", 1, 0))
        << "the setup table compares attribute names case-insensitively";
}

TEST(DsnSetupSslKeys, other_attributes_are_unaffected)
{
    EXPECT_TRUE(rs_dsn_ssl_key_is_named("HostName", 0, 0))
        << "only the two SSL mode keys are gated";
    EXPECT_TRUE(rs_dsn_ssl_key_is_named("TLSMethod", 0, 0))
        << "only the two SSL mode keys are gated";
}
