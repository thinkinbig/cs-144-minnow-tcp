#pragma once

#include "address.hh"
#include "ethernet_frame.hh"
#include "ipv4_datagram.hh"
#include "timer.hh"
#include "arp_table.hh"
#include "pending_arp_queue.hh"
#include <memory>
#include <queue>
#include <unordered_map>
#include <optional>

// A "network interface" that connects IP (the internet layer, or network layer)
// with Ethernet (the network access layer, or link layer).

// This module is the lowest layer of a TCP/IP stack
// (connecting IP with the lower-layer network protocol,
// e.g. Ethernet). But the same module is also used repeatedly
// as part of a router: a router generally has many network
// interfaces, and the router's job is to route Internet datagrams
// between the different interfaces.

// The network interface translates datagrams (coming from the
// "customer," e.g. a TCP/IP stack or router) into Ethernet
// frames. To fill in the Ethernet destination address, it looks up
// the Ethernet address of the next IP hop of each datagram, making
// requests with the [Address Resolution Protocol](\ref rfc::rfc826).
// In the opposite direction, the network interface accepts Ethernet
// frames, checks if they are intended for it, and if so, processes
// the the payload depending on its type. If it's an IPv4 datagram,
// the network interface passes it up the stack. If it's an ARP
// request or reply, the network interface processes the frame
// and learns or replies as necessary.

class NetworkInterface : public std::enable_shared_from_this<NetworkInterface>
{
public:
  // An abstraction for the physical output port where the NetworkInterface sends Ethernet frames
  class OutputPort
  {
  public:
    virtual void transmit( const NetworkInterface& sender, const EthernetFrame& frame ) = 0;
    virtual ~OutputPort() = default;
  };

  // ARP请求超时处理器
  class ARPRequestHandler : public ARPRequestTimeoutObserver {
  public:
    explicit ARPRequestHandler(std::shared_ptr<NetworkInterface> interface) 
      : interface_(interface) {}
    
    void on_arp_request_timeout(const Address& next_hop) override {
      interface_->send_arp_request(next_hop);
    }
  private:
    std::shared_ptr<NetworkInterface> interface_;
  };

  // Construct a network interface with given Ethernet (network-access-layer) and IP (internet-layer)
  // addresses
  NetworkInterface( std::string_view name,
                    std::shared_ptr<OutputPort> port,
                    const EthernetAddress& ethernet_address,
                    const Address& ip_address );

  // 初始化方法，在构造完成后调用
  void initialize() {
    if (initialized_) {
      return;
    }
    arp_handler_ = std::make_shared<ARPRequestHandler>(shared_from_this());
    pending_arp_queue_.set_timeout_observer(arp_handler_);
    initialized_ = true;
  }

  // Sends an Internet datagram, encapsulated in an Ethernet frame (if it knows the Ethernet destination
  // address). Will need to use [ARP](\ref rfc::rfc826) to look up the Ethernet destination address for the next
  // hop. Sending is accomplished by calling `transmit()` (a member variable) on the frame.
  void send_datagram( const InternetDatagram& dgram, const Address& next_hop );

  // Receives an Ethernet frame and responds appropriately.
  // If type is IPv4, pushes the datagram to the datagrams_in queue.
  // If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
  // If type is ARP reply, learn a mapping from the "sender" fields.
  void recv_frame( EthernetFrame frame );

  // Called periodically when time elapses
  void tick( size_t ms_since_last_tick );

  // Accessors
  const std::string& name() const { return name_; }
  const OutputPort& output() const { return *port_; }
  OutputPort& output() { return *port_; }

private:
  // Human-readable name of the interface
  std::string name_;

  // The physical output port (+ a helper function `transmit` that uses it to send an Ethernet frame)
  std::shared_ptr<OutputPort> port_;
  void transmit( const EthernetFrame& frame ) const { port_->transmit( *this, frame ); }

  // Ethernet (known as hardware, network-access-layer, or link-layer) address of the interface
  EthernetAddress ethernet_address_;

  // IP (known as internet-layer or network-layer) address of the interface
  Address ip_address_;

  // ARP table for IP to Ethernet address resolution
  ARPTable arp_table_;

  // Queue for datagrams waiting for ARP replies
  PendingARPQueue pending_arp_queue_ {};

  // ARP 请求处理器
  std::shared_ptr<ARPRequestHandler> arp_handler_;
  
  // 初始化标志
  bool initialized_ = false;

  // ARP 相关方法
  void send_arp_request(const Address& next_hop);
  void send_ipv4_datagram(const InternetDatagram& dgram, const Address& next_hop);
  void confirm_arp_reply(uint32_t ip_addr);  // 处理 ARP 回复
};
