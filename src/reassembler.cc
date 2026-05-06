#include "reassembler.hh"

#include <utility>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  Writer& writer = output_.writer();

  if ( is_last_substring ) {
    end_index_ = first_index + data.size();
    eof_seen_ = true;
  }

  // 1) Clip [first_index, first_index+|data|) to the acceptable window
  //    [first_unassembled, first_unacceptable).
  const uint64_t first_unassembled = writer.bytes_pushed();
  const uint64_t first_unacceptable = first_unassembled + writer.available_capacity();

  if ( first_index >= first_unacceptable or first_index + data.size() <= first_unassembled ) {
    data.clear();
  } else {
    if ( first_index < first_unassembled ) {
      data.erase( 0, first_unassembled - first_index );
      first_index = first_unassembled;
    }
    if ( first_index + data.size() > first_unacceptable ) {
      data.resize( first_unacceptable - first_index );
    }
  }

  if ( not data.empty() ) {
    // 2) Merge with the immediate predecessor if it touches or overlaps us.
    auto next = pending_.upper_bound( first_index );
    if ( next != pending_.begin() ) {
      auto prev = std::prev( next );
      const uint64_t prev_end = prev->first + prev->second.size();
      if ( prev_end >= first_index ) {
        if ( prev_end >= first_index + data.size() ) {
          data.clear(); // fully covered
        } else {
          // Extend prev with our suffix and adopt it as our working segment so
          // we can keep absorbing successors below.
          const uint64_t overlap = prev_end - first_index;
          prev->second.append( data, overlap );
          first_index = prev->first;
          data = std::move( prev->second );
          pending_.erase( prev );
        }
      }
    }
  }

  if ( not data.empty() ) {
    // 3) Absorb every successor that touches or overlaps us.
    auto it = pending_.lower_bound( first_index );
    while ( it != pending_.end() and it->first <= first_index + data.size() ) {
      const uint64_t next_end = it->first + it->second.size();
      if ( next_end > first_index + data.size() ) {
        const uint64_t overlap = first_index + data.size() - it->first;
        data.append( it->second, overlap );
      }
      it = pending_.erase( it );
    }
    pending_.emplace( first_index, std::move( data ) );
  }

  // 4) Flush the contiguous prefix into the output stream.
  while ( not pending_.empty() and pending_.begin()->first == writer.bytes_pushed() ) {
    auto node = pending_.extract( pending_.begin() );
    writer.push( std::move( node.mapped() ) );
  }

  if ( eof_seen_ and writer.bytes_pushed() == end_index_ ) {
    writer.close();
  }
}

uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t total = 0;
  for ( const auto& [_, data] : pending_ ) {
    total += data.size();
  }
  return total;
}
