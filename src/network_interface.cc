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
  // If the destination Ethernet address is already known, send it right away
  if (arp_table_.find(dgram.header.dst) != arp_table_.end()) {
    send_ipv4_datagram(dgram);
    return;
  }

  // Send ARP request if timer is not running or has expired
  if (!arp_timer_.is_running() || arp_timer_.is_expired()) {
    send_arp_request(dgram, next_hop);
    arp_timer_.start();  // Start/restart the timer
  }
  
  // Store the datagram in pending queue
  PendingDatagram pending;
  pending.dgram = dgram;
  pending.next_hop = next_hop;
  pending.time_sent = arp_timer_.elapsed_time_ms_;
  pending_datagrams_.push(pending);
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  // Ignore frames not destined for us (unless broadcast)
  if (frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST) {
    return;
  }

  // Handle frame based on type
  if (frame.header.type == EthernetHeader::TYPE_IPv4) {
    handle_ipv4_datagram(frame);
  } else if (frame.header.type == EthernetHeader::TYPE_ARP) {
    ARPMessage arp_msg;
    if (!parse(arp_msg, frame.payload)) {
      return;
    }

    if (arp_msg.opcode == ARPMessage::OPCODE_REPLY) {
      handle_arp_reply(frame);
    } else if (arp_msg.opcode == ARPMessage::OPCODE_REQUEST) {
      handle_arp_request(frame);
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // Update ARP timer
  arp_timer_.tick(ms_since_last_tick);

  // Clean expired ARP cache entries
  for (auto it = arp_table_.begin(); it != arp_table_.end();) {
    if (arp_timer_.elapsed_time_ms_ >= it->second.expire_time) {
      it = arp_table_.erase(it);
    } else {
      ++it;
    }
  }

  // Check if need to resend ARP request
  if (arp_timer_.is_expired() && !pending_datagrams_.empty()) {
    const auto& pending = pending_datagrams_.front();
    send_arp_request(pending.dgram, pending.next_hop);
    arp_timer_.start();  // Restart timer for next attempt
  }
}

void NetworkInterface::send_arp_request(const InternetDatagram& dgram, const Address& next_hop)
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

void NetworkInterface::send_ipv4_datagram(const InternetDatagram& dgram) {
    EthernetHeader eth_header;
    eth_header.dst = arp_table_[dgram.header.dst].eth_addr;
    eth_header.src = ethernet_address_;
    eth_header.type = EthernetHeader::TYPE_IPv4;
    EthernetFrame frame;
    frame.header = eth_header;
    frame.payload = serialize(dgram);
    transmit(frame);
}

void NetworkInterface::handle_arp_reply(const EthernetFrame& frame) {
  ARPMessage arp_msg;
  if (!parse(arp_msg, frame.payload)) {
    return;
  }

  // Update ARP cache
  ARPEntry entry;
  entry.eth_addr = arp_msg.sender_ethernet_address;
  entry.expire_time = arp_timer_.elapsed_time_ms_ + ARP_ENTRY_TTL;
  arp_table_[arp_msg.sender_ip_address] = entry;

  // Send pending datagrams
  while (!pending_datagrams_.empty()) {
    const auto& pending = pending_datagrams_.front();
    if (arp_table_.find(pending.next_hop.ipv4_numeric()) != arp_table_.end()) {
      send_ipv4_datagram(pending.dgram);
      pending_datagrams_.pop();
    } else {
      break;
    }
  }
}

void NetworkInterface::handle_arp_request(const EthernetFrame& frame) {
  ARPMessage arp_msg;
  if (!parse(arp_msg, frame.payload)) {
    return;
  }

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

  // Learn the mapping from the request
  ARPEntry entry;
  entry.eth_addr = arp_msg.sender_ethernet_address;
  entry.expire_time = arp_timer_.elapsed_time_ms_ + ARP_ENTRY_TTL;
  arp_table_[arp_msg.sender_ip_address] = entry;
}

void NetworkInterface::handle_ipv4_datagram(const EthernetFrame& frame) {
  InternetDatagram dgram;
  if (!parse(dgram, frame.payload)) {
    return;
  }
  
  // Only accept if destined for us
  if (dgram.header.dst != ip_address_.ipv4_numeric()) {
    return;
  }

  datagrams_received_.push(dgram);
}