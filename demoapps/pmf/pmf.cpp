 /* Copyright (c) 2009 Carnegie Mellon University. 
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
 *  Code written by Danny Bickson, CMU
 *  Any changes to the code must include this original license notice in full.
 */


#include <fstream>
#include <cmath>
#include <cstdio>
#include <stdlib.h>
#include  <boost/algorithm/string/predicate.hpp>
#include "graphlab.hpp"
#include "pmf.h"
#include "prob.hpp"
#include "bptf.hpp"
#include "sgd.hpp"
#include "lanczos.hpp"
#include "svd.hpp"
#include "biassgd.hpp"
#include "nmf.hpp"
#include "als.hpp"
#include "tensor.hpp"
#include "unittest.hpp"
#include "io.hpp"
#include "stats.hpp"
#include "implicit.hpp"
#include "lasso.hpp"
#include "cosamp.hpp"
#include "../gabp/advanced_config.h"
#include "svdpp.hpp"
#include "timesvdpp.hpp"
#include "rbm.hpp"
#include "libfm.hpp"
#include <graphlab/macros_def.hpp>

const char * runmodesname[] = {"ALS_MATRIX (Alternating least squares)", "BPTF_MATRIX (Bayesian Prob. Matrix Factorization)", "BPTF_TENSOR (Bayesian Prob. Tensor Factorization)", "BPTF_TENSOR_MULT", "ALS_TENSOR_MULT", "SVD++", "SGD (Stochastic Gradient Descent)", "SVD (Singular Value Decomposition via LANCZOS)", "NMF (non-negative factorization)", "Weighted alternating least squares", "Alternating least squares with sparse user factor matrix", "Alternating least squares with doubly sparse (user/movie) factor matrices", "Alternating least squares with sparse movie factor matrix", "SVD (Singular Value Decomposition)", "Koren's time-SVD++", "Bias-SGD", "RBM (Restricted Bolzman Machines)", "Libfm (factorization machines)"};

const char * countername[] = {"EDGE_TRAVERSAL", "BPTF_SAMPLE_STEP", "CALC_RMSE_Q", "ALS_LEAST_SQUARES", \
  "BPTF_TIME_EDGES", "BPTF_LEAST_SQUARES", "CALC_OBJ", "BPTF_MVN_RNDEX", "BPTF_LEAST_SQUARES2", "SVD_MULT_A", "SVD_MULT_A_TRANSPOSE"};

const char * testtypename[] = {"TRAINING", "VALIDATION", "TEST", "TEST2"};





using namespace graphlab;
using namespace std;

advanced_config ac;
problem_setup ps;


float predict(const vertex_data& v1, const vertex_data& v2, const edge_data * edge, float rating, float & prediction){

   //assert((!ps.tensor && ps.isals) || ps.algorithm == NMF);

   //predict missing value based on dot product of movie and user iterms
   prediction = dot(v1.pvec, v2.pvec);	
   //truncate prediction to allowed values
   prediction = std::min((double)prediction, ac.maxval);
   prediction = std::max((double)prediction, ac.minval);
   //return the squared error
   float sq_err = powf(prediction - rating, 2);
   if (ps.algorithm == WEIGHTED_ALS)
     sq_err *= edge->time;
   return sq_err;
}

template<typename vertex_data>
double predict(const vertex_data& user, const vertex_data &movie, const edge_data * edge, float rating, float & prediction){
   return predict(user, movie, edge, rating, prediction);
} 
float predict(const vertex_data& v1, const vertex_data& v2, const edge_data_mcmc * edge, const vertex_data *v3, float rating, float &prediction){
  return predict(v1, v2, (edge_data*)NULL, v3, rating, prediction);	
}
float predict(const vertex_data& v1, const vertex_data& v2, const edge_data * edge, const vertex_data *v3, float rating, float &prediction){
	if (v3 == NULL) //matrix	
		return predict(v1,v2,edge, rating,prediction);

	//else this is a tensor, compute prediction by the tensor
	//product of the three vectors: user, movie and time bins
	prediction = 0;
	for (int i=0; i< v1.pvec.size(); i++){
	   prediction += (v1.pvec[i] * v2.pvec[i] * v3->pvec[i]);
	}
   prediction = std::min((double)prediction, ac.maxval);
   prediction = std::max((double)prediction, ac.minval);
   float sq_err = powf(prediction - rating, 2);
   if (ps.algorithm == WEIGHTED_ALS)
     sq_err *= edge->time;
   return sq_err;
   
}

  //constructor
