
#pragma once

#include <cstddef>
#include <cstdint>

// A simple millisecond countdown timer. start() begins counting, tick() advances,
// is_expired() reports whether timeout has been reached.
class Timer
{
public:
  explicit Timer( size_t timeout_ms ) : timeout_ms_( timeout_ms ) {}

  void start()
  {
    running_ = true;
    elapsed_ms_ = 0;
  }
  void stop()
  {
    running_ = false;
    elapsed_ms_ = 0;
  }
  void tick( size_t ms_since_last_tick )
  {
    if ( running_ ) {
      elapsed_ms_ += ms_since_last_tick;
    }
  }

  bool is_running() const { return running_; }
  bool is_expired() const { return running_ && elapsed_ms_ >= timeout_ms_; }
  size_t timeout_ms() const { return timeout_ms_; }
  void set_timeout_ms( size_t t ) { timeout_ms_ = t; }

private:
  size_t timeout_ms_;
  size_t elapsed_ms_ {};
  bool running_ {};
};

// ARP-related timeouts. Just a Timer with named constants for the common values.
class NetworkTimer : public Timer
{
public:
  static constexpr size_t ARP_REQUEST_TIMEOUT = 5000;  // 5 seconds
  static constexpr size_t ARP_ENTRY_TIMEOUT = 30000;   // 30 seconds

  using Timer::Timer;
};

// Retransmission timer with exponential backoff. Owns a Timer plus the
// current RTO and a count of consecutive retransmissions.
class RetransmissionTimer
{
public:
  explicit RetransmissionTimer( uint64_t initial_RTO_ms )
    : initial_RTO_ms_( initial_RTO_ms ), timer_( initial_RTO_ms )
  {}

  void start() { timer_.start(); }
  void stop() { timer_.stop(); }
  void tick( size_t ms_since_last_tick ) { timer_.tick( ms_since_last_tick ); }
  bool is_running() const { return timer_.is_running(); }
  bool is_expired() const { return timer_.is_expired(); }

  // Successful new ACK: reset RTO and backoff counter. Caller chooses
  // whether to start() or stop() afterwards depending on outstanding data.
  void reset_backoff()
  {
    timer_.set_timeout_ms( initial_RTO_ms_ );
    consecutive_retransmissions_ = 0;
  }

  // RTO expired and a real retransmission is being sent (window > 0):
  // double the RTO and bump the count. Caller still calls start() to
  // restart the elapsed clock.
  void record_retransmission()
  {
    ++consecutive_retransmissions_;
    timer_.set_timeout_ms( timer_.timeout_ms() * 2 );
  }

  uint64_t consecutive_retransmissions() const { return consecutive_retransmissions_; }

private:
  uint64_t initial_RTO_ms_;
  Timer timer_;
  uint64_t consecutive_retransmissions_ {};
};
