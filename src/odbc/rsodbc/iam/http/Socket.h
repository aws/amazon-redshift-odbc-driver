#pragma once

#include "rslog.h"
#include "SocketSupport.h"
#include "Selector.h"
#include "rs_string.h"

/*
* This class is used to wrap the network socket
* in order to have cross-platfrom code to send and receive
* data from incoming connections.
*/
class Socket
{
    public:
        Socket();
        
        Socket( SOCKET sfd);
        
        Socket(const Socket& s) = delete;
        
        Socket& operator=(const Socket& s) = delete;
        
        Socket(Socket&& s);
        
        Socket& operator=(Socket&& s);
        
        ~Socket();
        
        /*
        * Close a socket file descriptor, return true on success
        * or false in case of error.
        */
        bool Close();
        
        /*
        * Get port where socket is listening.
        */
        int GetListenPort() const;

        /*
        * Listen for connections on a socket and return zero on success,
        * or -1 in case of error.
        */
        int Listen(int backlog) const;
        
        /*
        * Accept a connection on a socket and return StreamSocket object.
        */
        Socket Accept() const;
        
        /*
        * Forcibly bind to a port in use by another socket and return true on success,
        * or false in case of error.
        */
        bool SetReusable() const;
        
        /*
        * Set socket to non-blocking mode and return true on success
        * or false in case of error.
        */
        bool SetNonBlocking() const;
        
        /*
        * Return true if error is caused by non-blocking mode of socket
        * otherwise return false.
        */
        bool IsNonBlockingError() const;
        
        /*
        * Prepare socket to handle incoming connections.
        */
        void PrepareListenSocket(const rs_string& port);
        
        /*
        * Register socket in master file descriptor set using Selector class.
        */
        void Register(Selector& selector) const;
        
        /*
        * Clear socket in master file descriptor set using Selector class.
        */
        void Unregister(Selector& selector) const;
        
        /*
        * Receive a message from a socket and return the number of bytes received,
        * or -1 if an error occurred.
        */
        int Receive(char *buffer, int length, int flags) const;
        
        /*
        * Send a message on a socket and return the number of bytes sent,
        * or -1 if an error occured.
        */
        int Send(const char *buffer, int length, int flags) const;
        
        /*
        * Bind a name to a socket and return zero on success,
        * or -1 in case of error.
        */
        int Bind(const struct sockaddr *address, size_t address_len) const;
        
    private:

        const int CONNECTION_BACKLOG = 10;

        SOCKET socket_fd_;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
inline Socket::Socket() : socket_fd_(INVALID_SOCKET)
{
    ; // Do nothing.
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline Socket::Socket(SOCKET sfd) : socket_fd_(sfd)
{
    ; // Do nothing.
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline Socket::Socket(Socket&& s)
{
    socket_fd_ = std::move(s.socket_fd_);

    s.socket_fd_ = INVALID_SOCKET;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline Socket& Socket::operator=(Socket&& s)
{
    socket_fd_ = std::move(s.socket_fd_);

    s.socket_fd_ = INVALID_SOCKET;

    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline Socket::~Socket()
{
  RS_LOG_DEBUG("IAM", "Socket::~Socket");

    Close();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool Socket::SetNonBlocking() const
{
  RS_LOG_DEBUG("IAM", "Socket::SetNonBlocking");

#if (defined(_WIN32) || defined(_WIN64))
    unsigned long mode = 1;
    return ioctlsocket(socket_fd_, FIONBIO, &mode) == 0 ? true : false;
#else
    int flags = fcntl(socket_fd_, F_GETFL, 0);
    return fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK) == 0 ? true : false;
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool Socket::IsNonBlockingError() const
{
  RS_LOG_DEBUG("IAM", "Socket::IsNonBlockingError");

#if (defined(_WIN32) || defined(_WIN64))
    return WSAGetLastError() == WSAEWOULDBLOCK;
#else
    return errno == EWOULDBLOCK;
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline void Socket::Register(Selector& selector) const
{
  RS_LOG_DEBUG("IAM", "Socket::Register");

    selector.Register(socket_fd_);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline void Socket::Unregister(Selector& selector) const
{
  RS_LOG_DEBUG("IAM", "Socket::Unregister");
    
    selector.Unregister(socket_fd_);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline int Socket::Bind(const struct sockaddr *address, size_t address_len) const
{
    RS_LOG_DEBUG("IAM", "Socket::Bind");

    return bind(socket_fd_, address, (int)address_len);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline int Socket::GetListenPort() const
{
  RS_LOG_DEBUG("IAM", "Socket::GetListenPort");

    sockaddr_storage addr;
    socklen_t len = sizeof(addr);
    int port = 0;

    getsockname(socket_fd_, (struct sockaddr*)&addr, &len);
    port = htons(((sockaddr_in*)&addr)->sin_port);

    RS_LOG_DEBUG("IAM", 
        "Socket::GetListenPort %d", port);

    return port;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline int Socket::Listen(int backlog) const
{
  RS_LOG_DEBUG("IAM", "Socket::Listen");

    return listen(socket_fd_, backlog);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline Socket Socket::Accept() const
{
  RS_LOG_DEBUG("IAM", "Socket::Accept");

    sockaddr_storage remoteaddr;
    socklen_t addrlen = sizeof(remoteaddr);

    return Socket(accept(socket_fd_, (struct sockaddr*)&remoteaddr, &addrlen));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool Socket::SetReusable() const
{
  RS_LOG_DEBUG("IAM", "Socket::SetReusable");
    
    int yes = 1;

    // Windows: int setsockopt(SOCKET s, int level, int optname, const char *optval, int optlen);
    // The optval parameter has ponter to const char, but according to the MSDN we should use int:
    //     To enable a Boolean option, the optval parameter points to a nonzero integer.
    //     To disable the option optval points to an integer equal to zero.
    //     The optlen parameter should be equal to sizeof(int) for Boolean options.
    // On Linux the optval parameter is pointer to void.
    return setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR,
        (char *)&yes, sizeof(yes)) == 0 ? true : false;
}
