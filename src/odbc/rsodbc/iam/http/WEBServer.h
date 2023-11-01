#pragma once

#include "Parser.h"
#include "Selector.h"
#include "Socket.h"

#include "rs_string.h"
#include "rslog.h"

#include <thread>
#include <atomic>

/*
* This class is used to launch the HTTP WEB server in separate thread
* to wait for the redirect response from the /oauth2/authorize and
* extract the authorization code from it.
*/
class WEBServer
{
    public:
        WEBServer(            rs_string& state,
            rs_string& port,
            rs_string& timeout);
        
        ~WEBServer() = default;
        
        /*
        * Launch the HTTP WEB server in separate thread.
        */
        void LaunchServer();
        
        /*
        * Wait until HTTP WEB server is finished.
        */
        void Join();
        
        /*
        * Extract the authorization code from response.
        */
        rs_string GetCode() const;
        
        /*
        * Get port where server is listening.
        */
        int GetListenPort() const;
        
        /*
        * Extract the SAML Assertion from response.
        */
        rs_string GetSamlResponse() const;
        
        /*
        * If server is listening for connections return true, otherwise return false.
        */
        bool IsListening() const;
        
        /*
        * If timeout happened return true, otherwise return false.
        */
        bool IsTimeout() const;
        
    private:
        /*
        * Main HTTP WEB server function that perform initialization and
        * listen for incoming connections for specified time by user.
        */
        void ListenerThread();
                
        /*
        * If incoming connection is available call HandleConnection.
        */
        void Listen();
                
        /*
        * Launch parser if incoming connection is acceptable.
        */
        void HandleConnection();
                
        /*
        * Perform socket preparation to launch the HTTP WEB server.
        */
        bool WEBServerInit();

        rs_string state_;
        rs_string port_;
        int timeout_;
        rs_string code_;
        rs_string saml_;
        std::thread thread_;
        Selector selector_;
        Parser parser_;
        Socket listen_socket_;
        int listen_port_;
        int connections_counter_;
        std::atomic<bool> listening_;           
};

////////////////////////////////////////////////////////////////////////////////////////////////////
inline WEBServer::WEBServer( rs_string& state,
    rs_string& port, rs_string& timeout) :
        state_(state),
        port_(port),
        timeout_(std::stoi(timeout)),
        selector_(),
        parser_(),
        listen_socket_(),
        listen_port_(0),
        connections_counter_(0),
        listening_(false)
{
    ; // Do nothing.
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline void WEBServer::LaunchServer()
{
  RS_LOG_DEBUG("IAM", "WEBServer::LaunchServer");
    
    // Create thread to launch the server to listen the incoming connection.
    // Waiting for the redirect response from the /oauth2/authorize.
    thread_ = std::thread(&WEBServer::ListenerThread, this);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline void WEBServer::Join()
{
  RS_LOG_DEBUG("IAM", "WEBServer::Join");

    if (thread_.joinable())
    {
        thread_.join();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline rs_string WEBServer::GetCode() const
{
  RS_LOG_DEBUG("IAM", "WEBServer::GetCode");
    
    return code_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline int WEBServer::GetListenPort() const
{
  RS_LOG_DEBUG("IAM", "WEBServer::GetListenPort");

    return listen_port_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline rs_string WEBServer::GetSamlResponse() const
{
  RS_LOG_DEBUG("IAM", "WEBServer::GetSamlResponse");
    
    return saml_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool WEBServer::IsListening() const
{
  RS_LOG_DEBUG("IAM", "WEBServer::IsListening");

    return listening_.load();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool WEBServer::IsTimeout() const
{
  RS_LOG_DEBUG("IAM", "WEBServer::IsTimeout");

    return connections_counter_ > 0 ? false : true;
}
