/*
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

#include <vector>
#include <string>
#include <fstream>

#include <graphlab.hpp>
#include <graphlab/engine/gl3engine.hpp>
// #include <graphlab/macros_def.hpp>
using namespace graphlab;
size_t PARAM_SIZE = 10000;
size_t PARAMS_PER_POINT = 100;
size_t POINTS_PER_THREAD = 1000;
size_t NUM_VTHREADS = 100;
#define DELTA_SCATTER 0

typedef char vertex_data_type;
typedef empty edge_data_type;
typedef distributed_graph<vertex_data_type, edge_data_type> graph_type;
typedef gl3engine<graph_type> engine_type;
/*
 * A simple function used by graph.transform_vertices(init_vertex);
 * to initialize the vertes data.
 */
void init_vertex(graph_type::vertex_type& vertex) { vertex.data() = 1; }



std::vector<double> true_weights;
double stepsize = 0.1; // eta
std::vector<double> w;

void generate_ground_truth_weight_vector() {
  // generate a random small weight vector between -1 and 1
  true_weights.resize(PARAM_SIZE);
  for (size_t i = 0; i < PARAM_SIZE; ++i) {
    true_weights[i] = random::fast_uniform<double>(-1, 1);
  }
}

/**
 * Generates a simple synthetic binary classification dataset in X,Y with numdata
 * datapointsand where each datapoint has numweights weights and num_features_per_x features
 * Every weight value will be between -1 and 1.
 * Also returns the true weight vector used to generate the data.
 * (the dataset generated by this procedure is actually quite hard to learn)
 *
 * If wseed is not -1, a seed is used to generate the weights. after which
 * the generator will be reset with a non-det seed.
 */
void generate_datapoint(std::vector<size_t> & x,
                        std::vector<double> & xvalue,
                        double& y,
                        size_t num_features_per_x) {
  x.clear();
  xvalue.clear();
  // use logistic regression to predict a y
  double linear_predictor = 0;
  for (size_t j = 0; j < num_features_per_x; ++j) {
    x.push_back(random::fast_uniform<size_t>(0, PARAM_SIZE - 1));
    xvalue.push_back(random::fast_uniform<double>(-1, 1));
    linear_predictor += xvalue[j] * true_weights[x[j]];
  }
  double py0 = 1.0 / (1 + std::exp(linear_predictor));
  double py1 = 1.0 - py0;
  // generate a 0/1Y alue.
  y = py1 + graphlab::random::gaussian() * 0.2;
}



/**
 * Takes a logistic gradient step using the datapoint (x,y)
 * changes the global variable weights, firsttime and timestep
 * Also returns the predicted value for the datapoint
 * At exit, weights will contain the deltas
 */
double logistic_sgd_step(const std::vector<size_t>& x,
                         const std::vector<double>& xvalue,
                         double y,
                         boost::unordered_map<size_t, any> &weights) {
  // compute predicted value of y
  double linear_predictor = 0;
  boost::unordered_map<size_t, any>::iterator iter = weights.begin();
  while (iter != weights.end()) {
    if (iter->second.empty()) iter->second = any(double(0.0));
    ++iter;
  }

  for (size_t i = 0; i < x.size(); ++i) {
    linear_predictor += xvalue[i] * weights[x[i]].as<double>();
  }

  // probability that y is 0
  double py0 = 1.0 / (1 + std::exp(linear_predictor));
  double py1 = 1.0 - py0;
  // note that there is a change that we get NaNs here. If we get NaNs,

  // ok compute the gradient
  // the gradient update is easy
  for (size_t i = 0; i < x.size(); ++i) {
    weights[x[i]].as<double>() = stepsize * (double(y) - py1) * xvalue[i]; // atomic
  }
  return py1;
}

void delta_scatter_fn(any& a, const any& b) {
  if (a.empty()) {
    a = b;
  } else {
    a.as<double>() += b.as<double>();
  }
}

void print_l1_param_error(engine_type::context_type& context) {
  // ask for all the weights
  std::vector<size_t> allq;
  for (size_t i = 0;i < PARAM_SIZE; ++i) {
    allq.push_back(i);
  }
  double l1gap = 0;
  boost::unordered_map<size_t, any> r = context.dht_gather(allq);
  for (size_t i = 0;i < PARAM_SIZE; ++i) {
    double d = 0;
    if (!r[i].empty()) d = r[i].as<double>();
    l1gap += std::fabs(d - true_weights[i]);
  }
  std::cout << "Parameter L1 Gap = " << l1gap << "\n";
}

atomic<size_t> num_points_processed;

void logistic_sgd(engine_type::context_type& context) {

  std::vector<size_t> x;
  std::vector<double> xvalue;
  double y;
  for (size_t i = 0;i < POINTS_PER_THREAD; ++i) {
    generate_datapoint(x, xvalue, y, PARAMS_PER_POINT);
    boost::unordered_map<size_t, any> weights = context.dht_gather(x);
    logistic_sgd_step(x, xvalue, y, weights);
    context.dht_scatter(DELTA_SCATTER, weights);
    num_points_processed.inc();
    logger_ontick(1, LOG_EMPH, "Processed: %ld", num_points_processed.value);
  }
}




int main(int argc, char** argv) {
  // Initialize control plain using mpi
  mpi_tools::init(argc, argv);
  distributed_control dc;
  global_logger().set_log_level(LOG_INFO);

  // Parse command line options -----------------------------------------------
  command_line_options clopts("SGD Simulation");
  clopts.set_scheduler_type("fifo");
  std::string graph_dir;
  clopts.attach_option("param_size", PARAM_SIZE,
                       "Number of parameters");
  clopts.attach_option("params_per_point", PARAMS_PER_POINT,
                       "Density");
  clopts.attach_option("points_per_thread", POINTS_PER_THREAD,
                       "Points to create per thread");
  clopts.attach_option("num_vthreads", NUM_VTHREADS,
                       "Number of threads");
  clopts.attach_option("stepsize", stepsize,
                       "stepsize");


  if(!clopts.parse(argc, argv)) {
    dc.cout() << "Error in parsing command line arguments." << std::endl;
    return EXIT_FAILURE;
  }

  random::seed(100);
  generate_ground_truth_weight_vector();

  random::seed();
  graph_type graph(dc);
  graph.finalize();
  timer ti;
  engine_type engine(dc, graph, clopts);
  engine.register_dht_scatter(DELTA_SCATTER, delta_scatter_fn);

  for (size_t i = 0;i < NUM_VTHREADS; ++i) {
    engine.launch_other_task(logistic_sgd);
  }

  engine.wait();
  if (dc.procid() == 0) {
    engine.launch_other_task(print_l1_param_error);
  }
  engine.wait();
  const float runtime = ti.current_time();
  dc.cout() << "Finished Running engine in " << runtime
            << " seconds." << std::endl;
  dc.cout() << engine.num_updates()
            << " updates." << std::endl;

  dc.barrier();
  // Tear-down communication layer and quit -----------------------------------
  mpi_tools::finalize();
  return EXIT_SUCCESS;
} // End of main


// We render this entire program in the documentation


