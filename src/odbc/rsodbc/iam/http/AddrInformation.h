#pragma once

#include "RsLogger.h"
#include "rs_string.h"
#include "SocketSupport.h"

/*
* This class is used to support range-based for loop and iterators
* in AddrInformation class.
*/
class AddrInformationIterator {
public:
    AddrInformationIterator(addrinfo *addr);

    AddrInformationIterator(const AddrInformationIterator& itr);

    ~AddrInformationIterator();

    AddrInformationIterator& operator++();

    AddrInformationIterator operator++(int);

    bool operator!=(const AddrInformationIterator& itr) const;

    bool operator==(const AddrInformationIterator& itr) const;

    addrinfo* operator->();

    const addrinfo* operator*() const;

    addrinfo* operator*();

private:
    addrinfo *addr_;

};

/*
* This class is used to perform network address and service
* translation via getaddrinfo call. Returns one or more addrinfo structures, each
* of which contains an Internet address.
*/
class AddrInformation
{
public:
    AddrInformation(RsLogger *in_log, const rs_string& port);

    ~AddrInformation();

    AddrInformationIterator begin();

    AddrInformationIterator end();

    AddrInformationIterator begin() const;

    AddrInformationIterator end() const;

private:
    RsLogger *logger_;

    addrinfo *addrinfo_;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
inline AddrInformation::~AddrInformation()
{
   logger_->log( "AddrInformation::~AddrInformation");

   freeaddrinfo(addrinfo_);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline AddrInformationIterator AddrInformation::begin()
{
    return AddrInformationIterator(addrinfo_);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline AddrInformationIterator AddrInformation::end()
{
    return AddrInformationIterator(nullptr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline AddrInformationIterator AddrInformation::begin() const
{
    return AddrInformationIterator(addrinfo_);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline AddrInformationIterator AddrInformation::end() const
{
    return AddrInformationIterator(nullptr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline AddrInformationIterator::AddrInformationIterator(addrinfo* addr) : addr_(addr)
{
    ; // Do nothing.
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline AddrInformationIterator::AddrInformationIterator(const AddrInformationIterator& itr)
    : addr_(itr.addr_)
{
    ; // Do nothing.
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline AddrInformationIterator::~AddrInformationIterator()
{
    ; // Do nothing.
}


////////////////////////////////////////////////////////////////////////////////////////////////////
inline AddrInformationIterator AddrInformationIterator::operator++(int)
{
    AddrInformationIterator tmp(*this);

    if (addr_)
    {
        addr_ = addr_->ai_next;
    }

    return tmp;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline AddrInformationIterator& AddrInformationIterator::operator++()
{
    if (addr_)
    {
        addr_ = addr_->ai_next;
    }

    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool AddrInformationIterator::operator!=(const AddrInformationIterator& itr) const
{
    return addr_ != itr.addr_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool AddrInformationIterator::operator==(const AddrInformationIterator& itr) const
{
    return addr_ == itr.addr_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline addrinfo* AddrInformationIterator::operator->()
{
    return addr_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline const addrinfo* AddrInformationIterator::operator*() const
{
    return addr_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline addrinfo* AddrInformationIterator::operator*()
{
    return addr_;
}
