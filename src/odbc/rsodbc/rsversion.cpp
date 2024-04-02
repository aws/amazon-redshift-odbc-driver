#include <rslog.h>
#include <rsversion.h>

#include <aws/core/VersionConfig.h> // For AWS SDK version
#include <openssl/opensslv.h>       // For OpenSSL version
#include <ares.h>                   // For c-ares version
#ifdef LINUX
#include <curl/curl.h>          // For libcurl version
#include <krb5.h>               // For Kerberos version
#include <nghttp2/nghttp2ver.h> // For nghttp2 version
#include <s2n.h>                // For s2n version
#include <zlib.h>               // For zlib version
#endif
#include <algorithm>
#include <map>


namespace {
const char *LOG_CAT = "RSVER";
} // namespace

namespace rsodbc {
static const std::map<std::string, std::string> dependencyVersions = {
    {"openssl", OPENSSL_VERSION_TEXT},
    {"aws-sdk-cpp", AWS_SDK_VERSION_STRING},
    {"c-ares", ares_version(NULL)},
#ifdef LINUX
    {"curl", curl_version()},
    {"nghttp2", NGHTTP2_VERSION},
    {"zlib", ZLIB_VERSION},
/* TODO: S2N, kerberos */
#endif
};

} // namespace rsodbc

std::string rsodbcVersion() { return ODBC_DRIVER_VERSION_FULL; }

std::string rsodbcDependencyVersion(const std::string &dependency) {
    static std::string errorMsgStatic;
    auto it = rsodbc::dependencyVersions.find(dependency);
    if (it == rsodbc::dependencyVersions.end()) {
        errorMsgStatic = "Dependency '" + dependency + "' not found";
        return errorMsgStatic;
    }
    return it->second;
}

int getVersionedDependencyNames(std::vector<std::string> &dependencies) {
    std::for_each(rsodbc::dependencyVersions.begin(),
                  rsodbc::dependencyVersions.end(),
                  [&](auto &item) { dependencies.push_back(item.first); });
    return rsodbc::dependencyVersions.size();
}

void logAllRsodbcVersions() {
    RS_LOG_INFO(LOG_CAT, "ODBC-%s", rsodbcVersion().c_str());
    for (auto &item : rsodbc::dependencyVersions) {
        RS_LOG_INFO(LOG_CAT, "%s-%s", item.first.c_str(), item.second.c_str());
    }
}