#ifndef GRAPHLAB_UPDATE_TASK_HPP
#define GRAPHLAB_UPDATE_TASK_HPP

#include <graphlab/graph/graph.hpp>
#include <graphlab/scope/iscope.hpp>


#include <graphlab/macros_def.hpp>
namespace graphlab {
  // Predecleration 
  template<typename Graph> class icallback;
  template<typename Graph> class ishared_data;

  
  template<typename Graph>
  class update_task {
  public:

    typedef Graph graph_type;
    typedef iscope<Graph> iscope_type;
    typedef icallback<Graph> callback_type;
    typedef ishared_data<Graph> ishared_data_type;

    /// This is the standard vertex update function
    typedef void(*update_function_type)(iscope_type& scope,
                                        callback_type& scheduler,
                                        ishared_data_type* sdm);


    struct  hash_functor {
      size_t operator()(const update_task& t) const { return t.hash();  }
    };

  private:
    vertex_id_t vertexid;  
    update_function_type func;

  public:
    update_task(vertex_id_t vertexid = -1, 
                update_function_type func = NULL) :
      vertexid(vertexid), func(func) { }

    ~update_task() {}
 
    // Move into engine to simplify typing of this object
    // void execute(scope_type& scope,
    //              ischeduler_callback &scheduler,
    //              const shared_data_manager* data_manager) {
    //   assert(func != NULL);
    //   func(scope, scheduler, data_manager);
    // }
    
    vertex_id_t vertex() const {
      return vertexid;
    }
    
    update_function_type function() const {
      return func;
    }

    /// Returns true if tasks are identical
    bool operator==(const update_task &i) const{
      return vertexid == i.vertexid && func == i.func ;
    }
    
    /// comparator
    bool operator<(const update_task &i) const{
      return (vertexid < i.vertexid) || 
        (vertexid == i.vertexid && func < i.func);
    }
    
    size_t hash() const {
      // TODO: this was arbitrarily decided. need something better here
      return vertexid ^ (size_t)(void*)(func);
    }
  
  };
   
  
}
#include <graphlab/macros_undef.hpp>
#endif
