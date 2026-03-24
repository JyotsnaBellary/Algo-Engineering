#pragma once
#include <chrono>
#include "graph.hpp"
#include <vector>
#include <optional>
using namespace std;
using Clock = chrono::steady_clock;

struct State {
    std::vector<bool> deleted;
    std::vector<bool> locked;
    std::vector<std::vector<NodeId>> groups;
};

struct CutNetwork {
    Graph cut_graph;
    NodeId super_source;
    NodeId super_sink;
};

class Exact {
private:
    Graph graph;
    vector<NodeId> terminals;
    optional<vector<NodeId>> best_cut;
    Clock::time_point run_start;
    double best_cut_time_ms = -1.0;

    Clock::time_point deadline;
    bool timed_out = false;

    bool time_limit_reached();
public:
    Exact(Graph graph, const vector<NodeId>& terminals);

    NodeId get_node_with_terminal_neighbor(const int terminal_group, const State& state, const vector<int>& group_of) const;
    CutNetwork build_cut_network(const State& state) const;
    int get_minimum_cut_size(const State& state, int k) const;
    optional<vector<NodeId>> nmc(const State& state, int k);
    optional<vector<NodeId>> run(int k_approx, int M,  int time_limit_ms);
     bool did_timeout() const { return timed_out; }
     double get_best_cut_time_ms() const { return best_cut_time_ms; }
     
};