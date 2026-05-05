#pragma once

#include "byte_stream.hh"

#include <map>
#include <string>

class Reassembler
{
public:
  // Construct Reassembler to write into given ByteStream.
  explicit Reassembler( ByteStream&& output ) : output_( std::move( output ) ) {}

  /*
   * Insert a new substring to be reassembled into a ByteStream.
   *   `first_index`: the index of the first byte of the substring
   *   `data`: the substring itself
   *   `is_last_substring`: this substring represents the end of the stream
   */
  void insert( uint64_t first_index, std::string data, bool is_last_substring );

  // How many bytes are stored in the Reassembler itself?
  // This function is for testing only; don't add extra state to support it.
  uint64_t count_bytes_pending() const;

  // Access output stream reader
  Reader& reader() { return output_.reader(); }
  const Reader& reader() const { return output_.reader(); }

  // Access output stream writer, but const-only (can't write from outside)
  const Writer& writer() const { return output_.writer(); }

  void set_error() { output_.set_error(); }
  bool has_error() const { return output_.has_error(); }
  void close() { output_.writer().close(); }

private:
  ByteStream output_;

  // Pending substrings keyed by their first_index. Invariant: no two entries
  // overlap or are adjacent — every insertion merges with neighbors.
  std::map<uint64_t, std::string> pending_ {};

  // Absolute index of the byte that follows the last byte of the stream
  // (only meaningful once eof_seen_ is true).
  uint64_t end_index_ {};
  bool eof_seen_ {};
};
