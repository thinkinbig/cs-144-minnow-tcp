#pragma once

#include "address.hh"
#include "ipv4_datagram.hh"
#include "timer.hh"

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

// Per-IP queue of IPv4 datagrams that are waiting on an outstanding ARP request.
// When the head of a queue ages past ARP_REQUEST_TIMEOUT, the entire queue is
// dropped and the registered callback is invoked so the caller can re-broadcast.
class ARPMessageQueue
{
public:
  struct PendingDatagram
  {
    InternetDatagram dgram {};
    Address next_hop { "0.0.0.0", 0 };
    NetworkTimer timer { NetworkTimer::ARP_REQUEST_TIMEOUT };
  };

  using TimeoutCallback = std::function<void( const Address& )>;

  ARPMessageQueue() = default;
  explicit ARPMessageQueue( TimeoutCallback cb ) : on_timeout_( std::move( cb ) ) {}

  void set_callback( TimeoutCallback cb ) { on_timeout_ = std::move( cb ); }

  void add_pending( const InternetDatagram& dgram, const Address& next_hop );
  std::vector<PendingDatagram> pop_pending( uint32_t ip );
  bool has_pending( uint32_t ip ) const;

  // Advance per-IP head timers; on expiry, drop the queue and notify the
  // callback so it can resend the ARP request.
  void tick( size_t ms_since_last_tick );

  size_t size() const;
  bool empty() const { return pending_.empty(); }

private:
  std::unordered_map<uint32_t, std::vector<PendingDatagram>> pending_ {};
  TimeoutCallback on_timeout_ {};
};
