#pragma once

#include "reassembler.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"

#include <optional>

// The receiving half of a TCP connection. Translates incoming TCPSenderMessages
// into stream-relative offsets and feeds them to the Reassembler; produces ACKs
// and window advertisements for the peer's TCPSender.
class TCPReceiver
{
public:
  explicit TCPReceiver( Reassembler&& reassembler ) : reassembler_( std::move( reassembler ) ) {}

  void receive( TCPSenderMessage message );
  TCPReceiverMessage send() const;

  const Reassembler& reassembler() const { return reassembler_; }
  Reader& reader() { return reassembler_.reader(); }
  const Reader& reader() const { return reassembler_.reader(); }
  const Writer& writer() const { return reassembler_.writer(); }

private:
  Reassembler reassembler_;

  // Set on the first SYN; remembers the peer's ISN so we can convert wrapping
  // seqnos to absolute stream indices.
  std::optional<Wrap32> isn_ {};
};