vertex_data::vertex_data(){
    //pvec = zeros(ac.D);
    rmse = 0;
    num_edges = 0;
}


template<typename core>
void add_tasks(core & glcore){

  std::vector<vertex_id_t> um;
  int start = 0;
  int end = ps.M+ps.N;
  if (ps.algorithm == SVD_PLUS_PLUS || ps.algorithm == TIME_SVD_PLUS_PLUS || ps.algorithm == RBM || ps.algorithm == STOCHASTIC_GRADIENT_DESCENT || ps.algorithm == BIAS_SGD || ps.algorithm == LIBFM)
     end = ps.M;

  for (int i=start; i< end; i++)
    um.push_back(i);

  if (ac.shuffle){
    logstream(LOG_INFO) << "Shuffling tasks" << std::endl;
    std::random_shuffle(um.begin(), um.end()-1);
  } 
 
 // add update function for user and movie nodes (tensor dims 1+2) 
  switch (ps.algorithm){
     case ALS_MATRIX:
     case ALS_SPARSE_USR_FACTOR:
     case ALS_SPARSE_USR_MOVIE_FACTORS:
     case ALS_SPARSE_MOVIE_FACTOR:
     case BPTF_TENSOR:
     case BPTF_MATRIX:
     case WEIGHTED_ALS:
     case BPTF_TENSOR_MULT:
     case ALS_TENSOR_MULT:
       glcore.add_tasks(um, user_movie_nodes_update_function, 1);
       break;

     case SVD_PLUS_PLUS:
       glcore.add_tasks(um, svd_plus_plus_update_function, 1);
       break;

     case TIME_SVD_PLUS_PLUS:
       glcore.add_tasks(um, time_svd_plus_plus_update_function, 1);
       break;

     case STOCHASTIC_GRADIENT_DESCENT:
       glcore.add_tasks(um, sgd_update_function, 1);
       break;
  
     case BIAS_SGD:
       glcore.add_tasks(um, bias_sgd_update_function, 1);
       break;
 
     case RBM:
       glcore.add_tasks(um, rbm_update_function, 1);
        break;
   
     case LIBFM:
       glcore.add_tasks(um, libfm_update_function, 1);
       break;
 
     case LANCZOS:
     case NMF:
     case SVD:
       //lanczos is unique since it has more than one update function
       //lanczos code is done later
       break;
     default: assert(false);
 }

  // add update function for time nodes (dim 3)
  if (ps.tensor && ps.algorithm != TIME_SVD_PLUS_PLUS && ps.algorithm != LIBFM){
    std::vector<vertex_id_t> tv;
    for (int i=ps.M+ps.N; i< ps.M+ps.N+ps.K; i++)
      tv.push_back(i);

      glcore.add_tasks(tv, time_node_update_function, 1);
  }
}

template<typename graph_type>
void init(graph_type *g){

  srand(time(NULL));

  if (ps.tensor){
    ps.dp = GenDiffMat(ps.K)*ps.pT;
    if (ac.debug)
      std::cout<<ps.dp<<std::endl;
  }
  
  if (ps.BPTF)
     init_self_pot(); 

  switch(ps.algorithm){
   case SVD_PLUS_PLUS:
     init_svdpp(g); break;
  
   case BIAS_SGD:
     init_biassgd(g); break;

   case TIME_SVD_PLUS_PLUS:
     init_time_svdpp<graph_type>(g); 
      break;

   case LANCZOS: 
     init_lanczos(); break;
  
   case SVD:
     init_svd(); break;
 
   case NMF:
      nmf_init(); break;

   case ALS_MATRIX:
   case ALS_TENSOR_MULT:
   case ALS_SPARSE_USR_FACTOR:
   case ALS_SPARSE_USR_MOVIE_FACTORS:
   case ALS_SPARSE_MOVIE_FACTOR:
   case WEIGHTED_ALS:
   case BPTF_TENSOR_MULT:
   case BPTF_MATRIX:
   case BPTF_TENSOR:
   case STOCHASTIC_GRADIENT_DESCENT:
      init_pmf<graph_type>(g); break;

   case RBM:
      rbm_init(); break;

   case LIBFM:
       init_libfm(g); break;
    
   default: assert(false);
  }

}


