#include <iostream>

#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "exception.hh"
#include "helpers.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  uint32_t next_hop_ip = next_hop.ipv4_numeric();
  
  // Check if we have a valid ARP entry
  auto it = arp_table_.find(next_hop_ip);
  if (it != arp_table_.end()) {
    send_ipv4_datagram(dgram, next_hop_ip);
    return;
  }

  // Queue the datagram
  PendingDatagram pending;
  pending.dgram = dgram;
  pending.next_hop = next_hop;
  pending_datagrams_.push(pending);

  // Send ARP request if timer allows
  auto timer_it = arp_timers_.find(next_hop_ip);
  if (timer_it == arp_timers_.end() || timer_it->second.is_expired()) {
    send_arp_request(next_hop);
    arp_timers_[next_hop_ip].start();
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  // Check if frame is for us
  if (frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST) {
    return;
  }

  if (frame.header.type == EthernetHeader::TYPE_IPv4) {
    handle_ipv4_datagram(frame);
    return;
  }

  if (frame.header.type == EthernetHeader::TYPE_ARP) {
    ARPMessage arp_msg;
    if (!parse(arp_msg, frame.payload)) {
      return;
    }

    //TODO: rewrite recv frame
    return;

}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // Update all timers
  for (auto& [ip, timer] : arp_timers_) {
    timer.tick(ms_since_last_tick);
  }

  // Clean up expired ARP entries
  for (auto it = arp_table_.begin(); it != arp_table_.end();) {
    if (arp_timers_[it->first].is_expired()) {
      it = arp_table_.erase(it);
    } else {
      ++it;
    }
  }
}

void NetworkInterface::send_arp_request(const Address& next_hop)
{
  // Create and send ARP request
  ARPMessage arp_request;
  arp_request.opcode = ARPMessage::OPCODE_REQUEST;
  arp_request.sender_ethernet_address = ethernet_address_;
  arp_request.sender_ip_address = ip_address_.ipv4_numeric();
  arp_request.target_ip_address = next_hop.ipv4_numeric();

  // Create Ethernet frame for ARP request
  EthernetFrame frame;
  frame.header.type = EthernetHeader::TYPE_ARP;
  frame.header.src = ethernet_address_;
  frame.header.dst = ETHERNET_BROADCAST;
  frame.payload = serialize(arp_request);

  // Send the frame
  transmit(frame);
}

void NetworkInterface::send_ipv4_datagram(const InternetDatagram& dgram, uint32_t next_hop_ip) {
    EthernetHeader eth_header;
    eth_header.dst = arp_table_[next_hop_ip].eth_addr;
    eth_header.src = ethernet_address_;
    eth_header.type = EthernetHeader::TYPE_IPv4;
    EthernetFrame frame;
    frame.header = eth_header;
    frame.payload = serialize(dgram);
    transmit(frame);
}

void NetworkInterface::handle_arp_request(const ARPMessage& arp_msg) {

  // Only respond if the target IP matches our IP
  if (arp_msg.target_ip_address != ip_address_.ipv4_numeric()) {
    return;
  }

  // Create ARP reply
  ARPMessage arp_reply;
  arp_reply.opcode = ARPMessage::OPCODE_REPLY;
  arp_reply.sender_ethernet_address = ethernet_address_;
  arp_reply.sender_ip_address = ip_address_.ipv4_numeric();
  arp_reply.target_ethernet_address = arp_msg.sender_ethernet_address;
  arp_reply.target_ip_address = arp_msg.sender_ip_address;

  // Create and send Ethernet frame
  EthernetFrame reply_frame;
  reply_frame.header.type = EthernetHeader::TYPE_ARP;
  reply_frame.header.src = ethernet_address_;
  reply_frame.header.dst = arp_msg.sender_ethernet_address;
  reply_frame.payload = serialize(arp_reply);
  transmit(reply_frame);
}

void NetworkInterface::handle_ipv4_datagram(const EthernetFrame& frame) {
  InternetDatagram dgram;
  if (!parse(dgram, frame.payload)) {
    return;
  }
  
  // We should accept all IPv4 datagrams that are sent to our MAC address
  // The IP layer above us will handle IP-level filtering
  datagrams_received_.push(dgram);
}