#include "pending_arp_queue.hh"
#include <algorithm>

void PendingARPQueue::add_pending(const InternetDatagram& dgram, const Address& next_hop) {
    PendingDatagram pending;
    pending.dgram = dgram;
    pending.next_hop = next_hop;
    pending.timer.start();  // 启动 5 秒计时器
    pending_.push_back(std::move(pending));
}

std::vector<PendingARPQueue::PendingDatagram> PendingARPQueue::pop_pending(uint32_t ip_addr) {
    std::vector<PendingDatagram> result;
    auto new_end = std::remove_if(pending_.begin(), pending_.end(),
        [&](const PendingDatagram& p) {
            if (p.next_hop.ipv4_numeric() == ip_addr) {
                result.push_back(p);
                return true;
            }
            return false;
        });
    pending_.erase(new_end, pending_.end());
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
    for (auto& p : pending_) {
        p.timer.tick(ms_since_last_tick);
        if (p.timer.is_expired()) {
            if (auto observer = timeout_observer_.lock()) {
                observer->on_arp_request_timeout(p.next_hop);
            }
            p.timer.start();  // 重置定时器
        }
    }
}