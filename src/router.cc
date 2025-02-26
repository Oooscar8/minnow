#include "router.hh"
#include "debug.hh"

#include <iostream>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  // debug( "unimplemented add_route() called" );

  optional<uint32_t> next_hop_ip;

  if ( !next_hop.has_value() ) {
    next_hop_ip = nullopt;
  } else {
    next_hop_ip = next_hop->ipv4_numeric();
  }

  routing_table_.insert( { make_pair( route_prefix, prefix_length ), make_pair( next_hop_ip, interface_num ) } );
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  // debug( "unimplemented route() called" );

  size_t i = 0;
  while ( i < interfaces_.size() ) {
    queue<InternetDatagram>& datagrams_queue = interface( i )->datagrams_received();
    while ( !datagrams_queue.empty() ) {
      InternetDatagram dgram = datagrams_queue.front();
      datagrams_queue.pop();
      if ( dgram.header.ttl <= 1 ) {
        // If the TTL field is already 0, or hits 0 after the decrement, drop the datagram.
        cerr << "DEBUG: Dropped datagram: ttl = " << static_cast<int>( dgram.header.ttl )
             << ", src = " << Address::from_ipv4_numeric( dgram.header.src ).ip()
             << ", dst = " << Address::from_ipv4_numeric( dgram.header.dst ).ip() << "\n";
        continue;
      }
      dgram.header.ttl--;
      dgram.header.compute_checksum();

      uint32_t next_hop_ip;
      size_t interface_num;
      if ( !find_longest_prefix_match( dgram.header.dst, next_hop_ip, interface_num ) ) {
        // If no route is found, drop the datagram.
        cerr << "DEBUG: No route found for datagram: ttl = " << static_cast<int>( dgram.header.ttl )
             << ", src = " << Address::from_ipv4_numeric( dgram.header.src ).ip()
             << ", dst = " << Address::from_ipv4_numeric( dgram.header.dst ).ip() << "\n";
        continue;
      }
      interface( interface_num )->send_datagram( dgram, Address::from_ipv4_numeric( next_hop_ip ) );
      cerr << "DEBUG: Routed datagram: ttl = " << static_cast<int>( dgram.header.ttl )
           << ", src = " << Address::from_ipv4_numeric( dgram.header.src ).ip()
           << ", dst = " << Address::from_ipv4_numeric( dgram.header.dst ).ip()
           << ", to next hop = " << Address::from_ipv4_numeric( next_hop_ip ).ip() << " on interface "
           << interface_num << "\n";
    }
    ++i;
  }
}
