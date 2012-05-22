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


/**
 * Also contains code that is Copyright 2011 Yahoo! Inc.  All rights
 * reserved.  
 *
 * Contributed under the iCLA for:
 *    Joseph Gonzalez (jegonzal@yahoo-inc.com) 
 *
 */



#ifndef GRAPHLAB_GRAPHLAB_OPTIONS_HPP
#define GRAPHLAB_GRAPHLAB_OPTIONS_HPP

#include <graphlab/options/options_map.hpp>
 
namespace graphlab {


  /**
   * The engine options class is really a simple struct that contains
   * the basic options needed to create an engine.  These options
   * include:
   
   <ul>

   <li> size_t ncpus: The number of cpus (threads) to use for this
   engine. </li>

   <li> std::string engine_type: The type of engine to use.  Currently
   we support {async,  synchronous}. </li>

   <li> std::string scheduler_type: The type of scheduler to user.
   Currently we support a wide range of schedulers: {synchronous,
   fifo, priority, sampling, splash,  sweep, multiqueue_fifo,
   multiqueue_priority,  set, clustered_priority, round_robin,
   chromatic} </li>

   <li> size_t splash_size: The size parameter for the splash
   scheduler. </li>
   </ul>
   */
  class graphlab_options {
  public:
    //! The number of cpus
    size_t ncpus;
    //! The type of engine {async,  synchronous}
    std::string engine_type;

    //! additional arguments to the engine
    options_map engine_args;
    
    //! The type of scheduler to use
    std::string scheduler_type;

    //! additional arguments to the scheduler
    options_map scheduler_args;

    //! The compiler flags
    std::string compile_flags;

    //! Options for the graph
    options_map graph_options;
    
    //! Use CPU affinities
    bool enable_cpu_affinities;

    bool enable_sched_yield;
 
    bool distributed_options;
    
    graphlab_options() :
      ncpus(2),
      engine_type("async"),
      scheduler_type("fifo"),
      enable_cpu_affinities(false),
      enable_sched_yield(true),
      distributed_options(false){
      // Grab all the compiler flags 
      /* \todo: Add these back at some point
        #ifdef COMPILEFLAGS
        #define QUOTEME_(x) #x
        #define QUOTEME(x) QUOTEME_(x)
        compile_flags = QUOTEME(COMPILEFLAGS);
        #undef QUOTEME
        #undef QUOTEME_
        #endif 
      */
    } // end of constructor




    virtual ~graphlab_options() {}

    //! Use distributed options instead of shared memory options
    void use_distributed_options() {
      scheduler_type = "queued_fifo";
      distributed_options = true;
    }



    //! Set the number of cpus
    void set_ncpus(size_t n) { ncpus = n; }

    //! Get the number of cpus
    size_t get_ncpus() const { return ncpus; }

    //! Set the engine type
    bool set_engine_type(const std::string& etype) {
      //! \todo: ADD CHECKING
      engine_type = engine_args.parse_string(etype);
      return true; 
    }

    //! Get the engine type
    const std::string& get_engine_type() const {
      return engine_type;
    }

    //! Get the engine arguments
    const options_map& get_engine_args() const { 
      return engine_args;
    }


    bool set_scheduler_type(const std::string& stype) {
      //! \todo: ADD CHECKING
      scheduler_type = scheduler_args.parse_string(stype);
      return true; 
    }    


    //! Get the type of scheduler
    const std::string& get_scheduler_type() const {
      return scheduler_type;
    }

    //! Get the scheduler options
    const options_map& get_scheduler_args() const { 
      return scheduler_args;
    }

    //! Get the scheduler options
    options_map& get_scheduler_args() { 
      return scheduler_args;
    }


    bool set_graph_options(const std::string& stype) {
      //! \todo: ADD CHECKING
      graph_options.parse_string(stype);
      return true; 
    }    

    const options_map& get_graph_options() const { 
      return graph_options;
    }

    options_map& get_graph_options() { 
      return graph_options;
    }     

    //! Get the compiler options (flags)
    const std::string& get_compile_flags() const {
      return compile_flags;
    }


    /**
     * Display the current engine options
     */
    virtual void print() const {
      std::cout << "GraphLab Options -------------------\n" 
                << "ncpus:       " << ncpus << "\n"
                << "engine:      " << engine_type << "\n"
                << "scheduler:   " << scheduler_type << "\n";
      std::cout << "\n";
      std::cout << "Scheduler Options: \n";
      std::cout << scheduler_args;
      std::cout << "Additional Engine Options: \n";
      std::cout << engine_args;
      std::cout << std::endl;
    }



  };


  
}
#endif

