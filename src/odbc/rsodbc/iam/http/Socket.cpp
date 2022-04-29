#include "Socket.h"
#include "AddrInformation.h"

#include <stdexcept>
#include <chrono>
#include <thread>

////////////////////////////////////////////////////////////////////////////////////////////////////
int Socket::Receive(char *buffer, int length, int flags) const
{
    RS_LOG(logger_)("Socket::Receive");
    
    int nbytes = 0, filled = 0;
    auto start = std::chrono::system_clock::now();

    // Give a chance to fully receive packet in case if there is no data in
    // non-blocking socket.
    while ((std::chrono::system_clock::now() - start < std::chrono::seconds(1)))
    {
        nbytes = recv(socket_fd_, buffer + filled, length - filled - 1, 0);

        if (nbytes <= 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        else
        {
            filled += nbytes;
        }
    }

    return filled == 0 ? nbytes : filled;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int Socket::Send(const char *buffer, int length, int flags) const
 {
    RS_LOG(logger_)("Socket::Send");

    int nbytes = 0, sent = 0;

    while ((nbytes = send(socket_fd_, buffer + sent, length - sent, flags)) > 0)
    {
        sent += nbytes;
    }

    return sent == 0 ? nbytes : sent;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Socket::PrepareListenSocket(const rs_string& port)
{
    RS_LOG(logger_)("Socket::PrepareListenSocket");

    AddrInformation addrInfo(logger_, port);

    for (const auto& ptr : addrInfo)
    {
        socket_fd_ = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

        if (socket_fd_ == INVALID_SOCKET)
            continue;

        if (!SetReusable())
        {
            Close();

            throw std::runtime_error("Unable to reuse the address.");
        }

        if (Bind(ptr->ai_addr, ptr->ai_addrlen) == 0)
        {
            break;
        }

        Close();
    }

    if (!SetNonBlocking())
    {
        Close();

        throw std::runtime_error("Unable to set socket to non-blocking mode.");
    }

    if ((socket_fd_ == INVALID_SOCKET) || (Listen(CONNECTION_BACKLOG)))
    {
        Close();

        throw std::runtime_error("Can not start listening on port: " + port);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Socket::Close()
{
    RS_LOG(logger_)("Socket::Close");

    if (socket_fd_ == -1)
        return false;

#if (defined(_WIN32) || defined(_WIN64))
    int res = closesocket(socket_fd_);
#else
    int res = close(socket_fd_);
#endif

    socket_fd_ = INVALID_SOCKET;

    return res == 0 ? true : false;
}
