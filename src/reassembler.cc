#include "reassembler.hh"
#include "debug.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  debug( "unimplemented insert({}, {}, {}) called", first_index, data, is_last_substring );

  if ( data.empty() && is_last_substring && first_index == next_byte_index() ) {
    get_writer().close();
    return;
  }

  if ( is_last_substring ) {
    last_byte_ = first_index + data.length() - 1;
  }

  // At least one byte of data is available to insert
  if ( first_index < next_byte_index() + available_capacity() && first_index + data.length() > next_byte_index() ) {
    uint64_t insert_key = ( first_index >= next_byte_index() ) ? first_index : next_byte_index();

    unassembled_substrings_.insert(
      make_pair( insert_key,
                 data.substr( insert_key - first_index,
                              min( data.length(), next_byte_index() + available_capacity() - insert_key ) ) ) );
    merge_substrings();
    if ( unassembled_substrings_.begin()->first == next_byte_index() ) {
      get_writer().push( unassembled_substrings_.begin()->second );
      if ( unassembled_substrings_.begin()->first + unassembled_substrings_.begin()->second.length() - 1
           == last_byte_ ) {
        get_writer().close();
      }
      unassembled_substrings_.erase( unassembled_substrings_.begin() );
    }
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  debug( "unimplemented count_bytes_pending() called" );

  uint64_t count = 0;
  for ( auto it = unassembled_substrings_.begin(); it != unassembled_substrings_.end(); ++it ) {
    count += it->second.length();
  }
  return count;
}

void Reassembler::merge_substrings()
{
  if ( unassembled_substrings_.size() <= 1 )
    return;

  auto it = unassembled_substrings_.begin();
  auto next = std::next( it );

  while ( next != unassembled_substrings_.end() ) {
    // Overlap or adjacent substrings can be merged
    if ( it->first + it->second.length() >= next->first ) {
      // If next is completely contained in it, erase next directly
      if ( it->first + it->second.length() >= next->first + next->second.length() ) {
        next = unassembled_substrings_.erase( next );
        continue;
      }

      // Otherwise, merge the overlap
      uint64_t overlap_index = it->first + it->second.length() - next->first;
      it->second += next->second.substr( overlap_index );
      next = unassembled_substrings_.erase( next );
    } else {
      ++it;
      ++next;
    }
  }
}
