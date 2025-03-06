#include "retransmission_timer.hh"

RetransmissionTimer::RetransmissionTimer(uint64_t initial_RTO_ms)
    : initial_RTO_ms_(initial_RTO_ms)
    , current_RTO_ms_(initial_RTO_ms)
    , running_(false)
    , elapsed_time_ms_(0)
    , consecutive_retransmissions_(0) {}

void RetransmissionTimer::start() {
    running_ = true;
    elapsed_time_ms_ = 0;  // 重置计时器
}

void RetransmissionTimer::stop() {
    running_ = false;
}

void RetransmissionTimer::reset() {
    current_RTO_ms_ = initial_RTO_ms_;
    consecutive_retransmissions_ = 0;
}

void RetransmissionTimer::tick(uint64_t ms_since_last_tick) {
    if (!running_) {
        return;
    }
    elapsed_time_ms_ += ms_since_last_tick;
}

bool RetransmissionTimer::is_expired() const {
    return running_ && (elapsed_time_ms_ >= current_RTO_ms_);
}
