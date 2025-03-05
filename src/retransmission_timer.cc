#include "retransmission_timer.hh"

RetransmissionTimer::RetransmissionTimer(uint64_t initial_RTO_ms)
    : initial_RTO_ms_(initial_RTO_ms)
    , current_RTO_ms_(initial_RTO_ms)
    , running_(false)
    , start_time_ms_(0)
    , consecutive_retransmissions_(0) {}

void RetransmissionTimer::start(uint64_t now_ms) {
    running_ = true;
    start_time_ms_ = now_ms;
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

    // 更新计时器时间
    start_time_ms_ += ms_since_last_tick;
}

bool RetransmissionTimer::is_expired(uint64_t now_ms) const {
    return running_ && (now_ms - start_time_ms_ >= current_RTO_ms_);
}

void RetransmissionTimer::double_RTO() {
    current_RTO_ms_ *= 2;
}

void RetransmissionTimer::increment_retransmissions() {
    consecutive_retransmissions_++;
}

uint64_t RetransmissionTimer::consecutive_retransmissions() const {
    return consecutive_retransmissions_;
}

bool RetransmissionTimer::is_running() const {
    return running_;
}