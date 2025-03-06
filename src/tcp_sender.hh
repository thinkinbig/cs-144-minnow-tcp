#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include "retransmission_timer.hh"

#include <functional>
#include <deque>

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) )
    , isn_( isn )
    , initial_RTO_ms_( initial_RTO_ms )
    , window_size_( 1 )
    , next_seqno_( 0 )
    , bytes_in_flight_( 0 )
    , timer_( initial_RTO_ms )
    , outstanding_segments_()
    , syn_sent_( false )
    , fin_sent_( false )
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // For testing: how many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // For testing: how many consecutive retransmissions have happened?
  const Writer& writer() const { return input_.writer(); }
  const Reader& reader() const { return input_.reader(); }
  Writer& writer() { return input_.writer(); }

private:
  ByteStream input_;                              //!< Input byte stream
  Wrap32 isn_;                                   //!< Initial sequence number
  uint64_t initial_RTO_ms_;                      //!< Initial retransmission timeout in milliseconds
  uint64_t window_size_;                         //!< Receiver's advertised window size 
  uint64_t next_seqno_;                          //!< Next sequence number to send
  uint64_t bytes_in_flight_;                     //!< Number of bytes in flight
  RetransmissionTimer timer_;                    //!< Retransmission timer
  std::deque<TCPSenderMessage> outstanding_segments_; //!< Queue of unacknowledged segments
  bool syn_sent_;                                //!< Whether SYN has been sent
  bool fin_sent_;                                //!< Whether FIN has been sent
};
