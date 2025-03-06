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
  return timer_.consecutive_retransmissions_;
}

void TCPSender::push(const TransmitFunction& transmit)
{
  // Try to send SYN first if not sent yet
  if (next_seqno_ == 0 && !syn_sent_) {
    TCPSenderMessage message {
      .seqno = isn_,
      .SYN = true,
      .payload = "",
      .FIN = false,
      .RST = false
    };

    // Set FIN flag if input is closed and window allows
    if (input_.writer().is_closed() && window_size_ >= 2) {  // Need space for both SYN and FIN
      message.FIN = true;
      fin_sent_ = true;
    }

    transmit(message);
    outstanding_segments_.push_back(message);
    bytes_in_flight_ += message.sequence_length();
    next_seqno_ += message.sequence_length();
    syn_sent_ = true;
    if (!timer_.running_) {
      timer_.start();
    }
    return;
  }

  // Don't send if SYN hasn't been sent
  if (!syn_sent_) {
    return;
  }

  // Calculate window sizes
  uint64_t effective_window = window_size_ == 0 ? 1 : window_size_;
  uint64_t window_remaining = effective_window - bytes_in_flight_;
  
  // Don't send if no window available
  if (window_remaining == 0) {
    return;
  }

  // Read data and calculate payload size
  string_view data = input_.reader().peek();
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

  // Try to set FIN flag if:
  // 1. Input is closed
  // 2. All remaining data fits in this segment
  // 3. There's room for FIN in the window
  if (!fin_sent_ &&  // Add this check to avoid setting FIN twice
      input_.writer().is_closed() && 
      input_.reader().peek().size() <= payload_size &&
      bytes_in_flight_ + payload_size + 1 <= effective_window) {
    message.FIN = true;
    fin_sent_ = true;
  }

  // Send if we have data or flags
  if (message.sequence_length() > 0) {
    transmit(message);
    outstanding_segments_.push_back(message);
    bytes_in_flight_ += message.sequence_length();
    next_seqno_ += message.sequence_length();
    if (payload_size > 0) {
      input_.reader().pop(payload_size);
    }
    if (!timer_.running_) {
      timer_.start();
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
  // Handle reset flag
  if (msg.RST) {
    outstanding_segments_.clear();
    timer_.stop();
    input_.writer().set_error();
    return;
  }

  // Update window size
  window_size_ = msg.window_size;
  if (window_size_ == 0) {
    // Zero window, stop timer
    timer_.stop();
  } else if (!timer_.running_) {
    // Window opened, start timer
    timer_.start();
  }

  // Handle acknowledgment
  if (msg.ackno.has_value()) {
    // Get absolute sequence number
    uint64_t abs_ackno = msg.ackno.value().unwrap(isn_, 0);
    
    // Ignore ackno beyond next_seqno
    if (abs_ackno > next_seqno_) {
      return;
    }

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
      timer_.start();  // 重新启动定时器
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if (!timer_.running_) {
    return;
  }

  // Update timer
  timer_.tick(ms_since_last_tick);
  
  // Check for timer expiration
  if (timer_.is_expired() && !outstanding_segments_.empty()) {
    // Increment retransmission counter and double RTO if window is non-zero
    timer_.consecutive_retransmissions_++;
    if (window_size_ > 0) {
      timer_.double_RTO();
    }
    
    // Retransmit the earliest outstanding segment
    transmit(outstanding_segments_.front());
    
    // Reset timer with new RTO
    timer_.start();
  }
}