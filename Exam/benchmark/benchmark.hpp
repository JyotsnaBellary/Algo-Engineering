#pragma once

#include <string>
#include <vector>

#include "graph.hpp"

using namespace std;

struct RunResult {
    bool cut_found = false;
    bool timed_out = false;
    vector<NodeId> cut_nodes;
    double execution_time_ms = -1.0;
    bool valid_cut = false;
    double exact_best_time = -1.0;
};

struct BenchmarkResult {
    string instance_name;
    string track_name;

    int num_nodes = 0;
    int num_edges = 0;
    int num_terminals = 0;

    double graph_density = -1.0;

    double heuristic_execution_time_ms = -1.0;
    double parallel_heuristic_execution_time_ms = -1.0;
    double approximation_execution_time_ms = -1.0;
    double exact_execution_time_ms = -1.0;
    double exact_best_time_ms = -1.0;

    int heuristic_cut_size = -1;
    int parallel_heuristic_cut_size = -1;
    int approximation_cut_size = -1;
    int exact_cut_size = -1;

    bool heuristic_valid = false;
    bool parallel_heuristic_valid = false;
    bool approximation_valid = false;
    bool exact_valid = false;

    bool heuristic_timed_out = false;
    bool parallel_heuristic_timed_out = false;
    bool approximation_timed_out = false;
    bool exact_timed_out = false;

    double heuristic_exact_ratio = -1.0;
    double parallel_heuristic_exact_ratio = -1.0;
    double approximation_exact_ratio = -1.0;

    double cut_to_node_heuristic_ratio = -1.0;
    double cut_to_node_parallel_heuristic_ratio = -1.0;
    double cut_to_node_approximation_ratio = -1.0;
    double cut_to_node_exact_ratio = -1.0;
};

class Benchmark {
public:
    static bool test_validity(const Graph& graph, const vector<NodeId>& cutNodes, const vector<NodeId>& terminals);
    static void calculate_ratios(BenchmarkResult& result);

    static RunResult run_approximation(Graph graph, const vector<NodeId>& terminals, int time_limit_ms = 180000);
    static RunResult run_exact(Graph graph, const vector<NodeId>& terminals, int k_approx, int M, int time_limit_ms = 180000);
    static RunResult run_heuristic(Graph graph, const vector<NodeId>& terminals, int time_limit_ms = 180000);
    static RunResult run_parallel_heuristic(Graph graph, const vector<NodeId>& terminals, int time_limit_ms = 180000);
    static void write_to_csv(const vector<BenchmarkResult>& results, const string& filename);

    static void run_benchmark_small(int time_limit_ms);
    static void run_benchmark_medium(int time_limit_ms);
    static void run_benchmark_large(int time_limit_ms);
    static void run_benchmark_pace_graphs( int small_time_limit_ms, int medium_time_limit_ms, int large_time_limit_ms);

    static void run_benchmark_ba_graph();
    static void run_benchmark_grid_graph();
    static void run_benchmark_sbm_graph();
};
