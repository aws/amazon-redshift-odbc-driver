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
    RS_LOG_DEBUG("IAMHTTP", "WEBServer::HandleConnection - Starting");

    /* Trying to accept the pending incoming connection. */
    Socket ssck(listen_socket_.Accept());

    ++connections_counter_;

    RS_LOG_DEBUG("IAMHTTP", "WEBServer::HandleConnection - Connection accepted, counter: %d", connections_counter_);

    if (ssck.SetNonBlocking())
    {
        RS_LOG_DEBUG("IAMHTTP", "WEBServer::HandleConnection - Socket set to non-blocking, creating stream");

        SocketStream socket_buffer(ssck);
        std::istream socket_stream(&socket_buffer);

        RS_LOG_DEBUG("IAMHTTP", "WEBServer::HandleConnection - Calling parser_.Parse()");
        STATUS status = parser_.Parse(socket_stream);

        RS_LOG_DEBUG("IAMHTTP", "WEBServer::HandleConnection - Parser returned status: %d", static_cast<int>(status));
        RS_LOG_DEBUG("IAMHTTP", "WEBServer::HandleConnection - Parser IsFinished(): %d", parser_.IsFinished());

        if (status == STATUS::SUCCEED)
        {
            RS_LOG_DEBUG("IAMHTTP", "WEBServer::HandleConnection - Sending ValidResponse");
            ssck.Send(ValidResponse.c_str(), ValidResponse.size(), 0);
            consecutive_empty_requests_ = 0;
        }
        else if (status == STATUS::FAILED)
        {
            RS_LOG_DEBUG("IAMHTTP", "WEBServer::HandleConnection - Sending InvalidResponse");
            ssck.Send(InvalidResponse.c_str(), InvalidResponse.size(), 0);
            consecutive_empty_requests_ = 0;
        }
        else
        {
            RS_LOG_DEBUG("IAMHTTP", "WEBServer::HandleConnection - Status is EMPTY_REQUEST, consecutive count: %d", consecutive_empty_requests_);
            /* Nothing is received from socket. This might indicate user closed browser. */
            consecutive_empty_requests_++;
            return;
        }
    }
    else
    {
        RS_LOG_DEBUG("IAMHTTP", "WEBServer::HandleConnection - Failed to set socket to non-blocking");
    }

    RS_LOG_DEBUG("IAMHTTP", "WEBServer::HandleConnection - Finished");
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
    RS_LOG_DEBUG("IAMHTTP", "WEBServer::ListenerThread - Starting, timeout: %d seconds", timeout_);

    if (!WEBServerInit())
    {
        RS_LOG_DEBUG("IAMHTTP", "WEBServer::WEBServerInit Failed");
        return;
    }

    RS_LOG_DEBUG("IAMHTTP", "WEBServer::ListenerThread - Server initialized successfully, listening on port: %d", listen_port_);

    auto start = std::chrono::system_clock::now();

    listening_.store(true);

    RS_LOG_DEBUG("IAMHTTP", "WEBServer::ListenerThread - Entering listening loop");

    int loop_count = 0;
    const int MAX_CONSECUTIVE_EMPTY_REQUESTS = 3;  // Exit early if browser disconnected

    while ((std::chrono::system_clock::now() - start < std::chrono::seconds(timeout_)) && !parser_.IsFinished())
    {
        loop_count++;
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - start).count();

        if (loop_count % 10 == 1)  // Log every 10 iterations to avoid spam
        {
            RS_LOG_DEBUG("IAMHTTP", "WEBServer::ListenerThread - Loop #%d, elapsed: %lld sec, parser finished: %d, consecutive empty: %d",
                         loop_count, elapsed, parser_.IsFinished(), consecutive_empty_requests_);
        }

        Listen();

        if (parser_.IsFinished())
        {
            RS_LOG_DEBUG("IAMHTTP", "WEBServer::ListenerThread - Parser finished after %d loops, %lld seconds",
                         loop_count, elapsed);
            break;
        }

        // Check if user closed browser - exit early instead of waiting for timeout
        if (connections_counter_ > 0 && consecutive_empty_requests_ >= MAX_CONSECUTIVE_EMPTY_REQUESTS)
        {
            RS_LOG_DEBUG("IAMHTTP", "WEBServer::ListenerThread - Detected browser disconnection after %d consecutive empty requests, exiting early",
                         consecutive_empty_requests_);
            break;
        }
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - start).count();
    bool timed_out = elapsed >= timeout_;

    RS_LOG_DEBUG("IAMHTTP", "WEBServer::ListenerThread - Exited loop: loops=%d, elapsed=%lld sec, timeout=%d sec, timed_out=%d, parser_finished=%d, connections=%d",
                 loop_count, elapsed, timeout_, timed_out, parser_.IsFinished(), connections_counter_);

    code_ = parser_.RetrieveAuthCode(state_);
    saml_ = parser_.RetrieveSamlResponse();

    RS_LOG_DEBUG("IAMHTTP", "WEBServer::ListenerThread - Retrieved code length: %zu, saml length: %zu",
                 code_.size(), saml_.size());

    listen_socket_.Close();

    RS_LOG_DEBUG("IAMHTTP", "WEBServer::ListenerThread - Socket closed, thread ending");
}
