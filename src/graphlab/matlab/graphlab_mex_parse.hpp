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

#ifndef GRAPHLAB_MEX_TYPECHECK_HPP
#define GRAPHLAB_MEX_TYPECHECK_HPP
#include "mex_save_graph.hpp"
#include "gl_emx_graphtypes.hpp"

/**
 * Performs a check over all the arguments to ensure that the
 * basic input formats are satisfied.
 * Outputs format failures as Matlab warnings.
 * Returns false on failure, true on success.
 */
bool basic_typecheck(mex_parameters &param);

/**
 * Constructs a graph from the vdata, edata, adjmat parameters
 * Return 1 on success.
 * Returns 0 on non-fatal conversion errors/warnings
 * Return -1 on failures
 */
int construct_graph(emx_graph &graph, const mxArray* vdata,
                    const mxArray* adjmat, const mxArray* edata);

/**
 * Tries to insert the edge from src->dest using the data in edata[edataidx]
 * Return 1 on success.
 * Returns 0 on non-fatal conversion errors/warnings
 * Return -1 on failures
 */
int try_add_edge(emx_graph &graph, const mxArray* edata,
                 size_t src, size_t dest, size_t edataidx);

/**
 * Inserts all edges described in adjmat into the graph.
 * adjmat must be a full matrix.
 */
int add_edges_from_full_adj(emx_graph &graph,
                            const mxArray* adjmat,
                            const mxArray* edata);

/**
 * Inserts all edges described in adjmat into the graph.
 * adjmat must be a sparse matrix.
 */
int add_edges_from_sparse_adj(emx_graph &graph,
                              const mxArray* adjmat,
                              const mxArray* edata);
#endif
