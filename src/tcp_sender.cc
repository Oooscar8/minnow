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

  while ( ( !FIN && sender_window_size_ > 0 ) || zero_windowsize_received_ ) {
    if ( FIN )
      return;

    TCPSenderMessage msg;

    if ( next_seqno_ == 0 ) {
      msg.SYN = true;
      SYN = true;
      FIN = false;
    }

    size_t payload_size;
    uint64_t original_sender_window_size = sender_window_size_;
    if ( zero_windowsize_received_ ) {
      // If the receiver has announced a zero-size window, we should pretend like the window size is one.
      sender_window_size_ = 1;
      payload_size = min( sender_window_size_ - msg.SYN, reader().bytes_buffered() );
    } else {
      payload_size
        = min( min( (uint64_t)TCPConfig::MAX_PAYLOAD_SIZE, reader().bytes_buffered() ), sender_window_size_ - msg.SYN );
    }

    msg.payload = reader().peek().substr( 0, payload_size );
    msg.seqno = Wrap32::wrap( next_seqno_, isn_ );
    if ( writer().is_closed() ) {
      last_sent_seqno_ = SYN + reader().bytes_popped() + reader().bytes_buffered() - 1;
      if ( next_seqno_ + msg.sequence_length() - 1 >= last_sent_seqno_ ) {
        // This is the segment containing the last byte of the outbound stream

        // Don't add the FIN flag if it would make the segment exceed the sender's window
        if ( msg.sequence_length() < sender_window_size_ || ( msg.SYN == true && sender_window_size_ == 1 ) ) {
          msg.FIN = true;
          FIN = true;
        }
      }
    }
    if ( input_.has_error() ) {
      msg.RST = true;
    }

    if ( msg.sequence_length() == 0 ) {
      return;
    }

    // debug( "pushed message with seqno = {}, SYN = {}, FIN = {}, RST = {}, sequence length = {}",
    //        next_seqno_,
    //        msg.SYN,
    //        msg.FIN,
    //        msg.RST,
    //        msg.sequence_length() );

    // Add the segment to the outstanding segments map and update the next sequence number.
    outstanding_segments_[next_seqno_] = msg;
    reader().pop( payload_size );
    next_seqno_ = reader().bytes_popped() + SYN + FIN;
    sender_window_size_ = rwindow_ - next_seqno_ + 1;

    transmit( msg );
    if ( msg.sequence_length() > 0 && !timer_.is_running() ) {
      timer_.start();
    }

    if ( zero_windowsize_received_ ) {
      zero_windowsize_received_ = false;
      sender_window_size_ = original_sender_window_size;
      return;
    }
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

  // debug( "made empty message with seqno = {}, RST = {}", next_seqno_, msg.RST );

  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // debug( "unimplemented receive() called" );

  // debug( "received message with ackno = {}, window size = {}, RST = {}",
  //        msg.ackno->unwrap( isn_, last_ackno_ ),
  //        msg.window_size,
  //        msg.RST );

  if ( msg.RST ) {
    input_.set_error();
  }

  // Ignore impossible ackno (beyond next seqno).
  if ( msg.ackno && msg.ackno->unwrap( isn_, last_ackno_ ) > next_seqno_ ) {
    return;
  }

  // Update the sender's and receiver's window even if we have received a duplicate ACK or a null ACK.
  if ( msg.ackno->unwrap( isn_, last_ackno_ ) == last_ackno_ || !msg.ackno ) {
    receiver_window_size_ = msg.window_size;
    if ( receiver_window_size_ == 0 ) {
      zero_windowsize_received_ = true;
    }
    rwindow_ = last_ackno_ + msg.window_size - 1;
    sender_window_size_ = rwindow_ - next_seqno_ + 1;
    return;
  }

  // If we get to this point, it means we have received a new ACK message.
  last_ackno_ = msg.ackno->unwrap( isn_, last_ackno_ );
  receiver_window_size_ = msg.window_size;
  if ( receiver_window_size_ == 0 ) {
    zero_windowsize_received_ = true;
  }
  rwindow_ = last_ackno_ + msg.window_size - 1;
  sender_window_size_ = rwindow_ - next_seqno_ + 1;

  // Remove any segments that have been acknowledged from the outstanding segments map.
  auto it = outstanding_segments_.begin();
  while ( it != outstanding_segments_.end() ) {
    if ( it->first + it->second.sequence_length() <= last_ackno_ ) {
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
