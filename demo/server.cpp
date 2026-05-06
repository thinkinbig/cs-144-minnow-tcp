// Blocking server backed by a fixed-size worker pool.
//
// Differences from the naive "one thread per connection, detached" version:
//   - Bounded number of worker threads → no DoS by connection flood.
//   - Bounded task queue → backpressure: when both worker and queue are
//     saturated, accept blocks, the kernel's listen backlog absorbs the
//     surge, and further connects get refused at the OS level.
//   - Workers join on pool destruction → no detached threads escaping.

#include "address.hh"
#include "helper.hh"
#include "socket.hh"
#include "worker_pool.hh"

#include <csignal>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {

constexpr size_t kWorkerCount = 64;
constexpr size_t kQueueCapacity = 256;

void serve_client( TCPSocket client )
{
  const std::string peer = client.peer_address().to_string();
  std::cerr << "[server] client connected: " << peer << "\n";

  std::string read_buffer;
  std::string write_buffer;
  bool closing = false;

  try {
    while ( !closing ) {
      std::string chunk;
      client.read( chunk );

      if ( chunk.empty() ) {
        if ( client.eof() ) {
          break;
        }
        continue;
      }
      read_buffer.append( chunk );

      Message msg;
      ParseResult r = ParseResult::NeedMore;
      while ( ( r = try_parse_message( read_buffer, msg ) ) == ParseResult::Ok ) {
        std::cout << "[server] received from " << peer << ": " << msg.body << "\n";
        const Message reply = handle_message( msg );
        write_buffer.append( encode_message( reply ) );
        if ( msg.type == MessageType::Quit ) {
          closing = true;
          break;
        }
      }
      if ( r == ParseResult::Invalid ) {
        throw std::runtime_error( "malformed frame from " + peer );
      }

      while ( !write_buffer.empty() ) {
        const size_t n = client.write( write_buffer );
        if ( n == 0 ) {
          throw std::runtime_error( "write returned 0 on blocking socket" );
        }
        write_buffer.erase( 0, n );
      }
    }
  } catch ( const std::exception& e ) {
    std::cerr << "[server] client " << peer << " error: " << e.what() << "\n";
  }

  std::cerr << "[server] client disconnected: " << peer << "\n";
}

} // namespace

int main()
{
  std::signal( SIGPIPE, SIG_IGN );

  TCPSocket listener;
  listener.set_reuseaddr();
  listener.bind( Address { "0.0.0.0", 8080 } );
  listener.listen();

  std::cerr << "[server] listening on " << listener.local_address().to_string() << " (workers=" << kWorkerCount
            << ", queue=" << kQueueCapacity << ")\n";

  WorkerPool<TCPSocket> pool { kWorkerCount, kQueueCapacity, serve_client };

  while ( true ) {
    TCPSocket client = listener.accept();
    if ( !pool.submit( std::move( client ) ) ) {
      break; // pool has been shut down
    }
  }
}
