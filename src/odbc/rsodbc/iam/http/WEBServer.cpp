#include "WEBServer.h"
#include "SocketStream.h"
#include "HtmlResponse.h"

#include <chrono>
#include <exception>
#include <array>
#include <sstream>
#include <fstream>
#include <streambuf>

////////////////////////////////////////////////////////////////////////////////////////////////////
bool WEBServer::WEBServerInit()
{
    RS_LOG_DEBUG("IAMHTTP", "WEBServer::WEBServerInit");
    
    // Prepare the environment for get the socket description.
    try
    {
        listen_socket_.PrepareListenSocket(port_);
        listen_socket_.Register(selector_);
        listen_port_ = listen_socket_.GetListenPort();
    }
    catch (std::exception& e)
    {
        RS_LOG_DEBUG("IAMHTTP", "WEBServer::WEBServerInit %s", e.what());

        return false;
    }
    
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void WEBServer::HandleConnection()
{
    RS_LOG_DEBUG("IAMHTTP", "WEBServer::HandleConnection");
    
    /* Trying to accept the pending incoming connection. */
    Socket ssck(listen_socket_.Accept());
    
    ++connections_counter_;
    
    if (ssck.SetNonBlocking())
    {
        SocketStream socket_buffer(ssck);
        std::istream socket_stream(&socket_buffer);

        STATUS status = parser_.Parse(socket_stream);

        if (status == STATUS::SUCCEED)
        {
            ssck.Send(ValidResponse.c_str(), ValidResponse.size(), 0);
        }
        else if (status == STATUS::FAILED)
        {
            ssck.Send(InvalidResponse.c_str(), InvalidResponse.size(), 0);
        }
        else
        {
            /* Nothing is recevied from socket. Continue to listen. */
            return;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void WEBServer::Listen()
{
    RS_LOG_DEBUG("IAMHTTP", "WEBServer::Listen");
    
    // Set timeout for non-blocking socket to 1 sec to pass it to Select.
    struct timeval tv = { 1, 0 };
    
    if (selector_.Select(&tv))
    {
        HandleConnection();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void WEBServer::ListenerThread()
{
    RS_LOG_DEBUG("IAMHTTP", "WEBServer::ListenerThread");

    if (!WEBServerInit())
    {
        RS_LOG_DEBUG("IAMHTTP", "WEBServer::WEBServerInit Failed");
        return;
    }

    auto start = std::chrono::system_clock::now();

    listening_.store(true);

    while ((std::chrono::system_clock::now() - start < std::chrono::seconds(timeout_)) && !parser_.IsFinished())
    {
        Listen();
    }

    code_ = parser_.RetrieveAuthCode(state_);
    saml_ = parser_.RetrieveSamlResponse();

    listen_socket_.Close();
}
