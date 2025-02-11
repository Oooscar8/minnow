#pragma once

#include "byte_stream.hh"
#include <map>

class Reassembler
{
  friend class TCPReceiver;
  
public:
  // Construct Reassembler to write into given ByteStream.
  explicit Reassembler( ByteStream&& output ) : output_( std::move( output ) ), capacity_( writer().available_capacity() ) {}

  /*
   * Insert a new substring to be reassembled into a ByteStream.
   *   `first_index`: the index of the first byte of the substring
   *   `data`: the substring itself
   *   `is_last_substring`: this substring represents the end of the stream
   *   `output`: a mutable reference to the Writer
   *
   * The Reassembler's job is to reassemble the indexed substrings (possibly out-of-order
   * and possibly overlapping) back into the original ByteStream. As soon as the Reassembler
   * learns the next byte in the stream, it should write it to the output.
   *
   * If the Reassembler learns about bytes that fit within the stream's available capacity
   * but can't yet be written (because earlier bytes remain unknown), it should store them
   * internally until the gaps are filled in.
   *
   * The Reassembler should discard any bytes that lie beyond the stream's available capacity
   * (i.e., bytes that couldn't be written even if earlier gaps get filled in).
   *
   * The Reassembler should close the stream after writing the last byte.
   */
  void insert( uint64_t first_index, std::string data, bool is_last_substring );

  // How many bytes are stored in the Reassembler itself?
  // This function is for testing only; don't add extra state to support it.
  uint64_t count_bytes_pending() const;

  // Access output stream reader
  Reader& reader() { return output_.reader(); }
  const Reader& reader() const { return output_.reader(); }

  // Access output stream writer, but const-only (can't write from outside)
  const Writer& writer() const { return output_.writer(); }

private:
  ByteStream output_;
  uint64_t capacity_;
  // index of last byte written to the ByteStream
  uint64_t last_byte_ = UINT64_MAX; 
  // out-of-order substrings that buffer in the reassembler's internal storage waiting to be written to the ByteStream
  std::multimap<uint64_t, std::string> unassembled_substrings_ {};
  bool SYN = false;    // Whether the TCP Receiver has received a SYN flag
  bool FIN = false;    // Whether the Reassembler has assembled the last byte of the stream with a FIN flag

  Writer& get_writer() { return output_.writer(); }; // get the writer 

  uint64_t available_capacity() const { return writer().available_capacity(); }; // How many bytes can be buffered in the Reassembler/ByteStream?

  uint64_t next_byte_index() const { return (writer().bytes_pushed() + SYN + FIN); };  // index of next byte to be written to ByteStream

  void merge_substrings(); // merge overlapping/adjacent substrings in the Reassembler's internal storage
};
