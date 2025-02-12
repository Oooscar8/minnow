#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // debug( "unimplemented sequence_numbers_in_flight() called" );

  uint64_t count = 0;
  auto it = outstanding_segments_.begin();
  while ( it != outstanding_segments_.end() ) {
    count += it->second.sequence_length();
    ++it;
  }
  return count;
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  // debug( "unimplemented consecutive_retransmissions() called" );

  return consecutive_retransmissions_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  // debug( "unimplemented push() called" );

  TCPSenderMessage msg;

  size_t payload_size
    = min( min( (uint64_t)TCPConfig::MAX_PAYLOAD_SIZE, sender_window_size_ ), reader().bytes_buffered() );
  msg.payload = reader().peek().substr( 0, payload_size );
  if ( next_seqno_ == 0 ) {
    msg.SYN = true;
  }
  msg.seqno = Wrap32::wrap( next_seqno_, isn_ );
  reader().pop( payload_size );
  if ( reader().is_finished() ) {
    msg.FIN = true;
  }
  if ( input_.has_error() ) {
    msg.RST = true;
  }

  // Add the segment to the outstanding segments map and update the next sequence number.
  outstanding_segments_[next_seqno_] = msg;
  next_seqno_ = reader().bytes_popped();

  transmit( msg );
  if ( msg.sequence_length() > 0 && !timer_.is_running() ) {
    timer_.start();
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  // debug( "unimplemented make_empty_message() called" );

  TCPSenderMessage msg;

  msg.seqno = Wrap32::wrap( next_seqno_, isn_ );
  if ( input_.has_error() ) {
    msg.RST = true;
  }

  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // debug( "unimplemented receive() called" );

  // Update the sender's and receiver's window even if we have received a duplicate ACK.
  if ( msg.ackno->unwrap( isn_, last_ackno_ ) == last_ackno_ ) {
    receiver_window_size_ = msg.window_size;
    rwindow_ = last_ackno_ + msg.window_size - 1;
    sender_window_size_ = rwindow_ - next_seqno_ + 1;
    return;
  }

  // If we get to this point, it means we have received a new ACK message
  last_ackno_ = msg.ackno->unwrap( isn_, last_ackno_ );
  receiver_window_size_ = msg.window_size;
  rwindow_ = last_ackno_ + msg.window_size - 1;
  sender_window_size_ = rwindow_ - next_seqno_ + 1;

  // Remove any segments that have been acknowledged from the outstanding segments map.
  auto it = outstanding_segments_.begin();
  while ( it != outstanding_segments_.end() ) {
    if ( it->second.seqno.unwrap( isn_, last_ackno_ ) + it->second.sequence_length() <= last_ackno_ ) {
      // This segment has been acknowledged.
      it = outstanding_segments_.erase( it );
    } else {
      ++it;
    }
  }

  /*
   * When the receiver gives the sender a new `ack` message:
   * 1. Set the RTO back to its initial value.
   * 2. If the sender has any outstanding data, restart the retransmission timer. Otherwise, stop the timer.
   * 3. Reset the consecutive retransmissions back to zero.
   */
  timer_.reset_RTO();
  if ( !outstanding_segments_.empty() ) {
    timer_.start();
  } else {
    timer_.stop();
  }
  consecutive_retransmissions_ = 0;
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // debug( "unimplemented tick({}, ...) called", ms_since_last_tick );

  if ( !timer_.is_running() )
    return;

  timer_.time_elapsed( ms_since_last_tick );

  if ( timer_.expired() ) {
    // Retransmit the earliest outstanding segment.
    transmit( outstanding_segments_.begin()->second );

    // If the receiver's window size is nonzero, increment the number of consecutive retransmissions and double RTO.
    if ( receiver_window_size_ != 0 ) {
      ++consecutive_retransmissions_;
      timer_.double_RTO();
    }

    // Start the timer again.
    timer_.start();
  }
}
