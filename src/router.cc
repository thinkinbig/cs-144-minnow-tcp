#include "router.hh"

#include <algorithm>
#include <functional>

using namespace std;

void Router::add_route( uint32_t route_prefix,
                        uint8_t prefix_length,
                        optional<Address> next_hop,
                        size_t interface_num )
{
  if ( prefix_length > 32 ) {
    return;
  }
  // Insert preserving descending order on prefix_length so find_route() can
  // stop at the first match.
  RouteEntry entry { route_prefix, prefix_length, next_hop, interface_num };
  const auto pos = std::ranges::upper_bound(
    routes_, prefix_length, std::ranges::greater {}, &RouteEntry::prefix_length );
  routes_.insert( pos, entry );
}

const Router::RouteEntry* Router::find_route( uint32_t destination ) const
{
  for ( const auto& r : routes_ ) {
    // A prefix length of 0 matches anything; otherwise, mask off the low bits.
    const uint32_t mask = r.prefix_length == 0 ? 0 : ( ~uint32_t { 0 } << ( 32 - r.prefix_length ) );
    if ( ( destination & mask ) == ( r.prefix & mask ) ) {
      return &r;
    }
  }
  return nullptr;
}

void Router::route()
{
  for ( size_t i = 0; i < interfaces_.size(); ++i ) {
    auto& queue = interfaces_[i]->datagrams_received();
    while ( not queue.empty() ) {
      forward( move( queue.front() ), i );
      queue.pop();
    }
  }
}

void Router::forward( InternetDatagram datagram, size_t arrived_on )
{
  if ( datagram.header.ttl <= 1 ) {
    return;
  }

  const RouteEntry* route = find_route( datagram.header.dst );
  if ( route == nullptr ) {
    return;
  }

  // Don't bounce a datagram back through the interface it arrived on if the
  // route would also need an ARP step (i.e., it's not a directly-attached host).
  if ( route->interface_num == arrived_on and route->next_hop.has_value() ) {
    return;
  }

  --datagram.header.ttl;
  datagram.header.compute_checksum();

  const Address next_hop
    = route->next_hop.value_or( Address::from_ipv4_numeric( datagram.header.dst ) );
  interfaces_[route->interface_num]->send_datagram( datagram, next_hop );
}
