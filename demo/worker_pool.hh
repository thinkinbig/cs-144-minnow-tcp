#pragma once

// Fixed-size worker pool driven by a BoundedBlockingQueue<T>.
//
// Each worker pops a T from the queue and runs the configured handler on it.
// Typed on T (not std::function<void()>) so move-only payloads like TCPSocket
// can be dispatched without shared_ptr gymnastics.
//
// Lifecycle:
//   - submit() blocks while the queue is full → natural backpressure
//   - shutdown() / dtor: close the queue, workers drain remaining items, join
//   - exceptions thrown by the handler are caught and logged; workers stay up

#include "bounded_queue.hh"

#include <cstddef>
#include <exception>
#include <functional>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>

template<typename T>
class WorkerPool
{
public:
  WorkerPool( size_t worker_count, size_t queue_capacity, std::function<void( T )> handler )
    : queue_( queue_capacity ), handler_( std::move( handler ) ), workers_()
  {
    workers_.reserve( worker_count );
    for ( size_t i = 0; i < worker_count; ++i ) {
      workers_.emplace_back( [this] { run(); } );
    }
  }

  ~WorkerPool() { shutdown(); }

  WorkerPool( const WorkerPool& ) = delete;
  WorkerPool& operator=( const WorkerPool& ) = delete;

  // Hand off an item to the pool. Blocks while the queue is full. Returns
  // false if the pool has been shut down (caller should stop submitting).
  bool submit( T item ) { return queue_.push( std::move( item ) ); }

  // Stop accepting new work, drain in-flight, join all workers. Idempotent.
  void shutdown()
  {
    queue_.close();
    for ( auto& t : workers_ ) {
      if ( t.joinable() ) {
        t.join();
      }
    }
    workers_.clear();
  }

private:
  void run()
  {
    while ( true ) {
      auto item = queue_.pop();
      if ( !item ) {
        return; // queue closed and drained
      }
      try {
        handler_( std::move( *item ) );
      } catch ( const std::exception& e ) {
        std::cerr << "[worker] uncaught exception: " << e.what() << "\n";
      } catch ( ... ) {
        std::cerr << "[worker] uncaught unknown exception\n";
      }
    }
  }

  BoundedBlockingQueue<T> queue_;
  std::function<void( T )> handler_;
  std::vector<std::thread> workers_;
};
