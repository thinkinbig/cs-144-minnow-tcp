#pragma once

#include "tcp_sender_message.hh"

#include <functional>

class RetransmissionTimer {
public:
    explicit RetransmissionTimer(uint64_t initial_RTO_ms);

    void start(uint64_t now_ms);

    void stop();

    void reset();

    void tick(uint64_t ms_since_last_tick);

    bool is_expired(uint64_t now_ms) const;

    void double_RTO();

    void increment_retransmissions();

    uint64_t consecutive_retransmissions() const;

    bool is_running() const;

private:
    const uint64_t initial_RTO_ms_;
    uint64_t current_RTO_ms_;
    bool running_;
    uint64_t start_time_ms_;
    uint64_t consecutive_retransmissions_;
};