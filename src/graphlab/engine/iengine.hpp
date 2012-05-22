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


/* \file iengine.hpp
   \brief The file containing the iengine description
   
   This file contains the description of the engine interface.  All
   graphlab engines (single_threaded, multi_threaded, distributed, ...)
   should satisfy the functionality described below.
*/

#ifndef GRAPHLAB_IENGINE_HPP
#define GRAPHLAB_IENGINE_HPP


#include <graphlab/scheduler/ischeduler.hpp>
#include <graphlab/vertex_program/icontext.hpp>
#include <graphlab/engine/execution_status.hpp>
#include <graphlab/scheduler/terminator/iterminator.hpp>
#include <graphlab/options/graphlab_options.hpp>




namespace graphlab {
  


  
  /**
     \brief The abstract interface of a GraphLab engine.
     The graphlab engine interface describes the core functionality
     provided by all graphlab engines.  The engine is templatized over
     the type of graph.
     
     The GraphLab engines are a core element of the GraphLab
     framework.  The engines are responsible for applying a the update
     tasks and sync operations to a graph and shared data using the
     scheduler to determine the update schedule. This class provides a
     generic interface to interact with engines written to execute on
     different platforms.
     
     While users are free to directly instantiate the engine of their
     choice we highly recommend the use of the \ref core data
     structure to manage the creation of engines. Alternatively, users
     can use the 
     \ref gl_new_engine "graphlab::engine_factory::new_engine"
     static functions to create
     engines directly from configuration strings.
  */
  template<typename Graph, typename VertexProgram>
  class iengine {
  public:

    //! The type of graph that the engine operates on
    typedef Graph graph_type;
    
    //! The type of the udpate functor
    typedef VertexProgram vertex_program_type;

    //! The generic iupdate functor type
    typedef iupdate_functor<graph_type, vertex_program_type> 
    ivertex_program_type;


    //! The edge list type used by the graph
    typedef typename graph_type::edge_list_type  edge_list_type;

    //! The type of vertex color used by the graph
    typedef typename graph_type::vertex_color_type vertex_color_type;


    //! The type of scheduler
    typedef ischeduler<graph_type, vertex_program_type> ischeduler_type;

    //! The type of context 
    typedef icontext<graph_type, vertex_program_type> icontext_type;

    
    // /**
    //  * The termination function is a function that reads the shared
    //  * data and returns true if the engine should terminate execution.
    //  * The termination function is called at fixed millisecond
    //  * intervals and therefore the engine may continue to execute even
    //  * after a termination function evaluates to true.  Because
    //  * termination functions are executed frequently and cannot
    //  * directly contribut to the computation, they should return
    //  * quickly.
    //  */
    // typedef bool (*termination_function_type) ();
    

    //! Virtual destructor required for inheritance 
    virtual ~iengine() {};
    
    /**
     * \brief Start the engine execution.
     *
     * This \b blocking function starts the engine and does not
     * return until either one of the termination conditions evaluate
     * true or the scheduler has no tasks remaining.
     */
    virtual void start() = 0;


    /**
     * \brief Force engine to terminate immediately.
     *
     * This function is used to stop the engine execution by forcing
     * immediate termination.  Any existing update tasks will finish
     * but no new update tasks will be started and the call to start()
     * will return.
     */
    virtual void stop() = 0;

    
    /**
     * \brief Describe the reason for termination.
     *
     * Return the reason for the last termination.
     */
    virtual execution_status::status_enum last_exec_status() const = 0;
   
    /**
     * \brief Get the number of updates executed by the engine.
     *
     * This function returns the numbe of updates executed by the last
     * run of this engine.
     * 
     * \return the total number of updates
     */
    virtual size_t last_update_count() const = 0;
           
    /**
     * \brief Send a message to a particular vertex
     */
    virtual void send_message(const vertex_type& vertex,
                              const message_type& message) = 0;


    /**
     * \brief Send a message to all vertices
     */
    virtual void send_message(const message_type& message,
                              const std::string& order = "sequential") = 0;
    
    /**
     *  \brief The timeout is the total
     *  ammount of time in seconds that the engine may run before
     *  exeuction is automatically terminated.
     */
    virtual void set_timeout(size_t timeout_secs) = 0;
    
    /**
     * Get the elapsed time since start was called in milliseconds
     */
    virtual size_t elapsed_time() const = 0;


    /**
     * \brief set a limit on the number of tasks that may be executed.
     * 
     * By once the engine has achived the max_task parameter execution
     * will be terminated. If max_tasks is set to zero then the
     * task_budget is ignored.  If max_tasks is greater than zero than
     * the value of max tasks is used.  Note that if max_task is
     * nonzero the engine encurs the cost of an additional atomic
     * operation in the main loop potentially reducing the overall
     * parallel performance.
     */
    virtual void set_task_budget(size_t max_tasks) = 0;


    /** \brief Update the engine options.  */
    virtual void set_options(const graphlab_options& opts) = 0;

    /** \brief get the current engine options. */
    virtual const graphlab_options& get_options() = 0;



    
    
  };

}

#endif

