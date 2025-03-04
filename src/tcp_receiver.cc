#include <cassert>
#include <cstdint>

#include "debug.hh"
#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // 1. Error handling: RST flag immediately terminates the connection
  if ( message.RST ) {
    reassembler_.set_error();
    reassembler_.close();
    return;
  }

  // 2. Connection establishment: handle SYN
  if ( !initialized_ ) {
    if ( !message.SYN ) {
      return; // Ignore non-SYN segments before initialization
    }
    zero_point_ = message.seqno;
    initialized_ = true;
  }

  // 3. Data processing
  uint64_t checkpoint = reassembler_.writer().bytes_pushed();
  uint64_t stream_index = message.seqno.unwrap( zero_point_, checkpoint );

  // Adjust stream index for non-SYN segments
  if ( !message.SYN ) {
    stream_index -= 1;
  }

  // Discard already processed segments, except empty FIN
  if ( !message.SYN && stream_index + message.payload.size() <= reassembler_.writer().bytes_pushed()
       && !( message.payload.empty() && message.FIN ) ) {
    return;
  }

  // Reassemble data
  reassembler_.insert( stream_index, message.payload, message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  // Return error message if reassembler has error
  if ( reassembler_.has_error() ) {
    return TCPReceiverMessage {
      .ackno = {},
      .window_size = 0,
      .RST = true,
    };
  }

  // Calculate window size, capped at UINT16_MAX
  uint64_t available_capacity = reassembler_.writer().available_capacity();
  uint16_t window_size = available_capacity > UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>( available_capacity );

  // Return uninitialized message if no SYN received
  if ( !initialized_ ) {
    return TCPReceiverMessage {
      .ackno = {},
      .window_size = window_size,
      .RST = false,
    };
  }

  // Calculate next expected sequence number:
  // 1. SYN occupies first sequence number
  // 2. Add pushed bytes
  // 3. FIN occupies last sequence number if stream is closed
  uint64_t next_seqno = 1;                            // For SYN
  next_seqno += reassembler_.writer().bytes_pushed(); // For data
  if ( reassembler_.writer().is_closed() ) {
    next_seqno += 1; // For FIN
  }

  return TCPReceiverMessage {
    .ackno = Wrap32::wrap( next_seqno, zero_point_ ),
    .window_size = window_size,
    .RST = false,
  };
}
