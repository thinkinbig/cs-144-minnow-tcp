#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"
#include <cstdio>

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
    if (input_.writer().is_closed() && window_size_ >= 2) {
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

  // First try to send data
  while (window_remaining > 0 && input_.reader().bytes_buffered() > 0) {
    // Read data and calculate payload size
    string_view data = input_.reader().peek();
    uint64_t payload_size = min({
      window_remaining,
      data.size(),
      TCPConfig::MAX_PAYLOAD_SIZE
    });
    
    if (payload_size == 0) {
      break;
    }

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
      input_.reader().pop(payload_size);
      if (!timer_.running_) {
        timer_.start();
      }
    }

    window_remaining = effective_window - bytes_in_flight_;
  }

  // If we haven't sent FIN yet and the stream is closed and we have window space
  if (!fin_sent_ && input_.writer().is_closed() && input_.reader().bytes_buffered() == 0 && window_remaining > 0) {
    TCPSenderMessage message {
      .seqno = isn_ + next_seqno_,
      .SYN = false,
      .payload = "",
      .FIN = true,
      .RST = false
    };

    transmit(message);
    outstanding_segments_.push_back(message);
    bytes_in_flight_ += message.sequence_length();
    next_seqno_ += message.sequence_length();
    fin_sent_ = true;
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
    .RST = input_.has_error()
  };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if (msg.RST) {
    outstanding_segments_.clear();
    timer_.stop();
    input_.writer().set_error();
    return;
  }

  window_size_ = msg.window_size;

  if (window_size_ == 0) {
    timer_.stop();
  } else if (!timer_.running_) {
    timer_.start();
  }

  if (msg.ackno.has_value()) {
    uint64_t abs_ackno = msg.ackno.value().unwrap(isn_, 0);
    
    if (abs_ackno > next_seqno_) {
      return;
    }

    bool segments_acked = false;
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

    if (segments_acked) {
      timer_.reset();  // 先重置 RTO 和重传计数
      if (!outstanding_segments_.empty()) {
        timer_.start();  // 如果还有未确认的段，则重启计时器
      } else {
        timer_.stop();   // 如果没有未确认的段，则停止计时器
      }
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if (!timer_.running_) {
    return;
  }

  timer_.tick(ms_since_last_tick);
  
  if (timer_.is_expired() && !outstanding_segments_.empty()) {
    if (window_size_ > 0) {
      timer_.consecutive_retransmissions_++;
      timer_.double_RTO();
    }
    
    transmit(outstanding_segments_.front());
    timer_.start();
  }
}