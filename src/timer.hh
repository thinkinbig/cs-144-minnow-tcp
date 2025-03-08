#pragma once

#include "tcp_sender_message.hh"

#include <functional>
#include <cstddef>

class TCPSender;

class NetworkInterface;

// 基础计时器抽象类
class Timer {
public:
  explicit Timer(size_t timeout_ms) : timeout_ms_(timeout_ms) {}
  virtual ~Timer() = default;
  
  void start() {
    running_ = true;
    elapsed_time_ms_ = 0;
  }

  void stop() {
    running_ = false;
    elapsed_time_ms_ = 0;
  }

  void tick(size_t ms_since_last_tick) {
    if (!running_) { return; }
    elapsed_time_ms_ += ms_since_last_tick;
  }

  bool is_expired() const {
    return running_ && (elapsed_time_ms_ >= timeout_ms_);
  }
  
  bool is_running() const { return running_; }

  size_t get_timeout_ms() const { return timeout_ms_; }
  size_t get_elapsed_ms() const { return elapsed_time_ms_; }

protected:
  bool running_ { false };
  size_t elapsed_time_ms_ { 0 };
  size_t timeout_ms_;
};

// RetransmissionTimer 继承自 Timer
class RetransmissionTimer : public Timer
{
public:
  explicit RetransmissionTimer( uint64_t initial_RTO_ms )
    : Timer(initial_RTO_ms)
    , initial_RTO_ms_( initial_RTO_ms )
    , current_RTO_ms_( initial_RTO_ms )
    , consecutive_retransmissions_( 0 )
  {}

  void reset() {
    current_RTO_ms_ = initial_RTO_ms_;
    consecutive_retransmissions_ = 0;
    elapsed_time_ms_ = 0;
  }

  void double_RTO() {
    current_RTO_ms_ <<= 1;
  }

  void increment_retransmissions() { consecutive_retransmissions_++; }
  uint64_t consecutive_retransmissions() const { return consecutive_retransmissions_; }

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

  // 创建 ARP 请求定时器
  static NetworkTimer create_request_timer() {
    return NetworkTimer(ARP_REQUEST_TIMEOUT);
  }

  // 创建 ARP 表项定时器
  static NetworkTimer create_entry_timer() {
    return NetworkTimer(ARP_ENTRY_TIMEOUT);
  }

private:
  friend class NetworkInterface;
};
