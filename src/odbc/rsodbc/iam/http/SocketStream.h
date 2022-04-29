#pragma once

#include "Socket.h"

#include <sstream>

/*
* This class is used to wrap socket by std::stream.
*/
class SocketStream : public std::streambuf
{
    public:
        /*
        * Construct object and set the value for the pointers that define
        * the boundaries of the buffered portion of the controlled INPUT sequence.
        */
        SocketStream(Socket& s);
        
        /*
        * Call sync inside the destructor to synchronize the contents in the stream buffer
        * with those of the associated character sequence.
        */
        virtual ~SocketStream();
        
    protected:
        /*
        * Virtual function called by other member functions to get the current character
        * in the controlled input sequence without changing the current position.
        * It is called by public member functions such as sgetc to request
        * a new character when there are no read positions available at the get pointer (gptr).
        */
        virtual int underflow();

        static const int SIZE = 2048;
        static const int MAX_SIZE = 16384;

        int received_size_;
        char input_buffer_[SIZE];
        Socket& socket_;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
inline SocketStream::SocketStream(Socket& s) : received_size_(0), socket_(s)
{
    setg(input_buffer_, input_buffer_, input_buffer_);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline SocketStream::~SocketStream()
{
    sync();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline int SocketStream::underflow()
{
    if (gptr() < egptr())
    {
        return traits_type::to_int_type(*gptr());
    }
    
    int received_bytes = socket_.Receive(input_buffer_, SIZE - 1, 0);
    
    /*
    * Return EOF in the following cases:
    * If the received bytes less than zero (error situation);
    * If the received bytes equal to zero (socket peer has performed an orderly shutdown);
    * If the length of received packets more than MAX_SIZE.
    */
    if ((received_bytes <= 0) || (received_size_ + received_bytes > MAX_SIZE))
    {
        return traits_type::eof();
    }
    
    received_size_ += received_bytes;
    
    setg(input_buffer_, input_buffer_, input_buffer_ + received_bytes);
    
    return traits_type::to_int_type(*gptr());
}
