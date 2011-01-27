#include <map>
#include <sstream>

#include <boost/unordered_map.hpp>
#include <boost/bind.hpp>

#include <graphlab/util/stl_util.hpp>

#include <graphlab/rpc/dc.hpp>
#include <graphlab/rpc/dc_tcp_comm.hpp>
#include <graphlab/rpc/dc_sctp_comm.hpp>

#include <graphlab/rpc/dc_stream_send.hpp>
#include <graphlab/rpc/dc_stream_receive.hpp>
#include <graphlab/rpc/dc_buffered_stream_send.hpp>
#include <graphlab/rpc/reply_increment_counter.hpp>
#include <graphlab/rpc/dc_services.hpp>

namespace graphlab {

 /**
Callback function. This function is called whenever data is received
*/
void dc_recv_callback(void* tag, procid_t src, const char* buf, size_t len) {
  distributed_control *dc = (distributed_control*)(tag);
  dc->receivers[src]->incoming_data(src, buf, len);
}

distributed_control::~distributed_control() {
  logstream(LOG_INFO) << "Shutting down distributed control " << std::endl;
  comm->close();
  for (size_t i = 0;i < senders.size(); ++i) {
    senders[i]->shutdown();
    delete senders[i];
  }
  for (size_t i = 0;i < receivers.size(); ++i) {
    receivers[i]->shutdown();
    delete receivers[i];
  }
  senders.clear();
  receivers.clear();
  // shutdown function call handlers
  fcallqueue.stop_blocking();
  fcallhandlers.join();
  delete comm;
}
  
void distributed_control::exec_function_call(procid_t source, std::istream &istrm) {
  // extract the dispatch function
  iarchive arc(istrm);
  size_t f; 
  arc >> f;
  // a regular funcion call
  if (f != 0) {
    dc_impl::dispatch_type dispatch = (dc_impl::dispatch_type)f;
    dispatch(*this, source, istrm);
  }
  else {
    // f is NULL!. This is a portable call. deserialize the function name
    std::string s;
    arc >> s;
    char isrequest;
    arc >> isrequest;
    if (isrequest == 0) {
      // std::cout << "portable call to " << s << std::endl;
      // look for the registration
      dc_impl::dispatch_map_type::const_iterator iter = portable_dispatch_call_map.find(s);
      if (iter == portable_dispatch_call_map.end()) {
        logstream(LOG_ERROR) << "Unable to locate dispatcher for function " << s << std::endl;
        return;
      }
      // dispatch
      iter->second(*this, source, istrm);

    }
    else {
     // std::cout << "portable request to " << s << std::endl;
     dc_impl::dispatch_map_type::const_iterator iter = portable_dispatch_request_map.find(s);
      if (iter == portable_dispatch_request_map.end()) {
        logstream(LOG_ERROR) << "Unable to locate dispatcher for function " << s << std::endl;
        return;
      }
      // dispatch
      iter->second(*this, source, istrm);

    }
  }
} 
 
 
void distributed_control::deferred_function_call(procid_t source, char* buf, size_t len) {
  fcallqueue.enqueue(function_call_block(source, buf, len));
}

void distributed_control::fcallhandler_loop() {
  // pop an element off the queue
  while(1) {
    std::pair<function_call_block, bool> entry;
    entry = fcallqueue.dequeue();
    // if the queue is empty and we should quit
    if (entry.second == false) return;
    
    //create a stream containing all the data
    boost::iostreams::stream<boost::iostreams::array_source> 
                                istrm(entry.first.data, entry.first.len);
    exec_function_call(entry.first.source, istrm);
    receivers[entry.first.source]->function_call_completed();
  }
}


std::map<std::string, std::string> distributed_control::parse_options(std::string initstring) {
  std::map<std::string, std::string> options;
  std::replace(initstring.begin(), initstring.end(), ',', ' ');
  std::replace(initstring.begin(), initstring.end(), ';', ' ');        
  std::string opt, value;
  // read till the equal
  std::stringstream s(initstring);
  while(s.good()) {
    getline(s, opt, '=');
    if (s.bad() || s.eof()) break;
    getline(s, value);
    if (s.bad()) break;
    options[trim(opt)] = trim(value);
  }
  return options;
}

void distributed_control::init(const std::vector<std::string> &machines,
            const std::string &initstring,
            procid_t curmachineid,
            size_t numhandlerthreads,
            dc_comm_type commtype) {   
  REGISTER_RPC((*this), reply_increment_counter);
  // parse the initstring
  std::map<std::string,std::string> options = parse_options(initstring);
  bool buffered_send = false;
  if (options["buffered_send"] == "true" || 
    options["buffered_send"] == "1" ||
    options["buffered_send"] == "yes") {
    buffered_send = true;
    std::cerr << "Buffered Send Option is ON." << std::endl;
  }
  
  
  if (commtype == TCP_COMM) {
    comm = new dc_impl::dc_tcp_comm();
    std::cerr << "TCP Communication layer constructed." << std::endl;
  }
  else if (commtype == SCTP_COMM) {
    #ifdef HAS_SCTP
    comm = new dc_impl::dc_sctp_comm();
    std::cerr << "SCTP Communication layer constructed." << std::endl;
    #else
    logger(LOG_FATAL, "SCTP support was not compiled");
    #endif
  }
  else {
    ASSERT_MSG(false, "Unexpected value for comm type");
  }
  // create the receiving objects
  if (comm->capabilities() && dc_impl::COMM_STREAM) {
    for (size_t i = 0; i < machines.size(); ++i) {
      receivers.push_back(new dc_impl::dc_stream_receive(this));
      if (buffered_send) {
        senders.push_back(new dc_impl::dc_buffered_stream_send(this, comm));
      }
      else{
        senders.push_back(new dc_impl::dc_stream_send(this, comm));
      }
    }
  }
  else {
    // TODO
    logstream(LOG_FATAL) << "Datagram handlers not implemented yet" << std::endl;
  }

  // create the handler threads
  // store the threads in the threadgroup
  for (size_t i = 0;i < numhandlerthreads; ++i) {
    launch_in_new_thread(fcallhandlers,
                          boost::bind(&distributed_control::fcallhandler_loop, 
                                      this));
  }

  // start the machines
  comm->init(machines, options, curmachineid, 
            dc_recv_callback, this); 
  
  // set the local proc values
  localprocid = comm->procid();
  localnumprocs = comm->numprocs();
  
  // construct the services
  distributed_services = new dc_services(*this);

}
  
dc_services& distributed_control::services() {
  return *distributed_services;
}


void distributed_control::comm_barrier(procid_t targetmachine) {
  ASSERT_LT(targetmachine, numprocs());
  if (targetmachine != procid()) {
    std::stringstream strm;
    senders[targetmachine]->send_data(targetmachine, BARRIER, strm, 0);
  }
}

void distributed_control::comm_barrier() {
  std::stringstream strm;
  for (size_t i = 0;i < senders.size(); ++i) {
    if (i != procid()) {
      senders[i]->send_data(i, BARRIER, strm, 0);
    }
  }
}

}
