#include "arp_message_queue.hh"
#include <algorithm>
#include <vector>

using namespace std;

void ARPMessageQueue::add_pending(const InternetDatagram& dgram, const Address& next_hop) {
    uint32_t ip = next_hop.ipv4_numeric();
    
    // Add to queue
    PendingDatagram pending;
    pending.dgram = dgram;
    pending.next_hop = next_hop;
    pending.timer.start();
    pending_[ip].push_back(move(pending));
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
    // Iterate through all pending queues
    for (auto it = pending_.begin(); it != pending_.end();) {
        // Update the timer of the first datagram in the queue
        // If the first one is expired, the whole queue is expired
        if (!it->second.empty()) {
            it->second.front().timer.tick(ms_since_last_tick);
            if (it->second.front().timer.is_expired()) {
                // Send a new ARP request and remove the queue
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