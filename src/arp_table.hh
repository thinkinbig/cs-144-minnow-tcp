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

    // 添加或更新表项
    void add_entry(uint32_t ip_addr, const EthernetAddress& eth_addr);

    // 删除表项
    void remove_entry(uint32_t ip_addr);

    // 查找表项（带读时出清）
    std::optional<EthernetAddress> lookup(uint32_t ip_addr);

    // 定时更新
    void tick(size_t ms_since_last_tick);

    // 获取表项数量
    size_t size() const { return entries_.size(); }

    // 检查是否为空
    bool empty() const { return entries_.empty(); }

    // 迭代器支持
    auto begin() const { return entries_.begin(); }
    auto end() const { return entries_.end(); }

private:
    std::unordered_map<uint32_t, ARPEntry> entries_ {};
}; 