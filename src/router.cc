#include "router.hh"
#include "debug.hh"

#include <iostream>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";
  
  if (prefix_length > 32) {
    return;
  }

  RouteEntry route_entry = { route_prefix, next_hop, interface_num };
  routes_[prefix_length].push_back(route_entry);
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  // Iterate through all interfaces
  for (size_t i = 0; i < interfaces_.size(); i++) {
    auto& interface = interfaces_[i];
    // Get all datagrams received on this interface
    auto& datagrams = interface->datagrams_received();
    // Process all datagrams on this interface
    while (!datagrams.empty()) {
      InternetDatagram datagram = move(datagrams.front());
      datagrams.pop();
      route_datagram(datagram, i);
    }
  }
}

std::optional<Router::RouteEntry> Router::find_route(const uint32_t& destination) {
  
  for (int prefix_length = 32; prefix_length >= 0; prefix_length--) {
    auto it = routes_.find(prefix_length);
    if (it != routes_.end()) {
      for (const auto& route : it->second) {
        uint32_t mask = (prefix_length == 32) ? 0xFFFFFFFF : ~(0xFFFFFFFF >> prefix_length);
        uint32_t dest_masked = destination & mask;
        uint32_t route_masked = route.route_prefix & mask;
        
        if (dest_masked == route_masked) {
          return route;
        }
      }
    }
  }
  return nullopt;
}

void Router::route_datagram(InternetDatagram& datagram, size_t interface_num) {
  uint32_t destination = datagram.header.dst;
  auto route = find_route(destination);
  if (route) {
    // Check TTL
    if (datagram.header.ttl <= 1) {
      return;
    }
    datagram.header.ttl--;
    datagram.header.compute_checksum();

    // Calculate next hop address
    Address nexthop = route->next_hop.has_value() ? *route->next_hop : Address::from_ipv4_numeric(datagram.header.dst);

    // If directly connected network and on same interface, send through incoming interface
    // If different interface, forward according to routing table
    if ((!route->next_hop.has_value() && route->interface_num == interface_num) || route->interface_num != interface_num) {
      interfaces_[interface_num]->send_datagram(datagram, nexthop);
    }

  }
}