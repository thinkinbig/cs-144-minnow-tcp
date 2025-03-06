#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"

#include <cassert>

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return bytes_in_flight_;
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  return timer_.consecutive_retransmissions_;
}

void TCPSender::fill_payload( TCPSenderMessage& message, uint64_t window_available )
{
  if ( window_available > 0 && input_.reader().bytes_buffered() > 0 ) {
    string_view data = input_.reader().peek();
    uint64_t payload_size = min( { window_available, data.size(), TCPConfig::MAX_PAYLOAD_SIZE } );

    if ( payload_size > 0 ) {
      message.payload = string( data.substr( 0, payload_size ) );
      input_.reader().pop( payload_size );
    }
  }
}

bool TCPSender::try_set_fin( TCPSenderMessage& message, uint64_t window_available )
{
  assert( window_available > 0 || window_size_ == 0 );

  if ( !fin_sent_ && input_.writer().is_closed() && input_.reader().bytes_buffered() == 0
       && bytes_in_flight_ + message.payload.size() + 1 <= window_available ) {
    message.FIN = true;
    fin_sent_ = true;
    return true;
  }
  return false;
}

void TCPSender::send_message( TCPSenderMessage& message, const TransmitFunction& transmit )
{

  assert( message.sequence_length() > 0 );
  assert( message.seqno == isn_ + next_seqno_ );

  transmit( message );
  outstanding_segments_.push_back( message );
  bytes_in_flight_ += message.sequence_length();
  next_seqno_ += message.sequence_length();
  if ( !timer_.running_ ) {
    timer_.start();
  }
}

void TCPSender::push( const TransmitFunction& transmit )
{
  // Calculate effective window size
  uint64_t effective_window = window_size_ == 0 ? 1 : window_size_;

  // Try to send SYN first if not sent yet
  if ( next_seqno_ == 0 && !syn_sent_ ) {
    TCPSenderMessage message = make_empty_message();
    message.SYN = true;

    // Calculate available window (reserve 1 for SYN)
    uint64_t available_window = effective_window > 1 ? effective_window - 1 : 0;

    // Try to fill payload if window allows
    fill_payload( message, available_window );

    // Try to set FIN if possible
    try_set_fin( message, effective_window );

    send_message( message, transmit );
    syn_sent_ = true;
    return;
  }

  // Don't send if SYN hasn't been sent
  if ( !syn_sent_ ) {
    return;
  }

  uint64_t window_remaining = effective_window - bytes_in_flight_;

  // Don't send if no window available
  if ( window_remaining == 0 ) {
    return;
  }

  // Try to send standalone FIN
  TCPSenderMessage message = make_empty_message();

  if ( try_set_fin( message, effective_window ) ) {
    send_message( message, transmit );
    return;
  }

  // Send data segments
  while ( window_remaining > 0 && input_.reader().bytes_buffered() > 0 ) {
    message = make_empty_message();

    // Fill payload
    fill_payload( message, window_remaining );

    // Try to set FIN flag
    try_set_fin( message, effective_window );

    send_message( message, transmit );

    window_remaining = effective_window - bytes_in_flight_;
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  return TCPSenderMessage {
    .seqno = isn_ + next_seqno_, .SYN = false, .payload = "", .FIN = false, .RST = input_.has_error() };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( msg.RST ) {
    outstanding_segments_.clear();
    timer_.stop();
    input_.writer().set_error();
    return;
  }

  window_size_ = msg.window_size;

  if ( window_size_ == 0 ) {
    timer_.stop();
  } else if ( !timer_.running_ ) {
    timer_.start();
  }

  if ( msg.ackno.has_value() ) {
    uint64_t abs_ackno = msg.ackno.value().unwrap( isn_, 0 );

    if ( abs_ackno > next_seqno_ ) {
      return;
    }

    bool segments_acked = false;
    while ( !outstanding_segments_.empty() ) {
      const TCPSenderMessage& seg = outstanding_segments_.front();
      uint64_t abs_seqno = seg.seqno.unwrap( isn_, 0 ) + seg.sequence_length();

      if ( abs_seqno <= abs_ackno ) {
        bytes_in_flight_ -= seg.sequence_length();
        outstanding_segments_.pop_front();
        segments_acked = true;
      } else {
        break;
      }
    }

    if ( segments_acked ) {
      timer_.reset();
      if ( !outstanding_segments_.empty() ) {
        timer_.start();
      } else {
        timer_.stop();
      }
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if ( !timer_.running_ ) {
    return;
  }

  timer_.tick( ms_since_last_tick );

  if ( timer_.is_expired() && !outstanding_segments_.empty() ) {
    if ( window_size_ > 0 ) {
      timer_.consecutive_retransmissions_++;
      timer_.double_RTO();
    }

    transmit( outstanding_segments_.front() );
    timer_.start();
  }
}