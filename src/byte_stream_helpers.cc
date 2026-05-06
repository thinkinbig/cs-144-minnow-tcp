#include "byte_stream.hh"

#include <stdexcept>

using namespace std;

void read( Reader& reader, uint64_t max_len, string& out )
{
  out.clear();

  while ( reader.bytes_buffered() and out.size() < max_len ) {
    auto view = reader.peek();
    if ( view.empty() ) {
      throw runtime_error( "Reader::peek() returned empty string_view" );
    }
    view = view.substr( 0, max_len - out.size() );
    out += view;
    reader.pop( view.size() );
  }
}

// Reader and Writer are zero-byte views over ByteStream — the static_asserts
// catch anyone accidentally adding state to a derived class instead of the base.

Reader& ByteStream::reader()
{
  static_assert( sizeof( Reader ) == sizeof( ByteStream ),
                 "Please add member variables to the ByteStream base, not the ByteStream Reader." );
  return static_cast<Reader&>( *this ); // NOLINT(*-downcast)
}

const Reader& ByteStream::reader() const
{
  static_assert( sizeof( Reader ) == sizeof( ByteStream ),
                 "Please add member variables to the ByteStream base, not the ByteStream Reader." );
  return static_cast<const Reader&>( *this ); // NOLINT(*-downcast)
}

Writer& ByteStream::writer()
{
  static_assert( sizeof( Writer ) == sizeof( ByteStream ),
                 "Please add member variables to the ByteStream base, not the ByteStream Writer." );
  return static_cast<Writer&>( *this ); // NOLINT(*-downcast)
}

const Writer& ByteStream::writer() const
{
  static_assert( sizeof( Writer ) == sizeof( ByteStream ),
                 "Please add member variables to the ByteStream base, not the ByteStream Writer." );
  return static_cast<const Writer&>( *this ); // NOLINT(*-downcast)
}
