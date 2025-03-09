#pragma once

#include "tcp_sender_message.hh"

#include <functional>
#include <cstddef>

class TCPSender;

// 基础计时器抽象类
class Timer {
public:
  explicit Timer(size_t timeout_ms) : timeout_ms_(timeout_ms) {}
  virtual ~Timer() = default;
  
  void start();
  void stop();
  void tick(size_t ms_since_last_tick);
  bool is_expired() const;
  bool is_running() const;
  size_t get_timeout_ms() const;
  size_t get_elapsed_ms() const;

protected:
  bool running_ { false };
  size_t elapsed_time_ms_ { 0 };
  size_t timeout_ms_;
};

// RetransmissionTimer 继承自 Timer
class RetransmissionTimer : public Timer
{
public:
  explicit RetransmissionTimer(uint64_t initial_RTO_ms);

  void reset();
  void double_RTO();
  void increment_retransmissions();
  uint64_t consecutive_retransmissions() const;

private:
  friend class TCPSender;

  const uint64_t initial_RTO_ms_;
  uint64_t current_RTO_ms_;
  uint64_t consecutive_retransmissions_ { 0 };
};

// 通用的网络接口定时器
class NetworkTimer : public Timer {
public:
  // 预定义的超时时间
  static constexpr size_t ARP_REQUEST_TIMEOUT = 5000;   // 5 seconds
  static constexpr size_t ARP_ENTRY_TIMEOUT = 30000;    // 30 seconds

  explicit NetworkTimer(size_t timeout_ms) : Timer(timeout_ms) {}

  static NetworkTimer create_request_timer();
  static NetworkTimer create_entry_timer();
};
