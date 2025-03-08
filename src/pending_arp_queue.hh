#pragma once

#include "ethernet_frame.hh"
#include "ipv4_datagram.hh"
#include "timer.hh"
#include "address.hh"
#include <queue>
#include <optional>
#include <vector>
#include <memory>
#include <unordered_map>

// ARP 请求超时观察者接口
class ARPRequestTimeoutObserver {
public:
    virtual ~ARPRequestTimeoutObserver() = default;
    virtual void on_arp_request_timeout(const Address& next_hop) = 0;
};

class PendingARPQueue {
public:
    // 等待 ARP 回复的数据报文
    struct PendingDatagram {
        InternetDatagram dgram {};
        Address next_hop { "0.0.0.0", 0 };
        NetworkTimer timer { NetworkTimer::ARP_REQUEST_TIMEOUT };  // 5秒超时
    };

    // 每个IP地址对应的待处理队列
    using PendingQueue = std::vector<PendingDatagram>;

    // 默认构造函数
    PendingARPQueue() = default;

    // 带观察者的构造函数
    explicit PendingARPQueue(std::shared_ptr<ARPRequestTimeoutObserver> observer) 
        : timeout_observer_(observer) {}

    ~PendingARPQueue() = default;

    // 添加待处理的数据报文
    void add_pending(const InternetDatagram& dgram, const Address& next_hop);

    // 获取并移除指定 IP 地址的所有待处理数据报文
    std::vector<PendingDatagram> pop_pending(uint32_t ip_addr);

    // 检查是否有等待特定 IP 的 ARP 回复
    bool has_pending(uint32_t ip_addr) const {
        return pending_.find(ip_addr) != pending_.end();
    }

    // 更新定时器
    void tick(size_t ms_since_last_tick);

    // 获取待处理数据报文数量
    size_t size() const { 
        size_t total = 0;
        for (const auto& [_, queue] : pending_) {
            total += queue.size();
        }
        return total;
    }

    // 检查是否为空
    bool empty() const { return pending_.empty(); }

    // 设置超时观察者
    void set_timeout_observer(std::shared_ptr<ARPRequestTimeoutObserver> observer) {
        timeout_observer_ = observer;
    }

    // 检查是否有观察者
    bool has_observer() const {
        return timeout_observer_ != nullptr;
    }

private:
    std::unordered_map<uint32_t, PendingQueue> pending_ {};
    std::shared_ptr<ARPRequestTimeoutObserver> timeout_observer_ {};
};