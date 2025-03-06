#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return bytes_in_flight_;
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  return timer_.consecutive_retransmissions();
}

void TCPSender::push( const TransmitFunction& transmit )
{
  // First, handle SYN if not sent yet
  if (next_seqno_ == 0 && !syn_sent_) {
    TCPSenderMessage message {
      .seqno = isn_,
      .SYN = true,
      .payload = "",
      .FIN = false,
      .RST = false
    };
    transmit(message);
    outstanding_segments_.push_back(message);
    bytes_in_flight_ += message.sequence_length();
    next_seqno_ += message.sequence_length();
    syn_sent_ = true;
    if (!timer_.is_running()) {
      timer_.start(last_tick_ms_);
    }
    return;
  }

  // Don't send if SYN hasn't been sent
  if (!syn_sent_) {
    return;
  }

  // Calculate available window (minimum 1 for zero window probing)
  uint64_t effective_window = window_size_ ? window_size_ : 1;
  uint64_t window_remaining = effective_window - bytes_in_flight_;
  
  // Don't send if no window available
  if (window_remaining == 0) {
    return;
  }

  // Read data from input stream
  string_view data = input_.reader().peek();
  
  // Calculate payload size considering:
  // 1. Available window
  // 2. Maximum payload size
  // 3. Available data
  uint64_t payload_size = min({
    window_remaining,
    data.size(),
    TCPConfig::MAX_PAYLOAD_SIZE
  });
  
  // Create message
  TCPSenderMessage message {
    .seqno = isn_ + next_seqno_,
    .SYN = false,
    .payload = string(data.substr(0, payload_size)),
    .FIN = false,
    .RST = false
  };

  // Add FIN if:
  // 1. Input is finished
  // 2. FIN hasn't been sent yet
  // 3. Window has enough space for payload + FIN
  if (input_.reader().is_finished() && 
      !fin_sent_ &&
      bytes_in_flight_ + payload_size + 1 <= effective_window) {
    message.FIN = true;
    fin_sent_ = true;
  }

  // Only send if we have data or flags
  if (message.sequence_length() > 0) {
    // Update state
    bytes_in_flight_ += message.sequence_length();
    next_seqno_ += message.sequence_length();
    if (payload_size > 0) {
      input_.reader().pop(payload_size);
    }
    
    // Send message and start timer
    transmit(message);
    outstanding_segments_.push_back(message);
    if (!timer_.is_running()) {
      timer_.start(last_tick_ms_);
    }
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  return TCPSenderMessage {
    .seqno = isn_ + next_seqno_,
    .SYN = false,
    .payload = "",
    .FIN = false,
    .RST = false
  };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Handle acknowledgment
  handle_ack(msg);
  
  // Handle window update
  handle_window_update(msg);
  
  // Handle reset flag
  if (msg.RST) {
    handle_rst();
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // Update timer and last tick time
  timer_.tick(ms_since_last_tick);
  last_tick_ms_ += ms_since_last_tick;
  
  // Check for timer expiration
  if (timer_.is_expired(ms_since_last_tick) && !outstanding_segments_.empty()) {
    // Increment retransmission counter
    timer_.increment_retransmissions();
    
    // Double RTO if window is non-zero
    if (window_size_ > 0) {
      timer_.double_RTO();
    }
    
    // Retransmit the earliest outstanding segment
    transmit(outstanding_segments_.front());
    
    // Restart timer
    timer_.start(last_tick_ms_);
  }
}

void TCPSender::handle_window_update(const TCPReceiverMessage& msg)
{
  window_size_ = msg.window_size;
}

void TCPSender::handle_rst()
{
  outstanding_segments_.clear();
  timer_.stop();
  input_.writer().set_error();
}

void TCPSender::handle_ack(const TCPReceiverMessage& msg)
{
  if (!msg.ackno.has_value()) {
    return;
  }

  // Get absolute sequence number
  uint64_t abs_ackno = msg.ackno.value().unwrap(isn_, 0);
  bool segments_acked = false;
  
  // Remove acknowledged segments
  while (!outstanding_segments_.empty()) {
    const TCPSenderMessage& seg = outstanding_segments_.front();
    uint64_t abs_seqno = seg.seqno.unwrap(isn_, 0) + seg.sequence_length();
    
    if (abs_seqno <= abs_ackno) {
      bytes_in_flight_ -= seg.sequence_length();
      outstanding_segments_.pop_front();
      segments_acked = true;
    } else {
      break;
    }
  }

  // Reset timer if we received new acknowledgments
  if (outstanding_segments_.empty()) {
    timer_.stop();
  } else if (segments_acked) {
    timer_.reset();
  }
}
