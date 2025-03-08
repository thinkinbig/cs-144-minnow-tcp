#include "pending_arp_queue.hh"
#include <algorithm>

void PendingARPQueue::add_pending(const InternetDatagram& dgram, const Address& next_hop) {
    PendingDatagram pending;
    pending.dgram = dgram;
    pending.next_hop = next_hop;
    pending.timer.start();  // 启动 5 秒计时器
    
    uint32_t ip = next_hop.ipv4_numeric();
    pending_[ip].push_back(std::move(pending));
}

std::vector<PendingARPQueue::PendingDatagram> PendingARPQueue::pop_pending(uint32_t ip_addr) {
    auto it = pending_.find(ip_addr);
    if (it == pending_.end()) {
        return {};
    }
    
    std::vector<PendingDatagram> result = std::move(it->second);
    pending_.erase(it);
    return result;
}

bool PendingARPQueue::has_pending(uint32_t ip_addr) const {
    return std::any_of(pending_.begin(), pending_.end(),
        [ip_addr](const PendingDatagram& p) {
            return p.next_hop.ipv4_numeric() == ip_addr;
        });
}

void PendingARPQueue::tick(size_t ms_since_last_tick) {
    // 更新所有定时器，通知观察者处理超时
    for (auto it = pending_.begin(); it != pending_.end();) {
        bool any_expired = false;
        auto& queue = it->second;
        
        // 更新队列中所有数据报的定时器
        for (auto& pending : queue) {
            pending.timer.tick(ms_since_last_tick);
            if (pending.timer.is_expired()) {
                any_expired = true;
            }
        }
        
        // 如果有任何一个数据报超时，通知观察者并重置所有定时器
        if (any_expired && timeout_observer_) {
            timeout_observer_->on_arp_request_timeout(queue[0].next_hop);
            for (auto& pending : queue) {
                pending.timer.start();
            }
            ++it;
        } else if (any_expired) {
            // 如果没有观察者，移除整个队列
            it = pending_.erase(it);
        } else {
            ++it;
        }
    }
}