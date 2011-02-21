/**
 * This class defines a scheduler that schedules (samples) tasks probabilistically
 * based on task priority.
 **/

#ifndef GRAPHLAB_SAMPLING_SCHEDULER_HPP
#define GRAPHLAB_SAMPLING_SCHEDULER_HPP

#include <cmath>
#include <cassert>

#include <graphlab/graph/graph.hpp>
#include <graphlab/scope/iscope.hpp>
#include <graphlab/tasks/update_task.hpp>
#include <graphlab/schedulers/ischeduler.hpp>
#include <graphlab/parallel/pthread_tools.hpp>
#include <graphlab/schedulers/support/direct_callback.hpp>
#include <graphlab/schedulers/support/vertex_task_set.hpp>
#include <graphlab/util/task_count_termination.hpp>
#include <graphlab/util/fast_multinomial.hpp>


#include <graphlab/macros_def.hpp>
namespace graphlab {
  
  template<typename Graph>
  class sampling_scheduler : 
    public ischeduler<Graph> {
  public:

    typedef Graph graph_type;
    typedef ischeduler<Graph> base;

    typedef typename base::iengine_type iengine_type;
    typedef typename base::update_task_type update_task_type;
    typedef typename base::update_function_type update_function_type;
    typedef typename base::callback_type callback_type;
    typedef typename base::monitor_type monitor_type;


  private:
    using base::monitor;
    
    //! Remember the number of vertices in the graph
    size_t num_vertices;
    
    //! Used to sample vertices quickly
    fast_multinomial multinomial; 
    
    /**
     * Task set tracks the actual task associated with each vertex and
     * their corresponding priorities
     */
    vertex_task_set<Graph> vertex_tasks;

    /** Locks used to ensure consistency in the vertex_tasks and the
        scheduler */
    std::vector<spinlock> locks;
    
    //! The callbacks pre-created for each cpuid
    std::vector< direct_callback<Graph> > callbacks;
    
    //! The terminator is responsible for assessing termination
    task_count_termination terminator;
    
  public:


    sampling_scheduler(iengine_type* engine,
                       Graph& g, 
                       size_t ncpus) :
      num_vertices(g.num_vertices()),
      multinomial(g.num_vertices(), ncpus),
      vertex_tasks(g.num_vertices()),
      locks(g.num_vertices()),
      callbacks(ncpus, direct_callback<Graph>(this, engine) ) { }

    
    callback_type& get_callback(size_t cpuid) {
      return callbacks[cpuid];
    }
    
    
    /** Get the next element in the queue */
    sched_status::status_enum get_next_task(size_t cpuid, update_task_type &ret_task) {
      if(terminator.finish()) return sched_status::COMPLETE;        
      size_t vertex_id = -1;
      // Try and draw a sample (select a vertex)
      while(multinomial.sample(vertex_id, cpuid)) {
        assert(vertex_id < num_vertices);
        double ret_priority = 0.0;
        bool success = false;
        // Grab the lock for that vertex
        locks[vertex_id].lock();
        // Try and grab a task from the task set for that vertex
        if(vertex_tasks.pop(vertex_id, ret_task, ret_priority)) {
          // We actually got a task so update the multinomial
          multinomial.set(vertex_id,
                          vertex_tasks.top_priority(vertex_id));
          // Return that a new task was obtained
          success = true;
        }
        locks[vertex_id].unlock();
        // If we succeeded at getting a task notify the listener and
        // return.
        if(success) {
          // Register the task retreival with the listener
          if (monitor != NULL)
            monitor->scheduler_task_scheduled(ret_task, ret_priority);
          return sched_status::NEWTASK;
        } 
        // else there were no tasks when we got to the vertex and so we
        // must try and sample again
      } // End of while loop

      // If we get to this point then the multinomial is currently empty
      // and we are either finished or waiting for some task to return
      // and update the multinomial
      return sched_status::WAITING;
    } // end of get next task
    
    
    void add_task(update_task_type task, double priority) {
      if(priority <= 0) {
        logger(LOG_WARNING, 
               "You have just added a task with non positive priority "
               "to the multinomial scheduler. This scheduler requires "
               "positive priority tasks. All non positive priority tasks "
               "will be dropped! ");
        return;
      }
      // Grab the lock
      bool newadd = false;
      locks[task.vertex()].lock();  
      // Try and add the task to the vertex tasks
      if (vertex_tasks.add(task, priority)) {
        // If this was a successful (first add) 
        terminator.new_job();  // record an additional job
        newadd = true;
      }
      // Update the multinomial
      multinomial.set(task.vertex(),
                      vertex_tasks.top_priority(task.vertex()));
      // Release the lock
      locks[task.vertex()].unlock();
      // Notify the listener
      if(monitor != NULL) {
        if(newadd) {
          monitor->scheduler_task_added(task, priority);
        } else {
          monitor->scheduler_task_promoted(task, priority, -1);
          monitor->scheduler_task_pruned(task);
        }
      } // end of if listener != NULL
    } // end of add_task
    
    void add_tasks(const std::vector<vertex_id_t> &vertices,
                   update_function_type func,
                   double priority) {
      foreach(vertex_id_t vertex, vertices) {
        add_task(update_task_type(vertex, func), priority);
      }
    } // end of add_tasks
    
    void add_task_to_all(update_function_type func, 
                         double priority) {
      for (vertex_id_t vertex = 0; vertex < num_vertices; ++vertex){
        add_task(update_task_type(vertex, func), priority);
      }
    } // add_task_to_all
    

    inline void update_state(size_t cpuid,
                             const std::vector<vertex_id_t>& updated_vertices,
                             const std::vector<edge_id_t>& updatededges) { }


    void completed_task(size_t cpuid, const update_task_type& task) {
      terminator.completed_job();
    }
    
    void scoped_modifications(size_t cpuid, vertex_id_t rootvertex,
                              const std::vector<edge_id_t>& updatededges){}    
    
    void abort() { terminator.abort(); }
  
    void restart() { terminator.restart(); }
    
  }; // end of sampling_scheduler
  

} // end of namespace graphlab
#include <graphlab/macros_undef.hpp>

#endif
