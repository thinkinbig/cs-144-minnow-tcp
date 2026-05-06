#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <algorithm>

using namespace std;

TCPSenderMessage TCPSender::make_empty_message() const
{
  return { .seqno = isn_ + next_seqno_, .SYN = false, .payload = {}, .FIN = false, .RST = input_.has_error() };
}

bool TCPSender::send_segment( const TransmitFunction& transmit, uint64_t window_remaining )
{
  if ( window_remaining == 0 ) {
    return false;
  }

  TCPSenderMessage msg = make_empty_message();
  uint64_t length = 0;

  if ( not syn_sent_ ) {
    msg.SYN = true;
    ++length;
  }

  // Pull as much payload as fits into both the window and a single segment.
  const uint64_t payload_room = min( window_remaining - length, TCPConfig::MAX_PAYLOAD_SIZE );
  Reader& reader = input_.reader();
  while ( msg.payload.size() < payload_room and reader.bytes_buffered() > 0 ) {
    const string_view view = reader.peek().substr( 0, payload_room - msg.payload.size() );
    msg.payload.append( view );
    reader.pop( view.size() );
  }
  length += msg.payload.size();

  // Piggyback FIN if the stream just closed and the receiver has room for it.
  if ( not fin_sent_ and input_.writer().is_closed() and reader.bytes_buffered() == 0
       and length < window_remaining ) {
    msg.FIN = true;
    ++length;
  }

  if ( length == 0 ) {
    return false;
  }

  transmit( msg );
  next_seqno_ += length;
  bytes_in_flight_ += length;
  syn_sent_ = syn_sent_ or msg.SYN;
  fin_sent_ = fin_sent_ or msg.FIN;

  outstanding_.push_back( move( msg ) );
  if ( not timer_.is_running() ) {
    timer_.start();
  }
  return true;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  // Treat a closed-window receiver as window=1 to allow probing (RFC 793 §3.7).
  const uint64_t effective_window = zero_window_ ? 1 : window_size_;
  const uint64_t abs_ack = next_seqno_ - bytes_in_flight_;
  const uint64_t right_edge = abs_ack + effective_window;

  while ( next_seqno_ < right_edge and not fin_sent_ ) {
    if ( not send_segment( transmit, right_edge - next_seqno_ ) ) {
      break;
    }
  }
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( msg.RST ) {
    outstanding_.clear();
    bytes_in_flight_ = 0;
    timer_.stop();
    input_.set_error();
    return;
  }

  zero_window_ = ( msg.window_size == 0 );
  window_size_ = msg.window_size;

  if ( not msg.ackno.has_value() ) {
    return;
  }
  const uint64_t abs_ackno = msg.ackno->unwrap( isn_, next_seqno_ );
  if ( abs_ackno > next_seqno_ ) {
    return; // ACK for data we haven't sent — ignore.
  }

  bool acked_anything = false;
  while ( not outstanding_.empty() ) {
    const TCPSenderMessage& seg = outstanding_.front();
    const uint64_t seg_end = seg.seqno.unwrap( isn_, next_seqno_ ) + seg.sequence_length();
    if ( seg_end > abs_ackno ) {
      break;
    }
    bytes_in_flight_ -= seg.sequence_length();
    outstanding_.pop_front();
    acked_anything = true;
  }

  if ( acked_anything ) {
    timer_.reset_backoff();
    if ( outstanding_.empty() ) {
      timer_.stop();
    } else {
      timer_.start(); // restart with fresh RTO
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  timer_.tick( ms_since_last_tick );
  if ( not timer_.is_expired() or outstanding_.empty() ) {
    return;
  }

  transmit( outstanding_.front() );
  // Don't penalize ourselves for retransmitting into a closed window — the
  // peer wasn't going to take it anyway.
  if ( not zero_window_ ) {
    timer_.record_retransmission();
  }
  timer_.start();
}
