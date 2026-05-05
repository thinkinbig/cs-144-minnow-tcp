#pragma once

// Tiny bounded blocking queue. Single-producer / single-consumer or
// multi-producer / multi-consumer — the mutex covers all the cases.
//
// Move-only T is supported (uses std::queue<T> internally).

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>

template<typename T>
class BoundedBlockingQueue
{
public:
  explicit BoundedBlockingQueue( size_t capacity ) : capacity_( capacity ) {}

  BoundedBlockingQueue( const BoundedBlockingQueue& ) = delete;
  BoundedBlockingQueue& operator=( const BoundedBlockingQueue& ) = delete;

  // Push an item; blocks while the queue is full. Returns false if the queue
  // is closed (no further pushes are accepted).
  bool push( T item )
  {
    std::unique_lock<std::mutex> lock( mu_ );
    not_full_.wait( lock, [&] { return q_.size() < capacity_ || closed_; } );
    if ( closed_ ) {
      return false;
    }
    q_.push( std::move( item ) );
    not_empty_.notify_one();
    return true;
  }

  // Pop an item; blocks while the queue is empty. Returns nullopt only when
  // the queue has been closed AND drained — workers use this as their exit
  // signal.
  std::optional<T> pop()
  {
    std::unique_lock<std::mutex> lock( mu_ );
    not_empty_.wait( lock, [&] { return !q_.empty() || closed_; } );
    if ( q_.empty() ) {
      return std::nullopt;
    }
    T item = std::move( q_.front() );
    q_.pop();
    not_full_.notify_one();
    return item;
  }

  // Close the queue. After this:
  //   - push() returns false immediately
  //   - pop() drains remaining items, then returns nullopt
  // Idempotent.
  void close()
  {
    {
      const std::lock_guard<std::mutex> lock( mu_ );
      closed_ = true;
    }
    not_empty_.notify_all();
    not_full_.notify_all();
  }

private:
  mutable std::mutex mu_ {};
  std::condition_variable not_empty_ {};
  std::condition_variable not_full_ {};
  std::queue<T> q_ {};
  size_t capacity_;
  bool closed_ { false };
};
