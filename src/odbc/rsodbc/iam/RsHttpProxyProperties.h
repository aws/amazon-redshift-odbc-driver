#ifndef _RS_HTTP_PROXY_PROPERTIES_H_
#define _RS_HTTP_PROXY_PROPERTIES_H_

#include "RsUidPwdProperties.h"

    /// Prefix used to identify custom HTTP headers from the DSN or connection string.
    static const char* RS_CUST_HTTP_HEADER_PREFIX = "http.header.";

    /// Custom HTTP header prefix length.
    #define RS_CUST_HTTP_HEADER_PREFIX_LEN 12

    /// The connection key for looking up HTTPPath setting.
    #define RS_HTTP_PATH_KEY L"HTTPPath"

    /// The connection key for looking up UseProxy setting.
    #define RS_USE_PROXY_KEY L"UseProxy"

    /// The connection key for looking up ProxyHost setting.
    #define RS_PROXY_HOST_KEY L"ProxyHost"

    /// The connection key for looking up ProxyPort setting.
    #define RS_PROXY_PORT_KEY L"ProxyPort"

    /// The connection key for looking up ProxyUID setting.
    #define RS_PROXY_UID_KEY L"ProxyUID"

    /// The connection key for looking up ProxyPWD.
    #define RS_PROXY_PWD_KEY L"ProxyPWD"

    /// The connection key for looking up ProxyEncryptedPWD.
    #define RS_PROXY_ENCRYPTED_PWD_KEY L"ProxyEncryptedPWD"

    /// The connection key for looking up EnableCookies.
    #define RS_ENABLE_COOKIES_KEY L"EnableCookies"

    /// The connection key for looking up CookieJar.
    #define RS_COOKIE_JAR_KEY L"CookieJar"

    /// The default value for useProxy.
    #define RS_DEFAULT_USE_PROXY false

    /// The connection key for looking up UserAgentEntry.
    #define RS_USER_AGENT_ENTRY_KEY L"UserAgentEntry"

    /// The connection key for looking up EnableCurlDebugLogging.
    #define RS_ENABLE_CURL_DEBUG_LOGGING_KEY "EnableCurlDebugLogging"

    // The proxy authentication type. Currently only no authentication and basic are supported.
    enum RSProxyAuthType
    {
        RS_PROXY_AUTH_NOAUTH = 0,
        RS_PROXY_AUTH_BASIC = 1
    };

    /// The default proxy authentication type.
    #define RS_DEFAULT_PROXY_AUTH_TYPE RS_PROXY_AUTH_NOAUTH


  /// @brief Captures information on how HTTP proxy configurations are treated.
  struct RsHttpProxyProperties
  {
    public:

      explicit RsHttpProxyProperties(
          const rs_wstring& in_useProxyKey = rs_wstring(RS_USE_PROXY_KEY),
          const rs_wstring& in_proxyHostKey = rs_wstring(RS_PROXY_HOST_KEY),
          const rs_wstring& in_proxyPortKey = rs_wstring(RS_PROXY_PORT_KEY),
          const rs_wstring& in_uidKey = rs_wstring(RS_PROXY_UID_KEY),
          const rs_wstring& in_pwdKey = rs_wstring(RS_PROXY_PWD_KEY),
          const rs_wstring& in_encryptedPwdKey = rs_wstring(RS_PROXY_ENCRYPTED_PWD_KEY),
          bool in_uidPwdRequired = false,
          bool in_enabledByDefault = false) :
              m_uidPwdProps(
                  in_uidKey,
                  in_pwdKey,
//                  in_encryptedPwdKey,
                  in_uidPwdRequired),
              m_useProxyKey(in_useProxyKey),
              m_proxyHostKey(in_proxyHostKey),
              m_proxyPortKey(in_proxyPortKey),
              m_enabledByDefault(in_enabledByDefault)
      {
          ; // Do nothing.
      }

      /// @breif Constructor.
      ///
      /// @param in_rhs                   The DSUidPwdProperties_ to copy from.
      explicit RsHttpProxyProperties(const RsHttpProxyProperties& in_rhs) :
          m_uidPwdProps(in_rhs.m_uidPwdProps),
          m_useProxyKey(in_rhs.m_useProxyKey),
          m_proxyHostKey(in_rhs.m_proxyHostKey),
          m_proxyPortKey(in_rhs.m_proxyPortKey),
          m_enabledByDefault(in_rhs.m_enabledByDefault)
      {
          ; // Do nothing.
      }

      /// @brief Destructor.
      ~RsHttpProxyProperties()
      {
          ; // Do nothing.
      }


      // The UID and PWD properties for basic proxy authentication.
      RsUidPwdProperties m_uidPwdProps;

      // The key name to use for the connection setting for controlling whether to use proxy.
      const rs_wstring m_useProxyKey;

      // The key name to use for the proxy host connection setting.
      const rs_wstring m_proxyHostKey;

      // The key name to use for the proxy port connection setting.
      const rs_wstring m_proxyPortKey;

      // Indicates whether proxy is enabled by default.
      const bool m_enabledByDefault;

    private:
        // Hidden default constructor.
        RsHttpProxyProperties();

        // Hidden assignment operator.
        const RsHttpProxyProperties& operator=(const RsHttpProxyProperties&);
  };

#endif
