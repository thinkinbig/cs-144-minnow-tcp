#pragma once

#include "exception.hh"
#include "network_interface.hh"

#include <memory>
#include <optional>
#include <vector>

// A router holding several NetworkInterfaces and a longest-prefix-match
// forwarding table. route() drains every interface's input queue and forwards
// each datagram to the appropriate outgoing interface.
class Router
{
public:
  // Add an interface; returns its index for use with add_route().
  size_t add_interface( std::shared_ptr<NetworkInterface> interface )
  {
    interfaces_.push_back( notnull( "add_interface", std::move( interface ) ) );
    return interfaces_.size() - 1;
  }

  std::shared_ptr<NetworkInterface> interface( size_t n ) { return interfaces_.at( n ); }

  // Install a forwarding rule. `next_hop` is empty for directly-attached
  // networks, in which case the datagram's destination is used as next-hop.
  void add_route( uint32_t route_prefix,
                  uint8_t prefix_length,
                  std::optional<Address> next_hop,
                  size_t interface_num );

  // Forward all currently queued datagrams between interfaces.
  void route();

private:
  struct RouteEntry
  {
    uint32_t prefix;
    uint8_t prefix_length;
    std::optional<Address> next_hop;
    size_t interface_num;
  };

  std::vector<std::shared_ptr<NetworkInterface>> interfaces_ {};

  // Routes are kept sorted by descending prefix_length, so a linear scan
  // implements longest-prefix-match.
  std::vector<RouteEntry> routes_ {};

  const RouteEntry* find_route( uint32_t destination ) const;
  void forward( InternetDatagram datagram, size_t arrived_on );
};
