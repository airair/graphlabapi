#ifndef GL_EMX_GRAPHTYPES_HPP
#define GL_EMX_GRAPHTYPES_HPP
#include <boost/type_traits/decay.hpp>
#include <boost/type_traits/remove_pointer.hpp>
#include <boost/type_traits/remove_const.hpp>
#include <boost/type_traits/function_traits.hpp>
#include <boost/typeof/std/utility.hpp>
#include <boost/function.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/bool.hpp>
#include <boost/mpl/less.hpp>
#include <boost/mpl/comparison.hpp>

// datatype_identifier is a special function which we use to pick up
// the datatypes for the vertex and edge data
#include "datatype_identifier.h"

// using boost to identify the vertex/edge data type. decay all consts
// and pointers. we store the base struct/type

typedef BOOST_TYPEOF(datatype_identifier) datatype_identifier_fn_type;

typedef boost::remove_const<
            boost::remove_pointer<
              boost::function<datatype_identifier_fn_type>::arg1_type>
            ::type>
        ::type gl_emx_vertextype;
typedef boost::remove_const<
            boost::remove_pointer<
              boost::function<datatype_identifier_fn_type>::arg2_type>
            ::type>
        ::type gl_emx_edgetype;

// define the graphlab data types
#include <graphlab.hpp>
typedef graphlab::graph<gl_emx_vertextype, gl_emx_edgetype> emx_graph;
typedef graphlab::types<emx_graph> gl_types;


// update function type
typedef void (*gl_emx_updatefn_type)(uint32_T eml_currentvertex, 
                                    const emxArray_uint32_T *eml_inedges, 
                                    const emxArray_uint32_T *eml_inv, 
                                    const emxArray_uint32_T *eml_outedges, 
                                    const emxArray_uint32_T *eml_outv, 
                                    double eml_handle);

#endif