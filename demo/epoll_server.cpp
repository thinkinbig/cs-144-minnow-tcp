// Non-blocking server driven by EpollEventLoop.
//
// Refactored from the original raw-epoll version to use the project's utility
// library (TCPSocket / Address / FileDescriptor / EpollEventLoop).

#include "address.hh"
#include "epoll_eventloop.hh"
#include "server_handler.hh"
#include "socket.hh"

#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

namespace {

struct Conn
{
  TCPSocket socket;
  ConnBuffers buf {};

  explicit Conn( TCPSocket&& s ) : socket( std::move( s ) ) {}
};

using ConnMap = std::unordered_map<int, std::unique_ptr<Conn>>;

void on_readable( Conn& conn )
{
  // Drain everything the kernel has buffered. FileDescriptor::read returns
  // an empty buffer on EAGAIN (non-blocking) and sets eof() on a clean close.
  while ( true ) {
    std::string chunk;
    conn.socket.read( chunk );
    if ( chunk.empty() ) {
      break;
    }
    conn.buf.read_buffer.append( chunk );
  }

  Message msg;
  ParseResult r = ParseResult::NeedMore;
  while ( ( r = try_parse_message( conn.buf.read_buffer, msg ) ) == ParseResult::Ok ) {
    const Message reply = handle_message( msg );
    conn.buf.write_buffer.append( encode_message( reply ) );
    if ( msg.type == MessageType::Quit ) {
      conn.buf.close_after_write = true;
      break;
    }
  }
  if ( r == ParseResult::Invalid ) {
    std::cerr << "[epoll-server] malformed frame, dropping connection (fd=" << conn.socket.fd_num() << ")\n";
    conn.buf.write_buffer.clear();
    conn.socket.close();
  }
}

void on_writable( Conn& conn )
{
  while ( !conn.buf.write_buffer.empty() ) {
    const size_t n = conn.socket.write( conn.buf.write_buffer );
    if ( n == 0 ) {
      // EAGAIN on a non-blocking socket → try again next time the kernel
      // reports the fd writable.
      return;
    }
    conn.buf.write_buffer.erase( 0, n );
  }

  if ( conn.buf.close_after_write ) {
    conn.socket.close();
  }
}

void register_client( EpollEventLoop& loop, ConnMap& clients, TCPSocket&& client )
{
  client.set_blocking( false );
  const int fd = client.fd_num();
  const std::string peer = client.peer_address().to_string();

  auto [it, inserted] = clients.try_emplace( fd, std::make_unique<Conn>( std::move( client ) ) );
  if ( !inserted ) {
    // fd reuse race; should not happen because the previous socket on this
    // fd is fully closed before the kernel hands it out again.
    return;
  }
  Conn& conn = *it->second;
  std::cerr << "[epoll-server] client connected: " << peer << " (fd=" << fd << ")\n";

  // Read rule: drain & parse. Owns the per-fd cleanup (drops the Conn from
  // the map when the connection ends — by EOF, error, or Quit).
  loop.add_rule(
    "read from client",
    conn.socket,
    EpollEventLoop::Direction::In,
    [&conn] { on_readable( conn ); },
    [] { return true; },
    [&clients, fd, peer] {
      std::cerr << "[epoll-server] client disconnected: " << peer << " (fd=" << fd << ")\n";
      clients.erase( fd );
    },
    [&conn] { conn.socket.close(); } );

  // Write rule: only interested while we have bytes to flush. The read
  // rule's cancel handler does the cleanup, so this one stays passive.
  loop.add_rule(
    "write to client",
    conn.socket,
    EpollEventLoop::Direction::Out,
    [&conn] { on_writable( conn ); },
    [&conn] { return !conn.buf.write_buffer.empty(); } );
}

} // namespace

int main()
{
  std::signal( SIGPIPE, SIG_IGN );

  EpollEventLoop loop;
  ConnMap clients;

  TCPSocket listener;
  listener.set_reuseaddr();
  listener.bind( Address { "0.0.0.0", 8080 } );
  listener.listen();

  std::cerr << "[epoll-server] listening on " << listener.local_address().to_string()
            << " (non-blocking + EpollEventLoop)\n";

  // Accept rule: accept a single connection per fire. The listener stays in
  // blocking mode — if epoll says it's readable, the next accept() is
  // guaranteed to find a pending connection. If multiple are queued the
  // level-triggered fd will simply fire again on the next iteration.
  loop.add_rule(
    "accept",
    listener,
    EpollEventLoop::Direction::In,
    [&] {
      TCPSocket client = listener.accept();
      // accept() does not bump read_count; mark progress so the eventloop's
      // busy-wait check is satisfied.
      listener.register_read();
      register_client( loop, clients, std::move( client ) );
    } );

  while ( loop.wait_next_event( -1 ) != EpollEventLoop::Result::Exit ) {
  }
}
