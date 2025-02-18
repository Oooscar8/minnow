#include <iostream>

#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "exception.hh"
#include "helpers.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  debug( "unimplemented send_datagram called" );
  (void)dgram;
  (void)next_hop;

  EthernetFrame eframe;

  // If the destination Ethernet address is already known, create a Ethernet frame and send it right away
  if ( getMapping( next_hop.ipv4_numeric(), eframe.header.dst ) ) {
    // Set up the Ethernet frame header
    eframe.header.type = EthernetHeader::TYPE_IPv4;
    eframe.header.src = ethernet_address_;

    // Set up the Ethernet frame payload to be the serialized datagram
    Serializer serializer;
    dgram.serialize( serializer );
    eframe.payload = serializer.finish();

    // Send the Ethernet frame
    transmit( eframe );
    return;
  }

  // The destination Ethernet address is unknown,
  // broadcast an ARP request and queue the datagram
  EthernetFrame arp_request = create_arp_message( ARPMessage::OPCODE_REQUEST,
                                                  ethernet_address_,
                                                  ip_address_.ipv4_numeric(),
                                                  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
                                                  next_hop.ipv4_numeric() );
  if ( can_send_arp_request( next_hop.ipv4_numeric() ) ) {
    transmit( arp_request );
    update_arp_request_time( next_hop.ipv4_numeric() );
  }
  datagrams_queued_.insert( make_pair( next_hop.ipv4_numeric(), dgram ) );
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  debug( "unimplemented recv_frame called" );
  (void)frame;

  // Ignore any frames not destined for the network interface
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST ) {
    return;
  }

  // If inbound frame is IPv4, parse the payload as an InternetDatagram
  // and if successful, push the resulting datagram to the datagrams_received_ queue
  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    Parser parser( frame.payload );

    InternetDatagram dgram;
    dgram.parse( parser );
    if ( parser.has_error() )
      return;
    datagrams_received_.push( dgram );
  }

  // If the inbound frame is ARP, parse the payload as an ARPMessage
  // an if successful, add the IP-to-Ethernet mapping to the mappings.
  // In addition, if it's an ARP request asking for our IP address,
  // send an appropriate ARP reply.
  if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    Parser parser( frame.payload );

    ARPMessage arp_message;
    arp_message.parse( parser );
    if ( parser.has_error() )
      return;
    addMapping( time_elapsed_, arp_message.sender_ethernet_address, arp_message.sender_ip_address );
    if ( arp_message.opcode == ARPMessage::OPCODE_REQUEST
         && arp_message.target_ip_address == ip_address_.ipv4_numeric() ) {
      EthernetFrame arp_reply = create_arp_message( ARPMessage::OPCODE_REPLY,
                                                    ethernet_address_,
                                                    ip_address_.ipv4_numeric(),
                                                    arp_message.sender_ethernet_address,
                                                    arp_message.sender_ip_address );
      transmit( arp_reply );
    }

    // Since we have received an ARP message, we can probably send any queued datagrams
    auto range = datagrams_queued_.equal_range( arp_message.sender_ip_address );
    for ( auto it = range.first; it != range.second; ) {
      InternetDatagram dgram = it->second;
      send_datagram( dgram, Address::from_ipv4_numeric( it->first ) );
      it = datagrams_queued_.erase( it );
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  debug( "unimplemented tick({}) called", ms_since_last_tick );

  time_elapsed_ += ms_since_last_tick;

  remove_expired_mappings();
}
