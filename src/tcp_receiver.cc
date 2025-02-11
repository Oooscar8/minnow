#include "tcp_receiver.hh"
#include "debug.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
  // debug( "receive() called, message: seqno={}, payload={}, SYN={}, FIN={}, RST={}",
  //        message.seqno.raw_value_,
  //        message.payload,
  //        message.SYN,
  //        message.FIN,
  //        message.RST );

  if ( message.RST ) {
    reassembler_.output_.set_error();
  }

  if ( message.SYN ) {
    zero_point_ = message.seqno;
    reassembler_.SYN = true;
    reassembler_.FIN = false;
    FIN = false;
  }
  if ( message.FIN ) {
    FIN = true;
  }

  uint64_t first_index = message.seqno.unwrap( zero_point_, reassembler_.next_byte_index() ) + message.SYN;
  reassembler_.insert( first_index, message.payload, message.FIN );

  if ( FIN && reassembler_.writer().is_closed() ) {
    reassembler_.FIN = true;
  }
}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  // debug( "send() called" );

  TCPReceiverMessage message;

  if ( reassembler_.output_.has_error() ) {
    message.RST = true;
  }

  if ( reassembler_.SYN ) {
    uint64_t ackno = reassembler_.next_byte_index();
    message.ackno = Wrap32::wrap( ackno, zero_point_ );
  }
  message.window_size = min( reassembler_.available_capacity(), (uint64_t)UINT16_MAX );

  return message;
}
