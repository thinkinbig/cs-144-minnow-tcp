#include "arp_table.hh"

using namespace std;

void ARPTable::add_entry( uint32_t ip, const EthernetAddress& mac )
{
  auto& entry = entries_[ip];
  entry.mac = mac;
  entry.ttl.start();
}

optional<EthernetAddress> ARPTable::lookup( uint32_t ip )
{
  const auto it = entries_.find( ip );
  if ( it == entries_.end() ) {
    return nullopt;
  }
  if ( it->second.ttl.is_expired() ) {
    entries_.erase( it );
    return nullopt;
  }
  return it->second.mac;
}

void ARPTable::tick( size_t ms_since_last_tick )
{
  for ( auto it = entries_.begin(); it != entries_.end(); ) {
    it->second.ttl.tick( ms_since_last_tick );
    if ( it->second.ttl.is_expired() ) {
      it = entries_.erase( it );
    } else {
      ++it;
    }
  }
}
