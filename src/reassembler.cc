#include "reassembler.hh"
#include "debug.hh"

#include <cassert>

using namespace std;

void Reassembler::Segment::merge_with( uint64_t other_index, const string& other_data )
{
  // If new data is completely contained in existing data, no need to merge
  if ( first_index <= other_index && first_index + data.size() >= other_index + other_data.size() ) {
    return;
  }

  // If new data completely contains existing data, replace it
  if ( other_index <= first_index && other_index + other_data.size() >= first_index + data.size() ) {
    first_index = other_index;
    data = other_data;
    return;
  }

  // Handle partial overlap
  if ( other_index < first_index ) {
    // New data extends to the left
    const size_t prefix_size = first_index - other_index;
    string new_data;
    new_data.reserve( prefix_size + data.size() );
    new_data.append( other_data.data(), prefix_size );
    new_data.append( data );
    data = std::move( new_data );
    first_index = other_index;
  }
  if ( other_index + other_data.size() > first_index + data.size() ) {
    // New data extends to the right
    const size_t old_size = data.size();
    const size_t suffix_start = first_index + old_size - other_index;
    const size_t suffix_size = other_data.size() - suffix_start;
    data.append( other_data.data() + suffix_start, suffix_size );
  }
}

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  Writer& writer = output_.writer();

  assert( first_unassembled_index_ == writer.bytes_pushed() );

  if ( is_last_substring ) {
    last_byte_index_ = first_index + data.size();
    eof_flag_ = true;
  }

  // Handle empty last substring
  if ( data.empty() && is_last_substring && pending_data_.empty() && first_index == first_unassembled_index_ ) {
    writer.close();
    return;
  }

  // Handle data that's already been assembled
  if ( first_index < first_unassembled_index_ ) {
    uint64_t overlap = first_unassembled_index_ - first_index;
    if ( overlap >= data.size() ) {
      return;
    }
    data = data.substr( overlap );
    first_index = first_unassembled_index_;
  }

  // Handle data beyond capacity
  uint64_t first_unaccepted_index = first_unassembled_index_ + writer.available_capacity();

  // If the data is beyond capacity, return
  if ( first_index >= first_unaccepted_index ) {
    return;
  }

  // If the data is beyond capacity, truncate it
  if ( data.size() > first_unaccepted_index - first_index ) {
    data = data.substr( 0, first_unaccepted_index - first_index );
  }

  if ( !data.empty() ) {
    // Create new segment
    Segment new_segment { first_index, data };

    // Find the first segment that starts after our new segment
    auto next = pending_data_.lower_bound( new_segment );

    // Find the last segment that starts before our new segment
    auto prev = next;
    if ( next != pending_data_.begin() ) {
      prev = std::prev( next );
    }

    // Check and merge with previous segment if it overlaps
    if ( prev != pending_data_.end() && prev != next ) {
      if ( prev->first_index + prev->data.size() > first_index ) {
        // Merge with previous segment
        Segment merged = *prev;
        merged.merge_with( first_index, data );
        new_segment = merged;
        prev = pending_data_.erase( prev );
      }
    }

    // Check and merge with all following segments that overlap
    while ( next != pending_data_.end() && next->first_index < new_segment.first_index + new_segment.data.size() ) {
      // Merge with next segment
      new_segment.merge_with( next->first_index, next->data );
      next = pending_data_.erase( next );
    }

    // Insert the merged segment
    pending_data_.insert( new_segment );
  }

  // Try to push contiguous data
  while ( !pending_data_.empty() ) {
    auto it = pending_data_.begin();
    if ( it->first_index == first_unassembled_index_ ) {
      writer.push( it->data );
      first_unassembled_index_ += it->data.size();
      pending_data_.erase( it );
    } else {
      break;
    }
  }

  // Close stream if we've written everything
  if ( eof_flag_ && first_unassembled_index_ == last_byte_index_ && pending_data_.empty() ) {
    writer.close();
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t bytes_pending = 0;
  for ( auto& segment : pending_data_ ) {
    bytes_pending += segment.data.size();
  }
  return bytes_pending;
}
