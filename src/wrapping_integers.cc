#include "wrapping_integers.hh"
#include "debug.hh"
#include <cmath>
#include <cstdlib>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  // debug( "wrap( {}, {} ) called", n, zero_point.raw_value_ );

  return Wrap32( static_cast<uint32_t>( ( zero_point.raw_value_ + n ) % ( (uint64_t)UINT32_MAX + 1 ) ) );
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Your code here.
  // debug( "Wrap32( {} ).unwrap( {}, {} ) called", raw_value_, zero_point.raw_value_, checkpoint );

  uint32_t offset = ( raw_value_ >= zero_point.raw_value_ ) ? raw_value_ - zero_point.raw_value_
                                                            : raw_value_ + UINT32_MAX - zero_point.raw_value_ + 1;

  uint64_t base_seqno = checkpoint - checkpoint % ( (uint64_t)UINT32_MAX + 1 ) + offset;

  // Check the three adjacent possible sequence numbers
  uint64_t seqno1 = base_seqno;
  uint64_t seqno2 = base_seqno + ( (uint64_t)UINT32_MAX + 1 );
  uint64_t seqno3 = (base_seqno >= ( (uint64_t)UINT32_MAX + 1 )) ? base_seqno - ( (uint64_t)UINT32_MAX + 1 ) : UINT64_MAX;

  // Calculate the absolute differences
  uint64_t diff1 = ( seqno1 >= checkpoint ) ? ( seqno1 - checkpoint ) : ( checkpoint - seqno1 );
  uint64_t diff2 = ( seqno2 >= checkpoint ) ? ( seqno2 - checkpoint ) : ( checkpoint - seqno2 );
  uint64_t diff3 = ( seqno3 >= checkpoint ) ? ( seqno3 - checkpoint ) : ( checkpoint - seqno3 );

  // Return the closest sequence number to the checkpoint
  if ( diff1 <= diff2 && diff1 <= diff3 ) {
    return seqno1;
  } else if ( diff2 <= diff1 && diff2 <= diff3 ) {
    return seqno2;
  } else {
    return seqno3;
  }
}
