#pragma once

#include "ethernet_frame.hh"
#include "timer.hh"
#include <unordered_map>
#include <optional>
#include <ostream>

class ARPTable {
public:
    struct ARPEntry {
        EthernetAddress eth_addr {};
        NetworkTimer timer { NetworkTimer::ARP_ENTRY_TIMEOUT };
    };

    ARPTable() = default;
    ~ARPTable() = default;

    // Add or update entry
    void add_entry(uint32_t ip_addr, const EthernetAddress& eth_addr);

    // Remove entry
    void remove_entry(uint32_t ip_addr);

    // Lookup entry (read-while-clear)
    std::optional<EthernetAddress> lookup(uint32_t ip_addr);

    // Update periodically
    void tick(size_t ms_since_last_tick);

    // Get number of entries
    size_t size() const { return entries_.size(); }

    // Check if empty
    bool empty() const { return entries_.empty(); }

    // Iterator support
    auto begin() const { return entries_.begin(); }
    auto end() const { return entries_.end(); }

private:
    std::unordered_map<uint32_t, ARPEntry> entries_ {};
}; 