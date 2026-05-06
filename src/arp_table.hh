#pragma once

#include "ethernet_header.hh"
#include "timer.hh"

#include <cstdint>
#include <optional>
#include <unordered_map>

// IP → MAC cache with a per-entry TTL (NetworkTimer::ARP_ENTRY_TIMEOUT).
// Entries are evicted lazily on lookup() and eagerly on tick().
class ARPTable
{
public:
  void add_entry( uint32_t ip, const EthernetAddress& mac );
  void remove_entry( uint32_t ip ) { entries_.erase( ip ); }

  // Returns the cached MAC if present and not expired; evicts on expiry.
  std::optional<EthernetAddress> lookup( uint32_t ip );

  void tick( size_t ms_since_last_tick );

  size_t size() const { return entries_.size(); }
  bool empty() const { return entries_.empty(); }

  auto begin() const { return entries_.begin(); }
  auto end() const { return entries_.end(); }

private:
  struct Entry
  {
    EthernetAddress mac {};
    NetworkTimer ttl { NetworkTimer::ARP_ENTRY_TIMEOUT };
  };
  std::unordered_map<uint32_t, Entry> entries_ {};
};
