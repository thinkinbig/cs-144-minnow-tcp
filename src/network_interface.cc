#include "network_interface.hh"
#include "arp_message.hh"
#include "exception.hh"
#include "helpers.hh"

using namespace std;

NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{}

void NetworkInterface::initialize()
{
  if ( initialized_ ) {
    return;
  }
  arp_queue_.set_callback( [this_weak = weak_from_this()]( const Address& next_hop ) {
    if ( auto self = this_weak.lock() ) {
      self->send_arp_request( next_hop );
    }
  } );
  initialized_ = true;
}

void NetworkInterface::tick( size_t ms_since_last_tick )
{
  arp_table_.tick( ms_since_last_tick );
  arp_queue_.tick( ms_since_last_tick );
}

void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  const uint32_t ip = next_hop.ipv4_numeric();
  if ( const auto mac = arp_table_.lookup( ip ); mac.has_value() ) {
    send_ipv4( dgram, *mac );
    return;
  }

  // Unknown MAC: queue the datagram. If this is the first request for this IP,
  // broadcast an ARP query — otherwise an earlier query is still in flight.
  const bool already_querying = arp_queue_.has_pending( ip );
  arp_queue_.add_pending( dgram, next_hop );
  if ( not already_querying ) {
    send_arp_request( next_hop );
  }
}

void NetworkInterface::recv_frame( EthernetFrame frame )
{
  if ( frame.header.dst != ethernet_address_ and frame.header.dst != ETHERNET_BROADCAST ) {
    return;
  }

  switch ( frame.header.type ) {
    case EthernetHeader::TYPE_IPv4: {
      InternetDatagram dgram;
      if ( parse( dgram, frame.payload ) ) {
        datagrams_received_.push( move( dgram ) );
      }
      return;
    }

    case EthernetHeader::TYPE_ARP: {
      ARPMessage msg;
      if ( not parse( msg, frame.payload ) ) {
        return;
      }
      // Either opcode teaches us the sender's mapping.
      arp_table_.add_entry( msg.sender_ip_address, msg.sender_ethernet_address );

      if ( msg.opcode == ARPMessage::OPCODE_REQUEST and msg.target_ip_address == ip_address_.ipv4_numeric() ) {
        send_arp_reply( msg );
      }
      flush_pending_for( msg.sender_ip_address );
      return;
    }

    default:
      return;
  }
}

EthernetFrame NetworkInterface::make_frame( uint16_t type,
                                            const EthernetAddress& dst,
                                            vector<Ref<string>> payload ) const
{
  EthernetFrame frame;
  frame.header.type = type;
  frame.header.src = ethernet_address_;
  frame.header.dst = dst;
  frame.payload = move( payload );
  return frame;
}

void NetworkInterface::send_ipv4( const InternetDatagram& dgram, const EthernetAddress& dst )
{
  transmit( make_frame( EthernetHeader::TYPE_IPv4, dst, serialize( dgram ) ) );
}

void NetworkInterface::send_arp_request( const Address& next_hop )
{
  ARPMessage req;
  req.opcode = ARPMessage::OPCODE_REQUEST;
  req.sender_ethernet_address = ethernet_address_;
  req.sender_ip_address = ip_address_.ipv4_numeric();
  req.target_ip_address = next_hop.ipv4_numeric();
  // target_ethernet_address left zero (unknown — that's why we're asking).

  transmit( make_frame( EthernetHeader::TYPE_ARP, ETHERNET_BROADCAST, serialize( req ) ) );
}

void NetworkInterface::send_arp_reply( const ARPMessage& request )
{
  ARPMessage reply;
  reply.opcode = ARPMessage::OPCODE_REPLY;
  reply.sender_ethernet_address = ethernet_address_;
  reply.sender_ip_address = ip_address_.ipv4_numeric();
  reply.target_ethernet_address = request.sender_ethernet_address;
  reply.target_ip_address = request.sender_ip_address;

  transmit( make_frame( EthernetHeader::TYPE_ARP, request.sender_ethernet_address, serialize( reply ) ) );
}

void NetworkInterface::flush_pending_for( uint32_t ip )
{
  const auto mac = arp_table_.lookup( ip );
  if ( not mac.has_value() ) {
    return;
  }
  for ( const auto& pending : arp_queue_.pop_pending( ip ) ) {
    send_ipv4( pending.dgram, *mac );
  }
}
