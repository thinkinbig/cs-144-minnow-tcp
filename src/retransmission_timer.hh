#pragma once

#include "tcp_sender_message.hh"

#include <functional>

class RetransmissionTimer {
public:
    // 构造函数，接收初始RTO值
    explicit RetransmissionTimer(uint64_t initial_RTO_ms);

    // 启动计时器
    void start(uint64_t now_ms);

    // 停止计时器
    void stop();

    // 重置计时器状态
    void reset();

    // 处理时间流逝
    void tick(uint64_t ms_since_last_tick);

    // 检查是否超时
    bool is_expired(uint64_t now_ms) const;

    // 使RTO翻倍（仅在窗口大小不为0时调用）
    void double_RTO();

    // 增加连续重传次数
    void increment_retransmissions();

    // 获取连续重传次数
    uint64_t consecutive_retransmissions() const;

    // 是否在运行
    bool is_running() const;

private:
    const uint64_t initial_RTO_ms_;    // 初始RTO值
    uint64_t current_RTO_ms_;          // 当前RTO值
    bool running_;                      // 计时器是否在运行
    uint64_t start_time_ms_;           // 计时器启动时间
    uint64_t consecutive_retransmissions_;  // 连续重传次数
};