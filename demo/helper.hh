#pragma once

#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>

inline constexpr uint32_t kMaxMessageSize = 1024 * 1024;

enum class MessageType : uint32_t
{
  Echo = 1,
  Time = 2,
  Quit = 3,
  Error = 999,
};

struct Message
{
  MessageType type {};
  std::string body {};
};

// Outcome of a single try_parse_message call.
//   Ok       -- consumed one complete message, buffer advanced
//   NeedMore -- header or body incomplete; keep reading
//   Invalid  -- header decoded but length > kMaxMessageSize; the stream is
//               unrecoverable and the caller should drop the connection
//               rather than letting the read buffer grow without bound.
enum class ParseResult : uint8_t
{
  Ok,
  NeedMore,
  Invalid,
};

// Per-connection buffers used by the event-loop server. The blocking server
// keeps its own buffers on the stack and doesn't need this struct.
struct ConnBuffers
{
  std::string read_buffer {};
  std::string write_buffer {};
  bool close_after_write { false };
};

inline std::string encode_message( const Message& msg )
{
  std::string out;
  if ( msg.body.size() > kMaxMessageSize ) {
    return out;
  }

  const uint32_t type_net = htonl( static_cast<uint32_t>( msg.type ) );
  const uint32_t len_net = htonl( static_cast<uint32_t>( msg.body.size() ) );

  out.append( reinterpret_cast<const char*>( &type_net ), sizeof( type_net ) );
  out.append( reinterpret_cast<const char*>( &len_net ), sizeof( len_net ) );
  out.append( msg.body );
  return out;
}

inline ParseResult try_parse_message( std::string& buffer, Message& msg )
{
  constexpr size_t header_size = sizeof( uint32_t ) * 2;
  if ( buffer.size() < header_size ) {
    return ParseResult::NeedMore;
  }

  uint32_t type_net = 0;
  uint32_t len_net = 0;
  std::memcpy( &type_net, buffer.data(), sizeof( type_net ) );
  std::memcpy( &len_net, buffer.data() + sizeof( type_net ), sizeof( len_net ) );

  const uint32_t type = ntohl( type_net );
  const uint32_t len = ntohl( len_net );

  if ( len > kMaxMessageSize ) {
    return ParseResult::Invalid;
  }
  if ( buffer.size() < header_size + len ) {
    return ParseResult::NeedMore;
  }

  msg.type = static_cast<MessageType>( type );
  msg.body.assign( buffer.data() + header_size, len );
  buffer.erase( 0, header_size + len );
  return ParseResult::Ok;
}

inline Message handle_message( const Message& msg )
{
  if ( msg.type == MessageType::Echo ) {
    return Message { MessageType::Echo, msg.body };
  }
  if ( msg.type == MessageType::Time ) {
    const std::time_t now = std::time( nullptr );
    return Message { MessageType::Time, std::ctime( &now ) };
  }
  if ( msg.type == MessageType::Quit ) {
    return Message { MessageType::Quit, "bye" };
  }
  return Message { MessageType::Error, "unknown message type" };
}
