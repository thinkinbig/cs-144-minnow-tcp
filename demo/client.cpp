// Simple line-driven echo client.
//
// Refactored to use the project's TCPSocket/Address utilities instead of raw
// POSIX sockets. Reads a line from stdin, sends it as an Echo message, and
// prints the server's reply.

#include "address.hh"
#include "helper.hh"
#include "socket.hh"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void send_message( TCPSocket& sock, const Message& msg )
{
  std::string framed = encode_message( msg );
  while ( !framed.empty() ) {
    const size_t n = sock.write( framed );
    if ( n == 0 ) {
      throw std::runtime_error( "write returned 0 on blocking socket" );
    }
    framed.erase( 0, n );
  }
}

bool recv_one_message( TCPSocket& sock, std::string& read_buffer, Message& out )
{
  while ( true ) {
    switch ( try_parse_message( read_buffer, out ) ) {
      case ParseResult::Ok:
        return true;
      case ParseResult::Invalid:
        std::cerr << "[client] malformed frame from server\n";
        return false;
      case ParseResult::NeedMore:
        break;
    }
    std::string chunk;
    sock.read( chunk );
    if ( chunk.empty() ) {
      return false; // EOF
    }
    read_buffer.append( chunk );
  }
}

} // namespace

int main()
{
  TCPSocket sock;
  sock.connect( Address { "127.0.0.1", 8080 } );
  std::cerr << "[client] connected to " << sock.peer_address().to_string() << "\n";

  std::string read_buffer;
  std::string line;
  while ( std::getline( std::cin, line ) ) {
    if ( line.empty() ) {
      break;
    }

    Message out { line == "quit" ? MessageType::Quit : MessageType::Echo, line };
    send_message( sock, out );

    Message reply;
    if ( !recv_one_message( sock, read_buffer, reply ) ) {
      std::cerr << "[client] server closed connection\n";
      break;
    }
    std::cout << "[client] reply: " << reply.body << "\n";

    if ( out.type == MessageType::Quit ) {
      break;
    }
  }
}
