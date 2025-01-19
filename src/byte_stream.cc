#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  // Push data to the buffer, but only as much as available capacity allows.
  uint64_t to_write = min( available_capacity(), data.size() );
  buffer_.append( data, 0, to_write );
  bytes_pushed_ += to_write;
}

void Writer::close()
{
  is_closed_ = true;
}

bool Writer::is_closed() const
{
  return is_closed_;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - ( buffer_.size() - read_index_ );
}

uint64_t Writer::bytes_pushed() const
{
  return bytes_pushed_;
}

string_view Reader::peek() const
{
  // Return a string_view of the data in the buffer.
  return string_view( buffer_.data() + read_index_, buffer_.size() - read_index_ );
}

void Reader::pop( uint64_t len )
{
  // Pop as much data as possible from the buffer if len is greater than the amount of data available.
  uint64_t to_read = min( bytes_buffered(), len );
  read_index_ += to_read;
  bytes_popped_ += to_read;

  // When more than half of the buffer is read, shrink it to avoid unnecessary memory usage.
  if ( read_index_ >= buffer_.size() / 2 ) {
    buffer_ = buffer_.substr( read_index_ );
    read_index_ = 0;
  }
}

bool Reader::is_finished() const
{
  return is_closed_ && bytes_buffered() == 0;
}

uint64_t Reader::bytes_buffered() const
{
  return buffer_.size() - read_index_;
}

uint64_t Reader::bytes_popped() const
{
  return bytes_popped_;
}
