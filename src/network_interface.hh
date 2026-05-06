#pragma once

#include "address.hh"
#include "arp_message.hh"
#include "arp_message_queue.hh"
#include "arp_table.hh"
#include "ethernet_frame.hh"
#include "ipv4_datagram.hh"
#include "ref.hh"

#include <memory>
#include <queue>
#include <string>
#include <vector>

// Bridges IP (network layer) and Ethernet (link layer): wraps outbound IPv4
// datagrams in Ethernet frames (resolving next-hop MACs via ARP), and unwraps
// inbound frames into IPv4 datagrams or ARP messages.
//
// The same module is reused inside Router, where one host owns many
// NetworkInterfaces and forwards datagrams between them.
class NetworkInterface : public std::enable_shared_from_this<NetworkInterface>
{
public:
  // Abstraction for the physical port that carries Ethernet frames out.
  class OutputPort
  {
  public:
    virtual void transmit( const NetworkInterface& sender, const EthernetFrame& frame ) = 0;
    virtual ~OutputPort() = default;
  };

  NetworkInterface( std::string_view name,
                    std::shared_ptr<OutputPort> port,
                    const EthernetAddress& ethernet_address,
                    const Address& ip_address );

  // Wires up the ARP timeout callback. Must run after construction (we need a
  // valid shared_from_this()).
  void initialize();

  // Send an IPv4 datagram via `next_hop`. If we don't yet know the next-hop's
  // MAC, queue the datagram and broadcast an ARP request.
  void send_datagram( const InternetDatagram& dgram, const Address& next_hop );

  // Process an incoming Ethernet frame: dispatch to IPv4 / ARP handling.
  void recv_frame( EthernetFrame frame );

  // Drive periodic timers (ARP cache expiry, in-flight ARP request timeout).
  void tick( size_t ms_since_last_tick );

  const std::string& name() const { return name_; }
  const OutputPort& output() const { return *port_; }
  OutputPort& output() { return *port_; }
  std::queue<InternetDatagram>& datagrams_received() { return datagrams_received_; }

private:
  std::string name_;
  std::shared_ptr<OutputPort> port_;
  EthernetAddress ethernet_address_;
  Address ip_address_;

  ARPTable arp_table_ {};
  ARPMessageQueue arp_queue_ {};
  std::queue<InternetDatagram> datagrams_received_ {};
  bool initialized_ {};

  // Frame helpers.
  void transmit( const EthernetFrame& frame ) const { port_->transmit( *this, frame ); }
  EthernetFrame make_frame( uint16_t type,
                            const EthernetAddress& dst,
                            std::vector<Ref<std::string>> payload ) const;
  void send_ipv4( const InternetDatagram& dgram, const EthernetAddress& dst );

  // ARP helpers.
  void send_arp_request( const Address& next_hop );
  void send_arp_reply( const ARPMessage& request );
  void flush_pending_for( uint32_t ip );
};
