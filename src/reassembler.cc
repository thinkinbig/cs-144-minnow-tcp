#include "reassembler.hh"
#include "debug.hh"

#include <cassert>

using namespace std;

void Reassembler::Segment::merge_with( uint64_t other_index, const std::string& other_data )
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
    string new_data(first_index - other_index + data.size(), '\0');
    copy(other_data.begin(), other_data.begin() + (first_index - other_index), new_data.begin());
    copy(data.begin(), data.end(), new_data.begin() + (first_index - other_index));
    data = std::move(new_data);
    first_index = other_index;
  }
  if ( other_index + other_data.size() > first_index + data.size() ) {
    // New data extends to the right
    size_t old_size = data.size();
    data.resize(other_index + other_data.size() - first_index);
    copy(other_data.begin() + (first_index + old_size - other_index), 
         other_data.end(), 
         data.begin() + old_size);
  }
}

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  Writer& writer = output_.writer();

  if ( is_last_substring ) {
    last_byte_index_ = first_index + data.size();
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
  if ( first_index + data.size() > first_unaccepted_index ) {
    data = data.substr( 0, first_unaccepted_index - first_index );
  }

  if ( !data.empty() ) {
    // Handle overlapping segments
    auto it = pending_data_.begin();
    while ( it != pending_data_.end() ) {
      // Check for overlap
      if ( it->first_index < first_index + data.size() && first_index < it->first_index + it->data.size() ) {
        // Create a new segment with merged data
        Segment merged = *it;
        merged.merge_with( first_index, data );

        // Update our working data with merged result
        data = merged.data;
        first_index = merged.first_index;

        it = pending_data_.erase( it );
      } else {
        ++it;
      }
    }
    pending_data_.insert( Segment { first_index, data } );
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
  if ( last_byte_index_ > 0 && first_unassembled_index_ == last_byte_index_ && pending_data_.empty() ) {
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
