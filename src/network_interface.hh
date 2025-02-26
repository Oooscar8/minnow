#pragma once

#include "address.hh"
#include "ethernet_frame.hh"
#include "ipv4_datagram.hh"
#include "arp_message.hh"

#include <map>
#include <memory>
#include <queue>
#include <unordered_map>

// A "network interface" that connects IP (the internet layer, or network layer)
// with Ethernet (the network access layer, or link layer).

// This module is the lowest layer of a TCP/IP stack
// (connecting IP with the lower-layer network protocol,
// e.g. Ethernet). But the same module is also used repeatedly
// as part of a router: a router generally has many network
// interfaces, and the router's job is to route Internet datagrams
// between the different interfaces.

// The network interface translates datagrams (coming from the
// "customer," e.g. a TCP/IP stack or router) into Ethernet
// frames. To fill in the Ethernet destination address, it looks up
// the Ethernet address of the next IP hop of each datagram, making
// requests with the [Address Resolution Protocol](\ref rfc::rfc826).
// In the opposite direction, the network interface accepts Ethernet
// frames, checks if they are intended for it, and if so, processes
// the the payload depending on its type. If it's an IPv4 datagram,
// the network interface passes it up the stack. If it's an ARP
// request or reply, the network interface processes the frame
// and learns or replies as necessary.
class NetworkInterface
{
public:
  // An abstraction for the physical output port where the NetworkInterface sends Ethernet frames
  class OutputPort
  {
  public:
    virtual void transmit( const NetworkInterface& sender, const EthernetFrame& frame ) = 0;
    virtual ~OutputPort() = default;
  };

  // Construct a network interface with given Ethernet (network-access-layer) and IP (internet-layer)
  // addresses
  NetworkInterface( std::string_view name,
                    std::shared_ptr<OutputPort> port,
                    const EthernetAddress& ethernet_address,
                    const Address& ip_address );

  // Sends an Internet datagram, encapsulated in an Ethernet frame (if it knows the Ethernet destination
  // address). Will need to use [ARP](\ref rfc::rfc826) to look up the Ethernet destination address for the next
  // hop. Sending is accomplished by calling `transmit()` (a member variable) on the frame.
  void send_datagram( const InternetDatagram& dgram, const Address& next_hop );

  // Receives an Ethernet frame and responds appropriately.
  // If type is IPv4, pushes the datagram to the datagrams_in queue.
  // If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
  // If type is ARP reply, learn a mapping from the "sender" fields.
  void recv_frame( EthernetFrame frame );

  // Called periodically when time elapses
  void tick( size_t ms_since_last_tick );

  // Accessors
  const std::string& name() const { return name_; }
  const OutputPort& output() const { return *port_; }
  OutputPort& output() { return *port_; }
  std::queue<InternetDatagram>& datagrams_received() { return datagrams_received_; }

private:
  static constexpr uint64_t MAPPING_TIMEOUT = 30000; // remember the IP-to-Ethernet mapping for 30 seconds

  static constexpr uint64_t ARP_REQUEST_TIMEOUT = 5000; // Same ARP request can only resend after 5 seconds

  // Human-readable name of the interface
  std::string name_;

  // The physical output port (+ a helper function `transmit` that uses it to send an Ethernet frame)
  std::shared_ptr<OutputPort> port_;
  void transmit( const EthernetFrame& frame ) const { port_->transmit( *this, frame ); }

  // Ethernet (known as hardware, network-access-layer, or link-layer) address of the interface
  EthernetAddress ethernet_address_;

  // IP (known as internet-layer or network-layer) address of the interface
  Address ip_address_;

  // Datagrams that have been received
  std::queue<InternetDatagram> datagrams_received_ {};

  struct QueuedDatagram
  {
    uint32_t next_hop_ip {};
    InternetDatagram dgram {};
  };

  // Datagrams that queued to learn the Ethernet address of the next hop
  std::multimap<uint64_t, QueuedDatagram> datagrams_queued_ {};

  uint64_t time_elapsed_ {}; // accumulated time elapsed

  // Define the value type as a pair of EthernetAddress and IP address.
  using MappingPair = std::pair<EthernetAddress, uint32_t>;

  // Define the map type with timestamp as key and (Ethernet, IP) pair as value.
  // This is the data structure that will be used to store all the IP-to-Ethernet mappings.
  using NetworkMappings = std::multimap<uint64_t, MappingPair>;

  NetworkMappings mappings {};

  std::unordered_map<uint32_t, uint64_t> last_arp_request_time_ {};

  // Add a new IP-to-Ethernet mapping
  void addMapping( uint64_t timestamp, const EthernetAddress& eth, uint32_t ip )
  {
    mappings.insert( make_pair( timestamp, MappingPair( eth, ip ) ) );
  }

  // Get mapping by IP address
  bool getMapping( uint32_t target_ip, EthernetAddress& eth ) const
  {
    for ( const auto& [ts, mapping] : mappings ) {
      if ( mapping.second == target_ip ) {
        eth = mapping.first;
        return true;
      }
    }
    return false;
  }

  // Remove expired mappings (older than 30 seconds)
  void remove_expired_mappings()
  {
    if ( time_elapsed_ < MAPPING_TIMEOUT ) {
      return;
    }

    // Calculate the expiration threshold
    uint64_t expiration_threshold = time_elapsed_ - MAPPING_TIMEOUT;

    // Find the first element that is not expired
    // Since map is ordered by timestamp, we can remove all elements
    // from beginning until we find a non-expired entry
    auto it = mappings.lower_bound( expiration_threshold );
    mappings.erase( mappings.begin(), it );
  }

  EthernetFrame create_arp_message( uint16_t opcode,
                                    const EthernetAddress& sender_eth,
                                    uint32_t sender_ip,
                                    const EthernetAddress& target_eth,
                                    uint32_t target_ip )
  {
    EthernetFrame frame;
    if ( opcode == ARPMessage::OPCODE_REQUEST ) {
      frame.header.dst = ETHERNET_BROADCAST;
    } else {
      frame.header.dst = target_eth;
    }
    frame.header.src = sender_eth;
    frame.header.type = EthernetHeader::TYPE_ARP;

    ARPMessage arp;
    arp.opcode = opcode;
    arp.sender_ethernet_address = sender_eth;
    arp.sender_ip_address = sender_ip;
    arp.target_ethernet_address = target_eth;
    arp.target_ip_address = target_ip;

    Serializer serializer;
    arp.serialize( serializer );
    frame.payload = serializer.finish();

    return frame;
  }

  bool can_send_arp_request( uint32_t target_ip ) const
  {
    auto it = last_arp_request_time_.find( target_ip );
    if ( it == last_arp_request_time_.end() ) {
      return true;
    }

    return time_elapsed_ - it->second >= ARP_REQUEST_TIMEOUT;
  }

  void update_arp_request_time( uint32_t target_ip ) { last_arp_request_time_[target_ip] = time_elapsed_; }

  // Drop any expired datagrams
  void drop_expired_datagrams()
  {
    // Calculate the expiration threshold
    uint64_t expiration_threshold = time_elapsed_ - ARP_REQUEST_TIMEOUT;
    if ( time_elapsed_ < ARP_REQUEST_TIMEOUT ) {
      return;
    }

    // Drop all expired datagrams
    auto it = datagrams_queued_.begin();
    while ( it != datagrams_queued_.end() && it->first <= expiration_threshold ) {
      it = datagrams_queued_.erase( it );
    }
  }
};
