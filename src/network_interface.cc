#include <iostream>

#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "exception.hh"
#include "helpers.hh"
#include "network_interface.hh"

#include <cassert>

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
  , arp_table_()
  , arp_message_queue_()
  , initialized_( false )
  , datagrams_received_()
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

void NetworkInterface::initialize() {
  if (initialized_) {
    return;
  }
  arp_message_queue_.set_observer(weak_from_this().lock());
  initialized_ = true;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    // 更新 ARP 表
    arp_table_.tick(ms_since_last_tick);
    
    // 更新待处理队列
    arp_message_queue_.tick(ms_since_last_tick);
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram(const InternetDatagram& dgram, const Address& next_hop) {
    if (arp_table_.lookup(next_hop.ipv4_numeric()).has_value()) {
        send_ipv4_datagram(dgram, next_hop);
    } else {
        // 无论是否已经有待处理的数据报，都要将新的数据报加入队列
        bool had_pending = arp_message_queue_.has_pending(next_hop.ipv4_numeric());
        arp_message_queue_.add_pending(dgram, next_hop);
        
        // 如果没有待处理的数据报，说明这是第一个请求，需要发送 ARP 请求
        // 如果已经有待处理的数据报，说明 ARP 请求已经发送，等待超时重试即可
        if (!had_pending) {
            send_arp_request(next_hop);
        }
    }
}

void NetworkInterface::send_arp_request(const Address& next_hop) {
    // 发送 ARP 请求
    ARPMessage arp_request;
    arp_request.opcode = ARPMessage::OPCODE_REQUEST;
    arp_request.sender_ethernet_address = ethernet_address_;
    arp_request.sender_ip_address = ip_address_.ipv4_numeric();
    arp_request.target_ethernet_address = {};
    arp_request.target_ip_address = next_hop.ipv4_numeric();

    EthernetFrame frame;
    frame.header.type = EthernetHeader::TYPE_ARP;
    frame.header.src = ethernet_address_;
    frame.header.dst = ETHERNET_BROADCAST;
    frame.payload = serialize(arp_request);
    transmit(frame);
}

void NetworkInterface::send_ipv4_datagram(const InternetDatagram& dgram, const Address& next_hop) {
    // 发送 IPv4 数据报
    assert(arp_table_.lookup(next_hop.ipv4_numeric()).has_value());
    EthernetFrame frame;
    frame.header.type = EthernetHeader::TYPE_IPv4;
    frame.header.src = ethernet_address_;
    frame.header.dst = arp_table_.lookup(next_hop.ipv4_numeric()).value();
    frame.payload = serialize(dgram);
    transmit(frame);
}

void NetworkInterface::confirm_arp_reply(uint32_t ip_addr) {
    // 获取所有等待该 IP 的数据报
    auto pending_datagrams = arp_message_queue_.pop_pending(ip_addr);
    
    // 发送所有数据报
    for (const auto& pending : pending_datagrams) {
        send_ipv4_datagram(pending.dgram, pending.next_hop);
    }
}

void NetworkInterface::send_arp_reply(const ARPMessage& arp_request) {
    ARPMessage arp_reply;
    arp_reply.opcode = ARPMessage::OPCODE_REPLY;
    arp_reply.sender_ethernet_address = ethernet_address_;
    arp_reply.sender_ip_address = ip_address_.ipv4_numeric();
    arp_reply.target_ethernet_address = arp_request.sender_ethernet_address;
    arp_reply.target_ip_address = arp_request.sender_ip_address;

    EthernetFrame frame;
    frame.header.type = EthernetHeader::TYPE_ARP;
    frame.header.src = ethernet_address_;
    frame.header.dst = arp_request.sender_ethernet_address;
    frame.payload = serialize(arp_reply);
    transmit(frame);
}

void NetworkInterface::recv_frame(EthernetFrame frame) {
    // 检查是否是发给我们的帧
    if (frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST) {
        return;
    }

    if (frame.header.type == EthernetHeader::TYPE_IPv4) {
      InternetDatagram dgram;
      if (parse(dgram, frame.payload)) {
        datagrams_received_.push(dgram);
      }
    } else if (frame.header.type == EthernetHeader::TYPE_ARP) {
      ARPMessage arp_msg;
      if (parse(arp_msg, frame.payload)) {
        // 学习发送方的映射
        arp_table_.add_entry(arp_msg.sender_ip_address, arp_msg.sender_ethernet_address);

        if (arp_msg.opcode == ARPMessage::OPCODE_REPLY) {
          // 收到ARP回复，发送等待的数据报
          confirm_arp_reply(arp_msg.sender_ip_address);
        } else if (arp_msg.opcode == ARPMessage::OPCODE_REQUEST && 
                  arp_msg.target_ip_address == ip_address_.ipv4_numeric()) {
          // 发送 ARP 回复
          send_arp_reply(arp_msg);
        }
      }
    }
}