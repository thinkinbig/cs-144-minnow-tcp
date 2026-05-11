#pragma once

#include "protocol.hh"

#include <ctime>
#include <string>

// Per-connection buffers used by the event-loop server. The blocking server
// keeps its own buffers on the stack and doesn't need this struct.
struct ConnBuffers
{
  std::string read_buffer {};
  std::string write_buffer {};
  bool close_after_write { false };
};

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
