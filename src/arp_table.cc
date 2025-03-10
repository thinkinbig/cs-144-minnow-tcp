#include "arp_table.hh"
#include <iomanip>

void ARPTable::add_entry(uint32_t ip_addr, const EthernetAddress& eth_addr) {
    auto& entry = entries_[ip_addr];
    entry.eth_addr = eth_addr;
    entry.timer.start();
}

std::optional<EthernetAddress> ARPTable::lookup(uint32_t ip_addr) {
    auto it = entries_.find(ip_addr);
    if (it == entries_.end()) {
        return std::nullopt;
    }

    if (it->second.timer.is_expired()) {
        entries_.erase(it);
        return std::nullopt;
    }

    return it->second.eth_addr;
}

void ARPTable::remove_entry(uint32_t ip_addr) {
    entries_.erase(ip_addr);
}

void ARPTable::tick(size_t ms_since_last_tick) {
    for (auto it = entries_.begin(); it != entries_.end();) {
        auto& entry = it->second;
        entry.timer.tick(ms_since_last_tick);
        
        if (entry.timer.is_expired()) {
            it = entries_.erase(it);
        } else {
            ++it;
        }
    }
}
