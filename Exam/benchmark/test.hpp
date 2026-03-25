#pragma once

#include <random>
#include <vector>

#include "benchmark.hpp"
#include "heuristic.hpp"
#include "graph.hpp"

std::vector<NodeId> choose_random_non_adjacent_terminals(
    const Graph& graph,
    int target_count,
    std::mt19937& rng,
    int max_attempts = 100
);

bool are_adjacent(const Graph& graph, NodeId u, NodeId v);


int test_heuristic( Graph& base_graph,  std::vector<NodeId>& terminals, int time_limit_ms = 18000);
int test_parallel_heuristic( Graph& base_graph,  std::vector<NodeId>& terminals, int time_limit_ms = 18000);
int test_approximation( Graph& base_graph,  std::vector<NodeId>& terminals, int time_limit_ms = 18000);
int test_exact( Graph& base_graph,  std::vector<NodeId>& terminals, int approx_k, int heuristic_k, int time_limit_ms = 18000);

void independent_tests_menu();
void benchmark_menu();
int run_test_menu();