template<typename core, typename graph_type, typename vertex_data>
void run_graphlab(core &glcore, graph_type * validation_graph){

     logstream(LOG_INFO) << "starting with scheduler: " << ac.scheduler << std::endl;
     if (boost::starts_with(ac.scheduler,"round_robin")){
        if (ps.algorithm == NMF)
           logstream(LOG_FATAL)<<"NMF should not be used with round_robin scheduler. Please don't specify a scheduler using the --scheduler= command" << std::endl;
        ac.round_robin = true;
     }
     glcore.start();
     // calculate final RMSE
     //double res, train_rmse =  agg_rmse_by_movie<graph_type,vertex_data>(res), res2;
     
     /*double obj = -1;
     double validation_rmse = 0; 
     if (ps.algorithm != TIME_SVD_PLUS_PLUS && ps.algorithm != RBM && ps.algorithm != BIAS_SGD && ps.algorithm != TIME_SVD_PLUS_PLUS){
       obj = calc_obj<graph_type, vertex_data>(res);
			 validation_rmse = calc_rmse_wrapper<graph_type, vertex_data>(validation_graph, true, res2);
			 printf(ac.printhighprecision ? 
			  "Final result. Obj=%g, TRAIN RMSE= %0.12f VALIDATION RMSE= %0.12f.\n":
			  "Final result. Obj=%g, TRAIN RMSE= %0.4f VALIDATION RMSE= %0.4f.\n"
			   , obj,  train_rmse, validation_rmse);
     }*/
     double runtime = ps.gt.current_time();
     printf("Finished in %lf seconds\n", runtime);
     if (ac.unittest > 0){
        verify_result(ps.obj, ps.training_rmse, ps.validation_rmse);
     }
}



/** 
 * ==== SETUP AND START
 */
