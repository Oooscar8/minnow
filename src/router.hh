#pragma once

#include "exception.hh"
#include "network_interface.hh"

#include <map>
#include <optional>

// \brief A router that has multiple network interfaces and
// performs longest-prefix-match routing between them.
class Router
{
public:
  // Add an interface to the router
  // \param[in] interface an already-constructed network interface
  // \returns The index of the interface after it has been added to the router
  size_t add_interface( std::shared_ptr<NetworkInterface> interface )
  {
    interfaces_.push_back( notnull( "add_interface", std::move( interface ) ) );
    return interfaces_.size() - 1;
  }

  // Access an interface by index
  std::shared_ptr<NetworkInterface> interface( const size_t N ) { return interfaces_.at( N ); }

  // Add a route (a forwarding rule)
  void add_route( uint32_t route_prefix,
                  uint8_t prefix_length,
                  std::optional<Address> next_hop,
                  size_t interface_num );

  // Route packets between the interfaces
  void route();

private:
  // Define a comparison function for the routing table
  struct RouteCompare
  {
    bool operator()( const std::pair<uint32_t, uint8_t>& a, const std::pair<uint32_t, uint8_t>& b ) const
    {
      // Sort by prefix_length in descending order (from largest to smallest)
      if ( a.second != b.second ) {
        return a.second > b.second;
      }
      // If prefix_length is the same, sort by route_prefix in ascending order
      return a.first < b.first;
    }
  };

  // The router's collection of network interfaces
  std::vector<std::shared_ptr<NetworkInterface>> interfaces_ {};
  std::map<std::pair<uint32_t, uint8_t>, std::pair<std::optional<uint32_t>, size_t>, RouteCompare> routing_table_ {};

  bool find_longest_prefix_match( const uint32_t dest_ip, uint32_t& next_hop_ip, size_t& interface_num ) const
  {
    for ( const auto& it : routing_table_ ) {
      if ( is_prefix_match( dest_ip, it.first.first, it.first.second ) ) {
        if ( !it.second.first.has_value() ) {
          next_hop_ip = dest_ip;
        } else {
          next_hop_ip = *it.second.first;
        }
        interface_num = it.second.second;

        return true;
      }
    }

    return false;
  }

  bool is_prefix_match( const uint32_t address, const uint32_t prefix, const uint8_t prefix_length ) const
  {
    // Special case: if prefix_length is 0, all addresses match
    if ( prefix_length == 0 ) {
      return true;
    }

    uint32_t mask = 0xffffffff << ( 32 - prefix_length );

    return ( address & mask ) == ( prefix & mask );
  }
};
