#include "arp_message_queue.hh"

using namespace std;

void ARPMessageQueue::add_pending( const InternetDatagram& dgram, const Address& next_hop )
{
  auto& queue = pending_[next_hop.ipv4_numeric()];
  auto& pending = queue.emplace_back();
  pending.dgram = dgram;
  pending.next_hop = next_hop;
  pending.timer.start();
}

vector<ARPMessageQueue::PendingDatagram> ARPMessageQueue::pop_pending( uint32_t ip )
{
  const auto it = pending_.find( ip );
  if ( it == pending_.end() ) {
    return {};
  }
  vector<PendingDatagram> out = move( it->second );
  pending_.erase( it );
  return out;
}

bool ARPMessageQueue::has_pending( uint32_t ip ) const
{
  const auto it = pending_.find( ip );
  return it != pending_.end() and not it->second.empty();
}

void ARPMessageQueue::tick( size_t ms_since_last_tick )
{
  for ( auto it = pending_.begin(); it != pending_.end(); ) {
    if ( it->second.empty() ) {
      it = pending_.erase( it );
      continue;
    }
    // Only the head datagram's timer matters: enqueueing later datagrams
    // doesn't reset the outstanding ARP request, so the queue ages with its head.
    auto& head = it->second.front();
    head.timer.tick( ms_since_last_tick );
    if ( head.timer.is_expired() ) {
      const Address next_hop = head.next_hop;
      it = pending_.erase( it );
      if ( on_timeout_ ) {
        on_timeout_( next_hop );
      }
    } else {
      ++it;
    }
  }
}

size_t ARPMessageQueue::size() const
{
  size_t total = 0;
  for ( const auto& [_, queue] : pending_ ) {
    total += queue.size();
  }
  return total;
}