template<typename gl_types, typename core, typename graph_type, typename vertex_data, typename edge_data>
void start(command_line_options& clopts) {
   
    core glcore;
    if (ps.glcore == NULL)
      ps.glcore = &glcore;

  
    ps.algorithm = (runmodes)ac.algorithm;
    printf("Setting run mode %s\n", runmodesname[ps.algorithm]);

/*  if (ac.scheduler == "round_robin"){
      char schedulerstring[256];
      sprintf(schedulerstring, "round_robin(max_iterations=%d,block_size=1)", ac.iter);
      clopts.set_scheduler_type(schedulerstring);
      assert(ac.iter > 0);
    }
 */
    graph_type &training = glcore.graph();
    graph_type validation_graph;
    graph_type test_graph, test_graph2; 

    ps.verify_setup();
    ps.glcore->set_engine_options(clopts); 

    logger(LOG_INFO, "%s starting\n",runmodesname[ps.algorithm]);

    //read the training data
    printf("loading data file %s\n", ac.datafile.c_str());
    if (!ac.manualgraphsetup){
    
    load_pmf_graph<graph_type,gl_types,vertex_data,edge_data>(ac.datafile.c_str(), &training, &training, TRAINING);
    ps.set_graph(&training, TRAINING);

  //read the vlidation data (optional)
    printf("loading data file %s\n", (ac.datafile+"e").c_str());
    load_pmf_graph<graph_type,gl_types,vertex_data,edge_data>((ac.datafile+"e").c_str(),&training, &validation_graph, VALIDATION);
    ps.set_graph(&validation_graph, VALIDATION);

   }  

   //UGLY: for time_svd++ , test files can not be loaded late.. to be fixed later
   if (ps.algorithm == TIME_SVD_PLUS_PLUS || ps.algorithm == SVD_PLUS_PLUS){
    //read the test data (optional)
    printf("loading data file %s\n", (ac.datafile+"t").c_str());
    load_pmf_graph<graph_type,gl_types,vertex_data,edge_data>((ac.datafile+"t").c_str(),&training, &test_graph, TEST);
    ps.set_graph(&test_graph, TEST);


    if (ac.test2){
      printf("loading data file %s\n", (ac.datafile+"t2").c_str());
      load_pmf_graph<graph_type,gl_types,vertex_data,edge_data>((ac.datafile+"t2").c_str(),&training, &test_graph2, TEST2);
      ps.set_graph(&test_graph2, TEST2);
    }
  }

  if (ac.loadfactors){
     import_uvt_from_file<graph_type>();
  }


  if (ac.stats){
    calc_stats<graph_type, vertex_data, edge_data>(TRAINING);
    calc_stats<graph_type, vertex_data, edge_data>(VALIDATION);
    calc_stats<graph_type, vertex_data, edge_data>(TEST);
    exit(0);
  }

  if (ps.isals){
    printf("setting regularization weight to %g\n", ac.als_lambda);
    ps.pU=ps.pV=ac.als_lambda;
  }
    
  add_tasks<core>(glcore);
  
  printf("%s for %s (%d, %d, %d):%d.  D=%d\n", runmodesname[ac.algorithm], ps.tensor?"tensor":"matrix", ps.M, ps.N, ps.K, ps.L, ac.D);
  
   init<graph_type>(&training);

   if (ac.datafile == "netflix" || ac.datafile == "netflix-r"){
       ac.minval = 1; ac.maxval = 5;
    }
   else if ((ac.datafile == "kddcup" || ac.datafile == "kddcup2") && ac.maxval == DEF_MAX_VAL){
       ac.minval = 0; ac.maxval = 100;
   }

   double res = 0, MAE = 0;
   if (ps.BPTF){
     calc_rmse<graph_type, vertex_data>(&training, TRAINING, res, MAE, NULL);
  } else if (ps.algorithm == LANCZOS || ps.algorithm == SVD){
     //In Lanczos, we limit the number of eigenvalues to matrix smaller dimension
     if (ac.iter > ps.M || ac.iter > ps.N)
       ac.iter = std::min(ps.M, ps.N);
  }
 
 
  if (ps.BPTF){
    //sample hyper priors and noise level
    sample_hyperpriors<graph_type>(res);
  }

  training.finalize();  
  ps.gt.start();

  /**** START GRAPHLAB AND RUN UNTIL COMPLETION *****/
    switch(ps.algorithm){
      case ALS_TENSOR_MULT:
      case ALS_MATRIX:
      case ALS_SPARSE_USR_FACTOR:
      case ALS_SPARSE_USR_MOVIE_FACTORS:
      case ALS_SPARSE_MOVIE_FACTOR:
      case WEIGHTED_ALS:
      case BPTF_TENSOR_MULT:
      case BPTF_TENSOR:
      case BPTF_MATRIX:
      case SVD_PLUS_PLUS:
      case STOCHASTIC_GRADIENT_DESCENT:
      case TIME_SVD_PLUS_PLUS:
      case BIAS_SGD:
      case RBM:
      case LIBFM:
         run_graphlab<core, graph_type, vertex_data>(glcore, &validation_graph);
         break;
     
     case LANCZOS:
        lanczos<core>(glcore); 
        break;

     case SVD:
        svd<core>(glcore);
        break;

     case NMF:
        nmf<core>(&glcore); 
        break;
  }

 if (ps.algorithm != LANCZOS && ps.algorithm != SVD){
    /**** OUTPUT TEST FORMAT *****/
    if (ac.reduce_mem_consumption){
      training.reduce_mem_consumption();
      validation_graph.reduce_mem_consumption();
    }
     

    //for all other algos we can delay loading of test data to after the run
    if (ps.algorithm != TIME_SVD_PLUS_PLUS && ps.algorithm != SVD_PLUS_PLUS){ 
    //read the test data (optional)
    printf("loading data file %s\n", (ac.datafile+"t").c_str());
    load_pmf_graph<graph_type,gl_types,vertex_data,edge_data>((ac.datafile+"t").c_str(),&training, &test_graph, TEST);
    ps.set_graph(&test_graph, TEST);


    if (ac.test2){
      printf("loading data file %s\n", (ac.datafile+"t2").c_str());
      load_pmf_graph<graph_type,gl_types,vertex_data,edge_data>((ac.datafile+"t2").c_str(),&training, &test_graph2, TEST2);
      ps.set_graph(&test_graph2, TEST2);
    }
    }
    //calculate an export prediction on test data
   if (ac.exporttest){
      if (ac.outputvalidation) //experimental: output prediction of validation data
 	     export_test_file<graph_type, vertex_data, edge_data>(validation_graph, VALIDATION, true);
      else {//output prediction of test data, as required by KDD 
	     export_test_file<graph_type, vertex_data, edge_data>(test_graph, TEST, true);
       if (ac.test2)
	       export_test_file<graph_type, vertex_data, edge_data>(test_graph2, TEST2, true);
      }
    }
 }
  print_runtime_counters(); 
  if (ac.exportlinearmodel)
    write_output<graph_type, vertex_data>(&training);
}



