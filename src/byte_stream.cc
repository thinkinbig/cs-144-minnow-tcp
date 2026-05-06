#include "byte_stream.hh"

using namespace std;

void Writer::push( string data )
{
  const uint64_t avail = available_capacity();
  if ( data.empty() or avail == 0 ) {
    return;
  }
  if ( data.size() > avail ) {
    data.resize( avail );
  }
  bytes_pushed_ += data.size();
  segments_.emplace_back( move( data ) );
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
  return capacity_ - ( bytes_pushed_ - bytes_popped_ );
}

uint64_t Writer::bytes_pushed() const
{
  return bytes_pushed_;
}

string_view Reader::peek() const
{
  if ( segments_.empty() ) {
    return {};
  }
  return string_view { segments_.front() }.substr( skip_in_front_ );
}

void Reader::pop( uint64_t len )
{
  len = min( len, bytes_pushed_ - bytes_popped_ );
  bytes_popped_ += len;

  while ( len > 0 ) {
    const uint64_t front_remaining = segments_.front().size() - skip_in_front_;
    if ( len >= front_remaining ) {
      segments_.pop_front();
      skip_in_front_ = 0;
      len -= front_remaining;
    } else {
      skip_in_front_ += len;
      len = 0;
    }
  }
}

bool Reader::is_finished() const
{
  return closed_ and bytes_pushed_ == bytes_popped_;
}

uint64_t Reader::bytes_buffered() const
{
  return bytes_pushed_ - bytes_popped_;
}

uint64_t Reader::bytes_popped() const
{
  return bytes_popped_;
}
