/*
This file is part of GraphLab.

GraphLab is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as 
published by the Free Software Foundation, either version 3 of 
the License, or (at your option) any later version.

GraphLab is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public 
License along with GraphLab.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef DC_STREAM_RECEIVE_Z_HPP
#define DC_STREAM_RECEIVE_Z_HPP
#include <boost/type_traits/is_base_of.hpp>
#include <zlib.h>
#include <graphlab/rpc/circular_char_buffer.hpp>
#include <graphlab/rpc/dc_internal_types.hpp>
#include <graphlab/rpc/dc_types.hpp>
#include <graphlab/rpc/dc_receive.hpp>
#include <graphlab/parallel/atomic.hpp>
#include <graphlab/parallel/pthread_tools.hpp>
#include <graphlab/logger/logger.hpp>

namespace graphlab {
class distributed_control;



namespace dc_impl {

/**
  \ingroup rpc
  ZLib Compressed receiver processor for the dc class.
  The job of the receiver is to take as input a byte stream
  (as received from the socket) and cut it up into meaningful chunks.
  This can be thought of as a receiving end of a multiplexor.
  
  This implements a matching receiver for the ZLib compressed sender
  dc_stream_send_z. ZLib required to compile this.
  
  \see dc_buffered_stream_send_expqueue_z
*/
class dc_stream_receive_z: public dc_receive{
 public:
  
  dc_stream_receive_z(distributed_control* dc): 
                  buffer(10240),
                  barrier(false), dc(dc),
                  bytesreceived(0){ 
    zstrm.zalloc = Z_NULL;
    zstrm.zfree = Z_NULL;
    zstrm.opaque = Z_NULL;
    zstrm.avail_in = 0;
    zstrm.next_in = Z_NULL;
    ASSERT_TRUE(inflateInit(&zstrm) == Z_OK);   
    zbuffer = (char*)malloc(128*1024);
  }

  /**
   Called by the controller when there is data coming
   from the source
  */
  void incoming_data(procid_t src, 
                     const char* buf, 
                     size_t len);
   
  /** called by the controller when a function
  call is completed */
  void function_call_completed(unsigned char packettype) ;
 private:
  /// the mutex protecting the buffer and the barrier 
  mutex bufferlock;
  
  /** the incoming data stream. This is protected
  by the bufferlock */
  circular_char_buffer buffer;

  /** number of rpc calls from this other processor
     which are in the deferred execution queue */
  atomic<size_t> pending_calls;
  
  /** whether a barrier has been issued. 
      this is protected by the bufferlock */
  bool barrier;
  
  /// pointer to the owner
  distributed_control* dc;

  size_t bytesreceived;
  atomic<size_t> compressed_bytesreceived;
  
  /**
    Reads the incoming buffer and processes, dispatching
    calls when enough bytes are received
  */
  void process_buffer(bool outsidelocked) ;

  size_t bytes_received();
  
  void shutdown();

  inline bool direct_access_support() {
    return false;
  }
  
  char* get_buffer(size_t& retbuflength);
  

  char* advance_buffer(char* c, size_t wrotelength, 
                              size_t& retbuflength);
                              
  z_stream zstrm;
  char* zbuffer;
  
};


} // namespace dc_impl
} // namespace graphlab
#endif

