#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + static_cast<uint32_t>( n );
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Of all absolute seqnos that map to *this, return the one closest to `checkpoint`.
  // Strategy: build the candidate that shares `checkpoint`'s upper 32 bits, then check
  // its ±2^32 neighbours and pick whichever is nearest.
  constexpr uint64_t kRound = 1ULL << 32;

  const uint32_t offset = raw_value_ - zero_point.raw_value_;
  const uint64_t candidate = ( checkpoint & ~static_cast<uint64_t>( UINT32_MAX ) ) | offset;

  auto distance = [checkpoint]( uint64_t v ) { return v > checkpoint ? v - checkpoint : checkpoint - v; };

  uint64_t best = candidate;
  uint64_t best_dist = distance( candidate );

  if ( candidate >= kRound ) {
    const uint64_t prev = candidate - kRound;
    if ( distance( prev ) < best_dist ) {
      best = prev;
      best_dist = distance( prev );
    }
  }
  const uint64_t next = candidate + kRound;
  if ( distance( next ) < best_dist ) {
    best = next;
  }
  return best;
}
