#pragma once

#include "Parser.h"
#include "Selector.h"
#include "Socket.h"

#include "rs_string.h"
#include "RsLogger.h"

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
        WEBServer(RsLogger* in_log,
            rs_string& state,
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

        RsLogger *logger_;
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
inline WEBServer::WEBServer(RsLogger *in_log, rs_string& state,
    rs_string& port, rs_string& timeout) :
        logger_(in_log),
        state_(state),
        port_(port),
        timeout_(std::stoi(timeout)),
        selector_(in_log),
        parser_(in_log),
        listen_socket_(in_log),
        listen_port_(0),
        connections_counter_(0),
        listening_(false)
{
    ; // Do nothing.
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline void WEBServer::LaunchServer()
{
  logger_->log("WEBServer::LaunchServer");
    
    // Create thread to launch the server to listen the incoming connection.
    // Waiting for the redirect response from the /oauth2/authorize.
    thread_ = std::thread(&WEBServer::ListenerThread, this);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline void WEBServer::Join()
{
  logger_->log("WEBServer::Join");

    if (thread_.joinable())
    {
        thread_.join();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline rs_string WEBServer::GetCode() const
{
  logger_->log("WEBServer::GetCode");
    
    return code_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline int WEBServer::GetListenPort() const
{
  logger_->log("WEBServer::GetListenPort");

    return listen_port_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline rs_string WEBServer::GetSamlResponse() const
{
  logger_->log("WEBServer::GetSamlResponse");
    
    return saml_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool WEBServer::IsListening() const
{
  logger_->log( "WEBServer::IsListening");

    return listening_.load();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool WEBServer::IsTimeout() const
{
  logger_->log("WEBServer::IsTimeout");

    return connections_counter_ > 0 ? false : true;
}
