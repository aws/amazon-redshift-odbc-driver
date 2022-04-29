#include "Selector.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
void Selector::Register(SOCKET sfd)
{
    RS_LOG(logger_)("Selector::Register");

    if (sfd == -1)
    {
        return;
    }

    FD_SET(sfd, &master_fds_);

#if defined WIN32
    max_fd_ = max(max_fd_, sfd);
#else
    max_fd_ = std::max(max_fd_, sfd);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Selector::Unregister(SOCKET sfd)
{
    RS_LOG(logger_)("Selector::Unregister");

    FD_CLR(sfd, &master_fds_);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Selector::Select(struct timeval *tv)
{
    RS_LOG(logger_)("Selector::Select");
    
    // As select will modify the file descriptior set we should keep temporary set to reflect the ready fd.
    fd_set read_fds_ = master_fds_;
    
    if (select(max_fd_ + 1, &read_fds_, nullptr, nullptr, tv) > 0)
    {
        for (SOCKET sfd = 0; sfd <= max_fd_; sfd++)
        {
            if (FD_ISSET(sfd, &read_fds_))
            {
                return true;
            }
        }
    }
    
    return false;
}
