/**  
 * Copyright (c) 2009 Carnegie Mellon University. 
 *     All rights reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing,
 *  software distributed under the License is distributed on an "AS
 *  IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 *  express or implied.  See the License for the specific language
 *  governing permissions and limitations under the License.
 *
 * For more about this software visit:
 *
 *      http://www.graphlab.ml.cmu.edu
 *
 */


#ifndef GRAPHLAB_BUFFERED_EXCHANGE_HPP
#define GRAPHLAB_BUFFERED_EXCHANGE_HPP

#include <graphlab/parallel/pthread_tools.hpp>
#include <graphlab/rpc/dc.hpp>
#include <graphlab/rpc/dc_dist_object.hpp>
#include <graphlab/util/mpi_tools.hpp>

namespace graphlab {


  template<typename T>
  class buffered_exchange {
  public:
    typedef std::vector<T> buffer_type;

  private:
    struct buffer_record {
      procid_t proc;
      buffer_type* buffer_ptr;
      buffer_record() : proc(-1), buffer_ptr(NULL) { }
    }; // end of buffer record



    /** The rpc interface for this class */
    mutable dc_dist_object<buffered_exchange> rpc;

    std::vector< buffer_record > recv_buffers;
    mutex recv_lock;

    std::vector< buffer_type > send_buffers;
    std::vector< mutex>        send_locks;
    size_t max_buffer_size;
    
  public:
    buffered_exchange(distributed_control& dc) : 
    rpc(dc, this), send_buffers(dc.numprocs()), send_locks(dc.numprocs()),
    max_buffer_size(1000000) { rpc.barrier(); }
    
    void send(procid_t proc, const T& value) {
      ASSERT_LT(proc, send_locks.size());
      send_locks[proc].lock();
      send_buffers[proc].push_back(value);
      if(send_buffers[proc].size() > max_buffer_size) {
        if(proc == rpc.procid()) {
          rpc_recv(proc, send_buffers[proc]);
        } else {
          rpc.remote_call(proc, &buffered_exchange::rpc_recv,
                          rpc.procid(), send_buffers[proc]);
        }
        send_buffers[proc].clear();
      }
      send_locks[proc].unlock();
    } // end of send


    void flush() {
      for(size_t i = 0; i < send_buffers.size(); ++i) {
        send_locks[i].lock();
        if(i == rpc.procid()) {
          rpc_recv(i, send_buffers[i]);
        } else {
          rpc.remote_call(i, &buffered_exchange::rpc_recv,
                          rpc.procid(), send_buffers[i]);
        }
        send_buffers[i].clear();
        send_locks[i].unlock();
      }
      rpc.full_barrier();
    } // end of flush


    bool recv(procid_t& ret_proc, buffer_type& ret_buffer) {
      bool success = false;
      recv_lock.lock();
      if(!recv_buffers.empty()) {
        success = true;
        buffer_record rec = recv_buffers.back();
        recv_buffers.pop_back();
        recv_lock.unlock();
        ASSERT_NE(rec.buffer_ptr, NULL);
        ASSERT_LT(rec.proc, rpc.numprocs());
        ret_proc = rec.proc;
        ret_buffer.swap(*rec.buffer_ptr);
        delete rec.buffer_ptr; rec.buffer_ptr = NULL;
      } else recv_lock.unlock();
      return success;
    } // end of recv

  private:
    void rpc_recv(procid_t src_proc, buffer_type& buffer) {
      buffer_record rec;
      rec.proc = src_proc; 
      rec.buffer_ptr = new buffer_type();
      ASSERT_NE(rec.buffer_ptr, NULL);
      rec.buffer_ptr->swap(buffer);
      recv_lock.lock();
      recv_buffers.push_back(rec);
      recv_lock.unlock();
    } // end of rpc rcv
    

    


  }; // end of buffered exchange


}; // end of graphlab namespace


#endif

