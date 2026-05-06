#include "tcp_receiver.hh"

#include <algorithm>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    reassembler_.set_error();
    reassembler_.close();
    return;
  }

  if ( not isn_.has_value() ) {
    if ( not message.SYN ) {
      return; // Pre-handshake: anything but SYN is dropped.
    }
    isn_ = message.seqno;
  }

  // Stream index of the first byte of `payload`. SYN occupies seqno 0 of the
  // logical stream, so non-SYN segments have their seqnos shifted by one.
  const uint64_t checkpoint = reassembler_.writer().bytes_pushed();
  const uint64_t abs_seqno = message.seqno.unwrap( *isn_, checkpoint );
  const uint64_t stream_index = message.SYN ? abs_seqno : abs_seqno - 1;

  reassembler_.insert( stream_index, move( message.payload ), message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  const Writer& writer = reassembler_.writer();
  const uint16_t window_size = static_cast<uint16_t>( min<uint64_t>( writer.available_capacity(), UINT16_MAX ) );

  if ( reassembler_.has_error() ) {
    return { .ackno = nullopt, .window_size = 0, .RST = true };
  }
  if ( not isn_.has_value() ) {
    return { .ackno = nullopt, .window_size = window_size, .RST = false };
  }

  // Acknowledge: SYN (1) + bytes received + (FIN if stream is closed).
  const uint64_t next_seqno = 1 + writer.bytes_pushed() + ( writer.is_closed() ? 1 : 0 );
  return { .ackno = Wrap32::wrap( next_seqno, *isn_ ), .window_size = window_size, .RST = false };
}
