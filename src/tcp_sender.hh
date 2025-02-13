#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <functional>
#include <map>

class RetransmissionTimer {
private:
    uint64_t initial_RTO_ms_;       // initial RTO
    uint64_t current_RTO_ms_;       // current RTO
    uint64_t time_elapsed_ {};      // accumulated time elapsed
    bool running_ = false;       // whether timer is running

public:
    explicit RetransmissionTimer(uint64_t initial_RTO_ms) 
        : initial_RTO_ms_(initial_RTO_ms)
        , current_RTO_ms_(initial_RTO_ms) {}
    
    // Start timer
    void start() {
        running_ = true;
        time_elapsed_ = 0;
    }
    
    // Stop timer
    void stop() { 
        running_ = false;
        time_elapsed_ = 0;
    }
    
    // Reset RTO to initial value
    void reset_RTO() {
        current_RTO_ms_ = initial_RTO_ms_;
    }
    
    // Double RTO value
    void double_RTO() {
        current_RTO_ms_ *= 2;
    }
    
    // Examine whether timer has expired
    bool expired() const {
        return running_ && time_elapsed_ >= current_RTO_ms_;
    }

    bool is_running() const {
      return running_;
    }

    void time_elapsed(uint64_t time_ms) {
      time_elapsed_ += time_ms;
    }
};

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms ), timer_(initial_RTO_ms)
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
  Reader& reader() { return input_.reader(); }

  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
  RetransmissionTimer timer_;
  uint64_t next_seqno_ {};    // The next sequence number to be sent
  uint64_t last_sent_seqno_ {};    // The last sequence number sent
  uint64_t last_ackno_ {};    // The last ACK number received, also the left edge of the sender's window
  uint64_t rwindow_ {};    // The right edge of the sender's window
  uint64_t sender_window_size_ = 1;    // The sender's window size
  uint64_t receiver_window_size_ = 1;    // The receiver's window size
  uint64_t consecutive_retransmissions_ {};
  bool SYN {};    // Whether the TCPSender has sent SYN flag
  bool FIN {};    // Whether the TCPSender has sent FIN flag

  std::map<uint64_t, TCPSenderMessage> outstanding_segments_ {};
};