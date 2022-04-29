#pragma once

#include "RsLogger.h"
#include "SocketSupport.h"

#include <algorithm>

/*
* This class is used to support insertion/deletion operations on
* master descriptor set and selecting incoming connections.
*/
class Selector
{
    public:
        Selector(RsLogger* in_log);
        
        ~Selector() = default;
        
        /*
        * Update master descriptor set with new socket.
        */
        void Register(SOCKET sfd);
        
        /*
        * Remove socket from master descriptor set.
        */
        void Unregister(SOCKET sfd);
        
        /*
        * If any incoming connections is readable return true, or
        * false otherwise.
        */
        bool Select(struct timeval* tv);
        
    private:
        RsLogger* logger_;

        SOCKET max_fd_;
        fd_set master_fds_;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
inline Selector::Selector(RsLogger *in_log) : logger_(in_log), max_fd_(0)
{
    FD_ZERO(&master_fds_);
}
