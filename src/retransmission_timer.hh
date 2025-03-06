#pragma once

#include "tcp_sender_message.hh"

#include <functional>

class TCPSender;

struct RetransmissionTimer
{
public:
  explicit RetransmissionTimer( uint64_t initial_RTO_ms );

  void start();

  void stop();

  void reset();

  void tick( uint64_t ms_since_last_tick );

  bool is_expired() const;

  void double_RTO();

  void increment_retransmissions() { consecutive_retransmissions_++; }

  uint64_t consecutive_retransmissions() const { return consecutive_retransmissions_; }

  bool is_running() const { return running_; }

private:
  friend class TCPSender;

  const uint64_t initial_RTO_ms_;
  uint64_t current_RTO_ms_;
  bool running_;
  uint64_t elapsed_time_ms_;
  uint64_t consecutive_retransmissions_;
};