#pragma once

#include "address.hh"
#include "ethernet_frame.hh"
#include "ipv4_datagram.hh"
#include "timer.hh"

#include <memory>
#include <optional>
#include <unordered_map>

// ARP request timeout observer interface
class ARPRequestObserver
{
public:
  virtual ~ARPRequestObserver() = default;
  virtual void on_arp_request( const Address& next_hop ) = 0;
};

class ARPMessageQueue
{
public:
  // Datagram waiting for ARP reply
  struct PendingDatagram
  {
    InternetDatagram dgram {};
    Address next_hop { "0.0.0.0", 0 };
    NetworkTimer timer { NetworkTimer::ARP_REQUEST_TIMEOUT }; // 5 seconds timeout
  };

  // Pending queue for each IP address
  using PendingQueue = std::vector<PendingDatagram>;
  using ARPRequestObserverPtr = std::shared_ptr<ARPRequestObserver>;

  // Default constructor
  ARPMessageQueue() = default;

  // Constructor with observer
  explicit ARPMessageQueue( ARPRequestObserverPtr observer ) : observer_( observer ) {}

  ~ARPMessageQueue() = default;

  // Add a pending datagram
  void add_pending( const InternetDatagram& dgram, const Address& next_hop );

  // Get and remove all pending datagrams for the specified IP address
  std::vector<PendingDatagram> pop_pending( uint32_t ip_addr );

  // Check if there are pending ARP replies for a specific IP
  bool has_pending( uint32_t ip_addr ) const;

  // Update timers
  void tick( size_t ms_since_last_tick );

  // Get number of pending datagrams
  size_t size() const
  {
    size_t total = 0;
    for ( const auto& [_, queue] : pending_ ) {
      total += queue.size();
    }
    return total;
  }

  // Check if empty
  bool empty() const { return pending_.empty(); }

  // Set timeout observer
  void set_observer( ARPRequestObserverPtr observer ) { observer_ = observer; }


private:
  std::unordered_map<uint32_t, PendingQueue> pending_ {};
  ARPRequestObserverPtr observer_ {};
};