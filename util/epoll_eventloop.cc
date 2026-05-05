#ifdef __linux__

#include "epoll_eventloop.hh"
#include "exception.hh"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

unsigned int EpollEventLoop::FDRule::service_count() const
{
  return direction == Direction::In ? fd.read_count() : fd.write_count();
}

EpollEventLoop::BasicRule::BasicRule( size_t s_category_id, InterestT s_interest, CallbackT s_callback )
  : category_id( s_category_id ), interest( move( s_interest ) ), callback( move( s_callback ) )
{}

EpollEventLoop::FDRule::FDRule( BasicRule&& base,
                                FileDescriptor&& s_fd,
                                Direction s_direction,
                                CallbackT s_cancel,
                                CallbackT s_error )
  : BasicRule( move( base ) )
  , fd( move( s_fd ) )
  , direction( s_direction )
  , cancel( move( s_cancel ) )
  , error( move( s_error ) )
{}

static int create_epoll_fd()
{
  const int fd = ::epoll_create1( EPOLL_CLOEXEC );
  if ( fd < 0 ) {
    throw unix_error( "epoll_create1" );
  }
  return fd;
}

EpollEventLoop::EpollEventLoop() : _epoll_fd( create_epoll_fd() ), _fd_states()
{
  _rule_categories.reserve( 64 );
}

size_t EpollEventLoop::add_category( const string& name )
{
  if ( _rule_categories.size() >= _rule_categories.capacity() ) {
    throw runtime_error( "maximum categories reached" );
  }
  _rule_categories.push_back( { name } );
  return _rule_categories.size() - 1;
}

EpollEventLoop::RuleHandle EpollEventLoop::add_rule( size_t category_id,
                                                    FileDescriptor& fd,
                                                    Direction direction,
                                                    const CallbackT& callback,
                                                    const InterestT& interest,
                                                    const CallbackT& cancel,
                                                    const CallbackT& error )
{
  if ( category_id >= _rule_categories.size() ) {
    throw out_of_range( "bad category_id" );
  }

  _fd_rules.emplace_back( make_shared<FDRule>(
    BasicRule { category_id, interest, callback }, fd.duplicate(), direction, cancel, error ) );

  // ensure an FdState exists; actual EPOLL_CTL_ADD happens lazily in
  // wait_next_event() based on interest()
  _fd_states.try_emplace( fd.fd_num() );

  return RuleHandle { _fd_rules.back() };
}

EpollEventLoop::RuleHandle EpollEventLoop::add_rule( const size_t category_id,
                                                    const CallbackT& callback,
                                                    const InterestT& interest )
{
  if ( category_id >= _rule_categories.size() ) {
    throw out_of_range( "bad category_id" );
  }
  _non_fd_rules.emplace_back( make_shared<BasicRule>( category_id, interest, callback ) );
  return RuleHandle { _non_fd_rules.back() };
}

void EpollEventLoop::RuleHandle::cancel()
{
  const shared_ptr<BasicRule> rule_shared_ptr = rule_weak_ptr_.lock();
  if ( rule_shared_ptr ) {
    rule_shared_ptr->cancel_requested = true;
  }
}

uint32_t EpollEventLoop::desired_events_for_fd( int fd_num ) const
{
  uint32_t mask = 0;
  for ( const auto& rule_ptr : _fd_rules ) {
    const auto& rule = *rule_ptr;
    if ( rule.cancel_requested || rule.fd.fd_num() != fd_num ) {
      continue;
    }
    if ( !rule.interest() ) {
      continue;
    }
    mask |= ( rule.direction == Direction::In ? EPOLLIN : EPOLLOUT );
  }
  return mask;
}

