#pragma once

#include <string>
#include <vector>

#include "graph.hpp"
#include "file_handler.hpp"

using namespace std;

struct RunResult
{
    bool cut_found = false;
    bool timed_out = false;
    vector<NodeId> cut_nodes;
    double execution_time_ms = -1.0;
    bool valid_cut = false;
    double exact_best_time = -1.0;
};

struct BenchmarkResult
{
    string instance_name;
    string track_name;

    int num_nodes = 0;
    int num_edges = 0;
    int num_terminals = 0;

    double graph_density = -1.0;

    // Execution times and cut sizes for each algorithm. If an algorithm did not find a cut, the cut size will be -1. If an algorithm timed out, the execution time will be -1.
    double heuristic_execution_time_ms = -1.0;
    double parallel_heuristic_execution_time_ms = -1.0;
    double approximation_execution_time_ms = -1.0;
    double exact_execution_time_ms = -1.0;
    double exact_best_time_ms = -1.0;

    // Cut sizes for each algorithm. If an algorithm did not find a cut, the cut size will be -1.
    int heuristic_cut_size = -1;
    int parallel_heuristic_cut_size = -1;
    int approximation_cut_size = -1;
    int exact_cut_size = -1;

    // Validity and timeout flags for each algorithm.
    bool heuristic_valid = false;
    bool parallel_heuristic_valid = false;
    bool approximation_valid = false;
    bool exact_valid = false;

    // Time out status
    bool heuristic_timed_out = false;
    bool parallel_heuristic_timed_out = false;
    bool approximation_timed_out = false;
    bool exact_timed_out = false;

    // solution Quality ratios (compared to exact)
    double heuristic_exact_ratio = -1.0;
    double parallel_heuristic_exact_ratio = -1.0;
    double approximation_exact_ratio = -1.0;

    double cut_to_node_heuristic_ratio = -1.0;
    double cut_to_node_parallel_heuristic_ratio = -1.0;
    double cut_to_node_approximation_ratio = -1.0;
    double cut_to_node_exact_ratio = -1.0;
};

class Benchmark
{
public:
    // Tests the validity of the given cut by checking if it separates all terminals in the graph. Returns true if the cut is valid, false otherwise.
    static bool test_validity(const Graph &graph, const vector<NodeId> &cutNodes, const vector<NodeId> &terminals);
    static void calculate_ratios(BenchmarkResult &result);

    // Run the algorithms 
    static RunResult run_approximation(Graph graph, const vector<NodeId> &terminals, int time_limit_ms = 180000);
    static RunResult run_exact(Graph graph, const vector<NodeId> &terminals, int k_approx, int M, int time_limit_ms = 180000);
    static RunResult run_heuristic(Graph graph, const vector<NodeId> &terminals, int time_limit_ms = 180000);
    static RunResult run_parallel_heuristic(Graph graph, const vector<NodeId> &terminals, int time_limit_ms = 180000);
    static void write_to_csv(const vector<BenchmarkResult> &results, const string &filename);

    // Benchmarking functions for different tracks and graph types
    static void run_benchmark_small(int time_limit_ms);
    static void run_benchmark_medium(int time_limit_ms);
    static void run_benchmark_large(int time_limit_ms);
    static void run_benchmark_pace_graphs(int small_time_limit_ms, int medium_time_limit_ms, int large_time_limit_ms);
    
    // Additional benchmark functions for generated graphs (not part of the PACE tracks)
    static void run_benchmark_grid_graph();
    static BenchmarkResult run_single_generated_instance(const GraphInstance &instance,
                                                         const string &instance_name,
                                                         const string &track_name,
                                                         int time_limit_ms,
                                                         bool run_exact_algo = true,
                                                         bool run_approx_algo = true);
};
