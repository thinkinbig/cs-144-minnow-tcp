#pragma once

#include "ref.hh"
#include <algorithm>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>

template<typename T>
class GeneralObjectPool {
public:
    using Object = T;
    using Creator = std::function<Object()>;
    using Resetter = std::function<void(Object&)>;

    // 构造函数
    explicit GeneralObjectPool(Creator creator, 
                             Resetter resetter = [](Object&) {},
                             size_t initial_size = 0,
                             size_t max_size = std::numeric_limits<size_t>::max())
        : max_size_(max_size)
        , pool_()
        , creator_(std::move(creator))
        , resetter_(std::move(resetter))
        , mutex_()
    {
        // 预分配对象
        for (size_t i = 0; i < initial_size; ++i) {
            pool_.push_back({creator_(), false});
        }
    }

    // 移动构造函数
    GeneralObjectPool(GeneralObjectPool&& other) noexcept
        : max_size_(other.max_size_)
        , pool_(std::move(other.pool_))
        , creator_(std::move(other.creator_))
        , resetter_(std::move(other.resetter_))
        , mutex_()
    {}

    // 移动赋值运算符
    GeneralObjectPool& operator=(GeneralObjectPool&& other) noexcept {
        if (this != &other) {
            max_size_ = other.max_size_;
            pool_ = std::move(other.pool_);
            creator_ = std::move(other.creator_);
            resetter_ = std::move(other.resetter_);
        }
        return *this;
    }

    // 删除拷贝构造函数和拷贝赋值运算符
    GeneralObjectPool(const GeneralObjectPool&) = delete;
    GeneralObjectPool& operator=(const GeneralObjectPool&) = delete;

    // 借出一个对象
    Ref<Object> borrow() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 尝试找到未借出的对象
        auto it = std::find_if(pool_.begin(), pool_.end(), 
            [](const ObjectWrapper& wrapper) { return !wrapper.is_borrowed; });
        
        if (it != pool_.end()) {
            it->is_borrowed = true;
            return Ref<Object>::borrow(it->obj);
        }

        // 如果没有可用对象且未达到最大数量限制，创建新对象
        if (pool_.size() < max_size_) {
            auto& wrapper = pool_.emplace_back(creator_(), true);
            return Ref<Object>::borrow(wrapper.obj);
        }

        throw std::runtime_error("Object pool is full");
    }

    // 归还对象
    void return_object(const Object& obj) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::find_if(pool_.begin(), pool_.end(), 
            [&obj](const ObjectWrapper& wrapper) { return &wrapper.obj == &obj; });
        
        if (it != pool_.end()) {
            // 重置对象状态
            resetter_(it->obj);
            it->is_borrowed = false;
        } else {
            throw std::runtime_error("Object not found in pool");
        }
    }

    // 获取池中当前对象数量
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.size();
    }

    // 获取当前借出的对象数量
    size_t borrowed_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::count_if(pool_.begin(), pool_.end(), 
            [](const ObjectWrapper& wrapper) { return wrapper.is_borrowed; });
    }

    // 获取当前可用的对象数量
    size_t available_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::count_if(pool_.begin(), pool_.end(), 
            [](const ObjectWrapper& wrapper) { return !wrapper.is_borrowed; });
    }

private:
    struct ObjectWrapper {
        Object obj;
        bool is_borrowed;
    };

    size_t max_size_;                // 最大对象数量限制
    std::deque<ObjectWrapper> pool_;  // 存储对象的容器
    Creator creator_;                 // 对象创建函数
    Resetter resetter_;              // 对象重置函数
    mutable std::mutex mutex_;       // 保护并发访问
}; 