void EpollEventLoop::sync_fd_registration( int fd_num, uint32_t desired )
{
  auto it = _fd_states.find( fd_num );
  if ( it == _fd_states.end() ) {
    return;
  }
  FdState& state = it->second;

  if ( !state.in_epoll ) {
    epoll_event ev {};
    ev.events = desired; // even 0 still delivers EPOLLERR/EPOLLHUP
    ev.data.fd = fd_num;
    if ( ::epoll_ctl( _epoll_fd.fd_num(), EPOLL_CTL_ADD, fd_num, &ev ) < 0 ) {
      throw unix_error( "epoll_ctl(ADD)" );
    }
    state.in_epoll = true;
    state.registered_events = desired;
    return;
  }

  if ( state.registered_events == desired ) {
    return;
  }

  epoll_event ev {};
  ev.events = desired;
  ev.data.fd = fd_num;
  if ( ::epoll_ctl( _epoll_fd.fd_num(), EPOLL_CTL_MOD, fd_num, &ev ) < 0 ) {
    throw unix_error( "epoll_ctl(MOD)" );
  }
  state.registered_events = desired;
}

// NOLINTBEGIN(*-cognitive-complexity)
// NOLINTBEGIN(*-signed-bitwise)
EpollEventLoop::Result EpollEventLoop::wait_next_event( const int timeout_ms )
{
  // 1. Non-fd rules: same loop semantics as EventLoop.
  for ( auto it = _non_fd_rules.begin(); it != _non_fd_rules.end(); ) {
    auto& this_rule = **it;
    bool rule_fired = false;

    if ( this_rule.cancel_requested ) {
      it = _non_fd_rules.erase( it );
      continue;
    }

    uint8_t iterations = 0;
    while ( this_rule.interest() ) {
      if ( iterations++ >= 128 ) {
        throw runtime_error( "EpollEventLoop: busy wait detected: rule \""
                             + _rule_categories.at( this_rule.category_id ).name + "\" is still interested after "
                             + to_string( iterations ) + " iterations" );
      }
      rule_fired = true;
      this_rule.callback();
    }

    if ( rule_fired ) {
      return Result::Success;
    }
    ++it;
  }

  // 2. Walk fd rules: drop dead ones, refresh kernel registration to match
  //    current interest()/direction state.
  for ( auto it = _fd_rules.begin(); it != _fd_rules.end(); ) {
    auto& this_rule = **it;
    bool drop = false;

    if ( this_rule.cancel_requested ) {
      drop = true;
    } else if ( this_rule.direction == Direction::In && this_rule.fd.eof() ) {
      this_rule.cancel();
      drop = true;
    } else if ( this_rule.fd.closed() ) {
      this_rule.cancel();
      drop = true;
    }

    if ( drop ) {
      it = _fd_rules.erase( it );
    } else {
      ++it;
    }
  }

  // Recompute and push registrations. Also tear down state for fds that
  // no longer have any rule.
  for ( auto state_it = _fd_states.begin(); state_it != _fd_states.end(); ) {
    const int fd_num = state_it->first;
    bool any_rule = false;
    for ( const auto& rule_ptr : _fd_rules ) {
      if ( rule_ptr->fd.fd_num() == fd_num ) {
        any_rule = true;
        break;
      }
    }

    if ( !any_rule ) {
      if ( state_it->second.in_epoll ) {
        // Best-effort DEL; the fd may already be closed (EBADF) which is fine.
        ::epoll_ctl( _epoll_fd.fd_num(), EPOLL_CTL_DEL, fd_num, nullptr );
      }
      state_it = _fd_states.erase( state_it );
      continue;
    }

    sync_fd_registration( fd_num, desired_events_for_fd( fd_num ) );
    ++state_it;
  }

  if ( _fd_rules.empty() ) {
    return Result::Exit;
  }

  // Are any rules actually interested? If every live rule has interest()
  // == false, mirror EventLoop's "nothing to poll" → Exit semantics.
  bool something_to_poll = false;
  for ( const auto& [_, state] : _fd_states ) {
    if ( state.registered_events != 0 ) {
      something_to_poll = true;
      break;
    }
  }
  if ( !something_to_poll ) {
    return Result::Exit;
  }

  // 3. epoll_wait. Use a modest batch size; we still only service one rule
  //    per iteration, but errors on every returned fd are processed eagerly.
  constexpr int kMaxEvents = 32;
  epoll_event events[kMaxEvents];
  int n = ::epoll_wait( _epoll_fd.fd_num(), events, kMaxEvents, timeout_ms );
  if ( n < 0 ) {
    if ( errno == EINTR ) {
      return Result::Success; // let caller loop again
    }
    throw unix_error( "epoll_wait" );
  }
  if ( n == 0 ) {
    return Result::Timeout;
  }

  // 4. Dispatch. First pass: handle errors / hangups for ALL ready fds so
  //    we drop dead rules even if an earlier rule's callback returns first.
  //    Track which fd we'll dispatch a callback for.
  int callback_fd = -1;
  uint32_t callback_revents = 0;

  for ( int i = 0; i < n; ++i ) {
    const auto& ev = events[i];
    const int fd_num = ev.data.fd;
    const uint32_t revents = ev.events;

    const bool err = ( revents & ( EPOLLERR ) ) != 0;

    if ( err ) {
      // Surface socket error if applicable, then cancel every rule on this fd.
      int socket_error = 0;
      socklen_t optlen = sizeof( socket_error );
      const int ret = ::getsockopt( fd_num, SOL_SOCKET, SO_ERROR, &socket_error, &optlen );
      string category;
      for ( const auto& rule_ptr : _fd_rules ) {
        if ( rule_ptr->fd.fd_num() == fd_num ) {
          category = _rule_categories.at( rule_ptr->category_id ).name;
          break;
        }
      }
      if ( ret == -1 && errno == ENOTSOCK ) {
        cerr << "error on epoll-watched file descriptor for rule \"" << category << "\"\n";
      } else if ( ret == -1 ) {
        throw unix_error( "getsockopt" );
      } else if ( optlen != sizeof( socket_error ) ) {
        throw runtime_error( "unexpected length from getsockopt: " + to_string( optlen ) );
      } else if ( socket_error ) {
        cerr << "error on epoll-watched socket for rule \"" << category << "\": " << strerror( socket_error )
             << "\n";
      }

      for ( auto it = _fd_rules.begin(); it != _fd_rules.end(); ) {
        if ( ( *it )->fd.fd_num() == fd_num ) {
          ( *it )->error();
          ( *it )->cancel();
          it = _fd_rules.erase( it );
        } else {
          ++it;
        }
      }
      auto state_it = _fd_states.find( fd_num );
      if ( state_it != _fd_states.end() ) {
        if ( state_it->second.in_epoll ) {
          ::epoll_ctl( _epoll_fd.fd_num(), EPOLL_CTL_DEL, fd_num, nullptr );
        }
        _fd_states.erase( state_it );
      }
      continue;
    }

    if ( callback_fd == -1 ) {
      callback_fd = fd_num;
      callback_revents = revents;
    }
  }

  if ( callback_fd == -1 ) {
    return Result::Success;
  }

  // 5. Find the first live rule on callback_fd whose direction matches a
  //    bit in revents and which is still interested.
  const bool readable = ( callback_revents & EPOLLIN ) != 0;
  const bool writable = ( callback_revents & EPOLLOUT ) != 0;
  const bool hup = ( callback_revents & EPOLLHUP ) != 0;

  for ( auto it = _fd_rules.begin(); it != _fd_rules.end(); ) {
    auto& this_rule = **it;
    if ( this_rule.fd.fd_num() != callback_fd ) {
      ++it;
      continue;
    }

    const bool dir_in = this_rule.direction == Direction::In;
    const bool ready = ( dir_in && readable ) || ( !dir_in && writable );

    // Mirror EventLoop's defunct-fd treatment: pure HUP with nothing to read,
    // or HUP on an Out direction, means the fd will never make progress.
    if ( hup && ( ( !ready && dir_in ) || !dir_in ) ) {
      this_rule.cancel();
      it = _fd_rules.erase( it );
      continue;
    }

    if ( ready && this_rule.interest() ) {
      const auto count_before = this_rule.service_count();
      this_rule.callback();

      if ( count_before == this_rule.service_count() && !this_rule.fd.closed() && this_rule.interest() ) {
        throw runtime_error( "EpollEventLoop: busy wait detected: rule \""
                             + _rule_categories.at( this_rule.category_id ).name
                             + "\" did not read/write fd and is still interested" );
      }
      return Result::Success;
    }

    ++it;
  }

  return Result::Success;
}
// NOLINTEND(*-signed-bitwise)
// NOLINTEND(*-cognitive-complexity)

#endif // __linux__
