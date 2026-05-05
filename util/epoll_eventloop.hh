#pragma once

#ifdef __linux__

#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <unordered_map>

#include "file_descriptor.hh"

//! Linux-only epoll(7) backed counterpart to EventLoop.
//!
//! Public API mirrors EventLoop so callers can swap by changing the type. See
//! eventloop.hh for the original poll(2) implementation that this file does
//! NOT replace.
class EpollEventLoop
{
public:
  enum class Direction : bool
  {
    In,
    Out
  };

private:
  using CallbackT = std::function<void( void )>;
  using InterestT = std::function<bool( void )>;

  struct RuleCategory
  {
    std::string name;
  };

  struct BasicRule
  {
    size_t category_id;
    InterestT interest;
    CallbackT callback;
    bool cancel_requested {};

    BasicRule( size_t s_category_id, InterestT s_interest, CallbackT s_callback );
  };

  struct FDRule : public BasicRule
  {
    FileDescriptor fd;
    Direction direction;
    CallbackT cancel;
    CallbackT error;

    FDRule( BasicRule&& base, FileDescriptor&& s_fd, Direction s_direction, CallbackT s_cancel, CallbackT s_error );

    unsigned int service_count() const;
  };

  // Per fd state in the epoll set. Multiple FDRules can map to the same fd
  // (e.g. one In + one Out on the same socket via FileDescriptor::duplicate),
  // so the registered events mask is the OR of all interested rules.
  struct FdState
  {
    uint32_t registered_events { 0 }; //!< the mask currently installed in the kernel
    bool in_epoll { false };          //!< whether EPOLL_CTL_ADD has been issued
  };

  std::vector<RuleCategory> _rule_categories {};
  std::list<std::shared_ptr<FDRule>> _fd_rules {};
  std::list<std::shared_ptr<BasicRule>> _non_fd_rules {};

  FileDescriptor _epoll_fd;                    //!< owns the epoll instance
  std::unordered_map<int, FdState> _fd_states; //!< keyed by fd_num

  // Compute the desired epoll event mask for fd_num across all live rules.
  uint32_t desired_events_for_fd( int fd_num ) const;

  // Push the desired mask into the kernel via EPOLL_CTL_ADD/MOD/DEL.
  void sync_fd_registration( int fd_num, uint32_t desired );

public:
  EpollEventLoop();

  enum class Result : uint8_t
  {
    Success,
    Timeout,
    Exit
  };

  size_t add_category( const std::string& name );

  class RuleHandle
  {
    std::weak_ptr<BasicRule> rule_weak_ptr_;

  public:
    template<class RuleType>
    explicit RuleHandle( const std::shared_ptr<RuleType> x ) : rule_weak_ptr_( x )
    {}

    void cancel();
  };

  RuleHandle add_rule(
    size_t category_id,
    FileDescriptor& fd,
    Direction direction,
    const CallbackT& callback,
    const InterestT& interest = [] { return true; },
    const CallbackT& cancel = [] {},
    const CallbackT& error = [] {} );

  RuleHandle
  add_rule( size_t category_id, const CallbackT& callback, const InterestT& interest = [] { return true; } );

  Result wait_next_event( int timeout_ms );

  template<typename... Targs>
  auto add_rule( const std::string& name, Targs&&... Fargs )
  {
    return add_rule( add_category( name ), std::forward<Targs>( Fargs )... );
  }
};

#endif // __linux__
