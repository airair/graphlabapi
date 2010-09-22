 
#ifndef GRAPHLAB_SIM_THREAD_ENGINE_HPP
#define GRAPHLAB_SIM_THREAD_ENGINE_HPP

#include <cassert>
#include <cstdio>



#include <graphlab/graph/graph.hpp>
#include <graphlab/scope/iscope.hpp>
#include <graphlab/engine/iengine.hpp>
#include <graphlab/tasks/update_task.hpp>
#include <logger/logger.hpp>
#include <graphlab/parallel/pthread_tools.hpp>
#include <graphlab/parallel/atomic.hpp>
#include <graphlab/util/timer.hpp>
#include <graphlab/util/random.hpp>
#include <graphlab/monitoring/imonitor.hpp>

#include <graphlab/macros_def.hpp>

namespace graphlab {
  
  
  
  
  /**
    * Multi-threaded engine.
   */
  template<typename Graph, typename Scheduler, typename ScopeFactory>
  class sim_thread_engine : 
    public iengine<Graph> {

  public:   
    typedef iengine<Graph> base;
    typedef typename base::update_task_type update_task_type;
    typedef typename base::update_function_type update_function_type;
    typedef typename base::ischeduler_type ischeduler_type;
    typedef typename base::imonitor_type imonitor_type;
    typedef typename base::termination_function_type termination_function_type;
    typedef typename base::iscope_type iscope_type;
    typedef typename base::ishared_data_manager_type ishared_data_manager_type;

  private:
        
    /** Responsible for managing the update of scopes */
    ScopeFactory scope_manager;

    /** Responsible for maintaining the schedule over tasks */
    Scheduler scheduler;

    /** The number of cpus to use */
    size_t ncpus;

    /* Worker bookkeeping */
    std::vector<size_t> task_counts;
    std::vector<size_t> worker_works;
    
    /* Listener */
    imonitor_type* listener;

    ishared_data_manager_type* data_manager;

    Graph& _graph;
    
    timer * _timer;
    
    size_t taskbudget;
    atomic<size_t> taskcount;
 


    double timeout;
    bool aborted;
    bool terminatoraborted;

    
    std::vector<termination_function_type> term_functions;
    float lasttermcheck;
  public:
    /** Initialize the multi threaded engine */
    sim_thread_engine(Graph& g, 
                      size_t num_cpus = thread::cpu_count()) :
      scope_manager(g, num_cpus),
      scheduler(g, num_cpus),
      ncpus(num_cpus),
      task_counts(num_cpus,0),
      worker_works(num_cpus,0),
      listener(NULL),
      data_manager(NULL),
      _graph(g), taskcount(0) {
      scheduler.register_monitor(NULL);
      _timer = NULL;
    
      timeout = 0;
      taskbudget = 0;
      aborted = false;
      terminatoraborted = false;

      assert(num_cpus > 0);
    }

    size_t get_ncpus(){ return ncpus; } 

    void set_default_scope(scope_range::scope_range_enum default_scope_range) {
      scope_manager.set_default_scope(default_scope_range);
    }

    /** register the listener */
    void register_monitor(imonitor_type* _listener) {
      if(_listener == NULL) return;
      this->listener = _listener;
      scheduler.register_monitor(listener);
      listener->init(this);
    }
    

    /**
     * Timeout. Default - no timeout. 
     */
    void set_timeout(size_t timeout_secs) {
      timeout = timeout_secs;
    }
    
    /**
     * Task budget - max number of tasks to allow
     */
    virtual void set_task_budget(size_t max_tasks) {
      taskbudget = max_tasks;
    }

    bool check_all_terminators() {
      if(data_manager != NULL) {        
        for (size_t i = 0;i < term_functions.size();++i) {
          if (term_functions[i](*data_manager)) return true;
        }
        return false;
      } else if (term_functions.size() > 0) {

        logger(LOG_WARNING,
               "Assessing termination without a data_manager!");
      }
      return false;

    }
        
    /**
     * Called by workers. out_of_work flag is passed as a pointer so
     * task's employment status can be updated immediatelly.  This
     * function must be thread safe.
     */
    bool run_next_task(int cpuid) {
      update_task_type task;
      


      while(true) {
        
        /* Check for task budget and timeout (only cpuid 0 checks) */
        if (timeout > 0 && cpuid == 0) {
          if (_timer->current_time() > timeout) {
            aborted = true;
          }
        }
        if (lowres_time_seconds() - lasttermcheck >= 0.1) {
          if (check_all_terminators()) terminatoraborted = true;
          if(data_manager != NULL) data_manager->signal_all();
          lasttermcheck = lowres_time_seconds();
          
        }
        
        if (aborted || terminatoraborted) return false;
        
        // Get the next task along with the status of the scheduler
        sched_status::status_enum stat = scheduler.get_next_task(cpuid, task);
          
        if (stat == sched_status::WAITING) { // If the status is waiting sleep and
          usleep(10);          // then try again
        } else if (stat == sched_status::COMPLETE) {
          /* Scheduler says we terminate */
          return false;
        } else if (stat == sched_status::NEWTASK) {
          const vertex_id_t vertex = task.vertex();
          // There is a new task to schedule 
          // Ensure that a valid task function was passed
          assert(task.function() != NULL);
          // Lock the vertex to ensure that no other processor tries
          // to take it
          // build a scope
          iscope_type* scope = scope_manager.get_scope(cpuid, vertex);
          
          assert(scope != NULL);
          
          // Update task counts and "work". Work is indegree+outdegree          
          task_counts[cpuid] = task_counts[cpuid]++;
          worker_works[cpuid] += scope->in_edge_ids().size() +
            scope->out_edge_ids().size();
          
          // get the callback for this cpu
          typename Scheduler::callback_type& scallback =
            scheduler.get_callback(cpuid);
          

          if (listener != NULL)
            listener->engine_task_execute_start(task, scope, cpuid);

          // execute the task
          // task.execute(*scope, scallback, data_manager);
          assert(task.function() != NULL);
          task.function()(*scope, scallback, data_manager);

          if (listener != NULL)
            listener->engine_task_execute_finished(task, scope, cpuid);      
            
          scope->commit();
          scope_manager.release_scope(scope);


          scheduler.completed_task(cpuid, task);
          
          
          if (taskbudget > 0 && (taskcount.inc() > taskbudget)) {
            aborted = true;
          }
          
          return true;
        } // end of else if (stat == newtask)
      } // end of while(true)


      
    } // end of run_next_task

   
    /** get a reference to the scheduler */
    ischeduler_type& get_scheduler() { return scheduler; }

    void set_shared_data_manager(ishared_data_manager_type* manager) {
      data_manager = manager;      
      if(data_manager != NULL) {
        data_manager->set_scope_factory(&scope_manager);
      }
    }

    void add_terminator(termination_function_type term) {
      term_functions.push_back(term);
    }

    void clear_terminators() {
      term_functions.clear();
    }

    /** Execute the preloaded tasks on the graph */
    exec_status start() {
      //! Finalize the graph (this could take a while so you should do
      //! it before calling start for timing purposes)
      _graph.finalize();

      // Ensure that the data manager has the correct scope_factory
      if(data_manager != NULL) {
        data_manager->set_scope_factory(&scope_manager);
      }


      
      aborted = false;
      terminatoraborted = false;
      /* Timing */
      if (_timer == NULL) {
        /* If graphlab called in a loop, timer is started only once */
        _timer = new timer();
        _timer->start();
      }
      lasttermcheck = lowres_time_seconds();
      /* Enable scheduler to clean up in restarts */
      scheduler.start();

      /* Wait for all threads to return */
      logger(LOG_INFO, "Wait until finished...");

      bool not_finished = true;
      while(not_finished) {
        size_t cpuid = random::rand_int(ncpus - 1);
        not_finished = run_next_task(cpuid);                     
      }
      double running_time = _timer->current_time();

      scheduler.stop();
      
      logstream(LOG_INFO) << "Running time: " << running_time << std::endl;
      /**
       * Log task counts. It is useful to see worke-specific task
       * counts to see if work was distributed evenly
       */
      size_t total_counts = 0;
      size_t total_work = 0;
      for(size_t wid = 0; wid < task_counts.size(); ++wid) {
        total_counts += task_counts[wid];
        total_work += worker_works[wid];
        logger(LOG_INFO,
               "Worker %d finished: task count = %d, work = %ld",
               wid, task_counts[wid], worker_works[wid]);
      } // end of loop over task_counts
      logger(LOG_INFO, "=== Total task count: %d,   work=%ld", total_counts, 
             total_work);
      
      if (!aborted) {
        return EXEC_COMPLETED;
      } else {
        if (taskbudget > 0 && taskcount.inc() > taskbudget) {
          return EXEC_MAX_TASKS_EXCEEDED;
        } else {
          return EXEC_TIMEOUT;
        }
      }
      
    } // end of start

    /**
     * Get the total number of updates executed by this engine
     */
    size_t last_update_count() {
      size_t total_updates = 0;
      foreach(size_t tc, task_counts) {
        total_updates += tc;
      }
      return total_updates;
    } // end of get the total number of updates


  }; // end of sim_thread_engine

  
      
}; // end of namespace graphlab


#include <graphlab/macros_undef.hpp>

#endif