int do_main(int argc, const char *argv[]){
  global_logger().set_log_level(LOG_INFO);
  global_logger().set_log_to_console(true);
  logstream(LOG_INFO)<< "(c) PMF/BPTF/ALS/SVD++/time-SVD++/SGD/Lanczos/SVD/bias-SGD/RBM Code written By Danny Bickson, CMU\nSend bug reports and comments to danny.bickson@gmail.com\n";

  int version = ITPP_SUPPORT;
#ifdef HAS_EIGEN
  logstream(LOG_WARNING)<<"Program compiled with Eigen Support\n";
  version = EIGEN_SUPPORT;
#elif defined(HAS_ITPP)
  logstream(LOG_WARNING)<<"Program compiled with it++ Support\n";
#endif

    command_line_options clopts;
    ac.init_command_line_options(clopts);
    if (ac.mainfunc){ //if called from main(), parse command line arguments
      if (!clopts.parse(argc, argv))
         return EXIT_FAILURE;
      ac.scheduler = clopts.scheduler_type;
      //clopts.set_scope_type("null");
   } 
  omp_set_num_threads(clopts.get_ncpus());

   //just display linear algebra package version and exit
   if (ac.show_version)
      return version;

   if (ac.unittest > 0)
        unit_testing(ac.unittest, clopts);

   switch(ac.algorithm){
      case ALS_TENSOR_MULT:
      case BPTF_TENSOR_MULT:
 	      start<gl_types_mult_edge, gl_types_mult_edge::core, graph_type_mult_edge, vertex_data, multiple_edges>(clopts);
        break;
 
      case BPTF_TENSOR:
      case BPTF_MATRIX:
        start<gl_types_mcmc, gl_types_mcmc::core, graph_type_mcmc, vertex_data, edge_data_mcmc>(clopts);
        break;
 
      case SVD_PLUS_PLUS:
      case TIME_SVD_PLUS_PLUS:
      case BIAS_SGD:
      case RBM:
       case ALS_MATRIX:
      case ALS_SPARSE_USR_FACTOR:
      case ALS_SPARSE_USR_MOVIE_FACTORS:
      case ALS_SPARSE_MOVIE_FACTOR:
      case WEIGHTED_ALS:
      case STOCHASTIC_GRADIENT_DESCENT:
      case LANCZOS:
      case SVD:
      case NMF:
      case LIBFM:
        start<gl_types, gl_types::core, graph_type, vertex_data, edge_data>(clopts);
        break;

      default:
        assert(false);
  }
  return EXIT_SUCCESS;

}


#include <graphlab/macros_undef.hpp>
