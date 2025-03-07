#pragma once

#include "tcp_sender_message.hh"

#include <functional>
#include <cstddef>

class TCPSender;

class NetworkInterface;

// 基础计时器抽象类
class Timer {
public:
  virtual ~Timer() = default;
  
  virtual void start();

  virtual void stop();

  virtual void tick(size_t ms_since_last_tick);

  virtual bool is_expired() const = 0;
  
  bool is_running() const { return running_; }

protected:
  bool running_ { false };
  size_t elapsed_time_ms_ { 0 };
};

// RetransmissionTimer 继承自 Timer
class RetransmissionTimer : public Timer
{
public:
  explicit RetransmissionTimer( uint64_t initial_RTO_ms );

  void reset();
  bool is_expired() const override;
  void double_RTO();
  void increment_retransmissions() { consecutive_retransmissions_++; }
  uint64_t consecutive_retransmissions() const { return consecutive_retransmissions_; }

private:
  friend class TCPSender;

  const uint64_t initial_RTO_ms_;
  uint64_t current_RTO_ms_;
  uint64_t consecutive_retransmissions_ { 0 };
};


class ARPTimer : public Timer {
public:
  static constexpr size_t TIMEOUT_MS = 5000;  // 5 seconds timeout

  bool is_expired() const override;

private:
  friend class NetworkInterface;
};