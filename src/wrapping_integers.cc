#include "wrapping_integers.hh"
#include "debug.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  debug( "wrap( {}, {} ) called", n, zero_point.raw_value_ );

  return Wrap32( static_cast<uint32_t>( ( zero_point.raw_value_ + n ) % ((uint64_t)UINT32_MAX + 1) ) );
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Your code here.
  debug( "Wrap32( {} ).unwrap( {}, {} ) called", raw_value_, zero_point.raw_value_, checkpoint );

  uint32_t offset = ( raw_value_ >= zero_point.raw_value_ )
                      ? raw_value_ - zero_point.raw_value_
                      : raw_value_ + UINT32_MAX - zero_point.raw_value_ + 1;

  uint64_t seqnos = offset;
  uint64_t difference = UINT64_MAX;
  while ( true ) {
    if ( seqnos >= checkpoint ) {
      uint64_t last_seqno = seqnos - ((uint64_t)UINT32_MAX + 1);
      return ( seqnos - checkpoint < difference ) ? seqnos : last_seqno;
    }
    difference = checkpoint - seqnos;
    seqnos = seqnos + UINT32_MAX + 1;
  }
}
