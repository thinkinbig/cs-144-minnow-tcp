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

void Timer::tick(size_t ms_since_last_tick) 
{
  if (!running_) { return; }
  elapsed_time_ms_ += ms_since_last_tick;
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
  consecutive_retransmissions_ = 0;
  elapsed_time_ms_ = 0;
}


void RetransmissionTimer::double_RTO()
{
  current_RTO_ms_ <<= 1;
}
