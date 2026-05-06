#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <string_view>

class Reader;
class Writer;

// A bounded byte stream with separate Writer / Reader views.
// All bookkeeping lives in the ByteStream base; Reader and Writer are
// zero-byte derived views (see the static_asserts in byte_stream_helpers.cc).
class ByteStream
{
public:
  explicit ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

  Reader& reader();
  const Reader& reader() const;
  Writer& writer();
  const Writer& writer() const;

  void set_error() { error_ = true; }
  bool has_error() const { return error_; }

protected:
  uint64_t capacity_;
  std::deque<std::string> segments_ {}; // chain of pushed buffers; front is the next byte to read
  uint64_t skip_in_front_ {};           // bytes already popped from segments_.front()
  uint64_t bytes_pushed_ {};
  uint64_t bytes_popped_ {};
  bool error_ {};
  bool closed_ {};
};

class Writer : public ByteStream
{
public:
  void push( std::string data ); // Push data, truncated to available_capacity().
  void close();                  // Signal end of stream; nothing more will be written.

  bool is_closed() const;
  uint64_t available_capacity() const;
  uint64_t bytes_pushed() const;
};

class Reader : public ByteStream
{
public:
  std::string_view peek() const; // Peek at the next contiguous run of buffered bytes.
  void pop( uint64_t len );      // Discard up to `len` buffered bytes from the front.

  bool is_finished() const;        // Closed and fully drained?
  uint64_t bytes_buffered() const; // Bytes pushed but not yet popped.
  uint64_t bytes_popped() const;
};

// Peek-and-pop up to `max_len` bytes from `reader` into `out`.
void read( Reader& reader, uint64_t max_len, std::string& out );
