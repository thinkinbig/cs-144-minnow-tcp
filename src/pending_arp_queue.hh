#pragma once

#include "ethernet_frame.hh"
#include "ipv4_datagram.hh"
#include "timer.hh"
#include "address.hh"
#include <queue>
#include <optional>
#include <vector>
#include <memory>

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

    PendingARPQueue() = default;
    ~PendingARPQueue() = default;

    // 添加待处理的数据报文
    void add_pending(const InternetDatagram& dgram, const Address& next_hop);

    // 获取并移除指定 IP 地址的所有待处理数据报文
    std::vector<PendingDatagram> pop_pending(uint32_t ip_addr);

    // 检查是否有等待特定 IP 的 ARP 回复
    bool has_pending(uint32_t ip_addr) const;

    // 更新定时器
    void tick(size_t ms_since_last_tick);

    // 获取待处理数据报文数量
    size_t size() const { return pending_.size(); }

    // 检查是否为空
    bool empty() const { return pending_.empty(); }

    // 设置超时观察者
    void set_timeout_observer(std::weak_ptr<ARPRequestTimeoutObserver> observer) {
        timeout_observer_ = observer;
    }

private:
    std::vector<PendingDatagram> pending_ {};
    std::weak_ptr<ARPRequestTimeoutObserver> timeout_observer_ {};
}; 