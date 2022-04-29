
#ifndef _RS_HTTP_PROXY_SETTINGS_H_
#define _RS_HTTP_PROXY_SETTINGS_H_

#include "RsHttpProxyProperties.h"
#include "RsUidPwdSettings.h"

  /// @brief Proxy configuration related connection settings.
  struct RsHttpProxySettings
  {
      /// @brief Constructor.
      RsHttpProxySettings() :
          m_proxyAuthType(RS_DEFAULT_PROXY_AUTH_TYPE),
          m_useProxy(RS_DEFAULT_USE_PROXY)
      {
          ; // Do nothing.
      }

      // The UID PWD configuration for Basic proxy authentication.
      RsUidPwdSettings m_uidPwdSettings;

      // The host of the proxy server to connect to.
      rs_string m_proxyHost;

      // The port of the proxy server to connect to.
      short m_proxyPort;

      // The proxy authentication type.
      RSProxyAuthType m_proxyAuthType;

      // Determines if driver is connecting through proxy server.
      bool m_useProxy;
  };

#endif

