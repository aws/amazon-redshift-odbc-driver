
#include "AddrInformation.h"

#include <stdexcept>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
AddrInformation::AddrInformation( const rs_string& port)
{
    RS_LOG_DEBUG("IAMHTTP", "AddrInformation::AddrInformation");

    addrinfo hints;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo("127.0.0.1", port.c_str(), &hints, &addrinfo_) != 0)
    {
        throw std::runtime_error("Unable to get address information.");
    }
}
