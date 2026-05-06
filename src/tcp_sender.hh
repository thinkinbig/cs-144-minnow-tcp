#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include "timer.hh"

#include <deque>
#include <functional>

// Drives the sending half of a TCP connection: turns the outbound ByteStream
// into a sequence of TCPSenderMessages, retransmits on timeout, and consumes
// ACKs / window updates from the peer.
class TCPSender
{
public:
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), timer_( initial_RTO_ms )
  {}

  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  // Emit segments until either the input is drained or the send window is full.
  void push( const TransmitFunction& transmit );

  // Advance time; retransmit the oldest unacked segment if the RTO has elapsed.
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Process an ACK / window-update / RST from the peer.
  void receive( const TCPReceiverMessage& msg );

  // A "blank" segment carrying just the next seqno and the RST flag if errored.
  // Test harnesses use this to probe seqno; tick() also uses it for empty FINs.
  TCPSenderMessage make_empty_message() const;

  // Test-only accessors.
  uint64_t sequence_numbers_in_flight() const { return bytes_in_flight_; }
  uint64_t consecutive_retransmissions() const { return timer_.consecutive_retransmissions(); }

  const Writer& writer() const { return input_.writer(); }
  const Reader& reader() const { return input_.reader(); }
  Writer& writer() { return input_.writer(); }

private:
  ByteStream input_;
  Wrap32 isn_;
  RetransmissionTimer timer_;

  // Receiver-advertised window. A zero window is treated as 1 for probing
  // (RFC 793 §3.7) — but retransmissions don't bump backoff in that case.
  uint16_t window_size_ { 1 };
  bool zero_window_ {};

  uint64_t next_seqno_ {};      // absolute seqno of the next byte to send
  uint64_t bytes_in_flight_ {}; // sum of sequence_length() over outstanding_
  bool syn_sent_ {};
  bool fin_sent_ {};

  std::deque<TCPSenderMessage> outstanding_ {};

  // Build and transmit the next segment starting at next_seqno_, bounded by
  // `window_remaining` sequence numbers. Returns true iff a segment was sent.
  bool send_segment( const TransmitFunction& transmit, uint64_t window_remaining );
};
