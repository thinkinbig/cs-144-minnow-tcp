#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  if ( buffer_.size() == capacity_ ) {
    return;
  }
  size_t len = min( data.size(), available_capacity() );
  buffer_.insert( buffer_.end(), data.begin(), data.begin() + len );
  bytes_pushed_ += len;
}

void Writer::close()
{
  closed_ = true;
}

bool Writer::is_closed() const
{
  return closed_;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - buffer_.size();
}

uint64_t Writer::bytes_pushed() const
{
  return bytes_pushed_;
}

string_view Reader::peek() const
{
  return string_view( buffer_.data(), buffer_.size() );
}

void Reader::pop( uint64_t len )
{
  if ( buffer_.empty() ) {
    return;
  }
  len = min( len, buffer_.size() );
  buffer_.erase( buffer_.begin(), buffer_.begin() + len );
  bytes_popped_ += len;
}

bool Reader::is_finished() const
{
  return buffer_.empty() && closed_;
}

uint64_t Reader::bytes_buffered() const
{
  return buffer_.size();
}

uint64_t Reader::bytes_popped() const
{
  return bytes_popped_;
}
