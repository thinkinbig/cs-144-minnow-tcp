#include "arp_message_queue.hh"
#include <algorithm>
#include <vector>

using namespace std;

void ARPMessageQueue::add_pending(const InternetDatagram& dgram, const Address& next_hop) {
    uint32_t ip = next_hop.ipv4_numeric();
    
    // 添加到队列
    PendingDatagram pending;
    pending.dgram = dgram;
    pending.next_hop = next_hop;
    pending.timer.start();
    pending_[ip].push_back(std::move(pending));
}

vector<ARPMessageQueue::PendingDatagram> ARPMessageQueue::pop_pending(uint32_t ip_addr) {
    auto it = pending_.find(ip_addr);
    if (it == pending_.end()) {
        return {};
    }
    
    vector<PendingDatagram> result = move(it->second);
    pending_.erase(it);
    return result;
}

bool ARPMessageQueue::has_pending(uint32_t ip_addr) const {
    auto it = pending_.find(ip_addr);
    return it != pending_.end() && !it->second.empty();
}

void ARPMessageQueue::tick(size_t ms_since_last_tick) {
    // 遍历所有待处理队列
    for (auto it = pending_.begin(); it != pending_.end();) {
        // 更新队列中第一个数据包的定时器
        // 如果第一个过期了，整个队列都过期了
        if (!it->second.empty()) {
            it->second.front().timer.tick(ms_since_last_tick);
            if (it->second.front().timer.is_expired()) {
                // 发送新的ARP请求并移除队列
                if (observer_) {
                    observer_->on_arp_request(it->second.front().next_hop);
                }
                it = pending_.erase(it);
                continue;
            }
        }
        ++it;
    }
}