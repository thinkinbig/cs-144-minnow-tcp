#include "timer.hh"

void Timer::start()
{
  running_ = true;
  elapsed_time_ms_ = 0;
}

void Timer::stop()
{
  running_ = false;
  elapsed_time_ms_ = 0;
}

void Timer::tick( size_t ms_since_last_tick )
{
  if ( !running_ ) {
    return;
  }
  elapsed_time_ms_ += ms_since_last_tick;
}

bool Timer::is_expired() const
{
  return running_ && ( elapsed_time_ms_ >= timeout_ms_ );
}

bool Timer::is_running() const
{
  return running_;
}

size_t Timer::get_timeout_ms() const
{
  return timeout_ms_;
}

size_t Timer::get_elapsed_ms() const
{
  return elapsed_time_ms_;
}

RetransmissionTimer::RetransmissionTimer( uint64_t initial_RTO_ms )
  : Timer( initial_RTO_ms )
  , initial_RTO_ms_( initial_RTO_ms )
  , current_RTO_ms_( initial_RTO_ms )
  , consecutive_retransmissions_( 0 )
{}

void RetransmissionTimer::reset()
{
  current_RTO_ms_ = initial_RTO_ms_;
  timeout_ms_ = current_RTO_ms_;
  consecutive_retransmissions_ = 0;
  elapsed_time_ms_ = 0;
}

void RetransmissionTimer::double_RTO()
{
  current_RTO_ms_ <<= 1;
  timeout_ms_ = current_RTO_ms_;
}

void RetransmissionTimer::increment_retransmissions()
{
  consecutive_retransmissions_++;
}

uint64_t RetransmissionTimer::consecutive_retransmissions() const
{
  return consecutive_retransmissions_;
}

NetworkTimer NetworkTimer::create_request_timer()
{
  return NetworkTimer( ARP_REQUEST_TIMEOUT );
}

NetworkTimer NetworkTimer::create_entry_timer()
{
  return NetworkTimer( ARP_ENTRY_TIMEOUT );
}
