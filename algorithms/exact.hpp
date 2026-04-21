#pragma once
#include <chrono>
#include "graph.hpp"
#include <vector>
#include <optional>
using namespace std;
using Clock = chrono::steady_clock;

struct State
{
    vector<bool> deleted;
    vector<bool> locked;
    vector<vector<NodeId>> groups;
};

struct CutNetwork
{
    Graph cut_graph;
    NodeId super_source;
    NodeId super_sink;
};

class Exact
{
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
    Exact(Graph graph, const vector<NodeId> &terminals);

    // Returns anode with a neighbor in one of the terminal sets
    NodeId get_node_with_terminal_neighbor(const int terminal_group, const State &state, const vector<int> &group_of) const;
    
    // Builds the cut network for the given state. The cut network is a directed graph where each node in the original graph is split into two nodes (vin and vout) connected by an edge with capacity 1 if the node is not locked, or INF if it is locked. Edges in the original graph are transformed into edges from vout of the source to vin of the target with capacity INF. Additionally, a super source is connected to vin of all terminals in the first group with capacity INF, and vout of all terminals in other groups are connected to a super sink with capacity INF.
    CutNetwork build_cut_network(const State &state) const;

    // calls the mincut algorithm on the cut network built from the given state and returns the size of the minimum cut. The parameter k is used to control the maximum cut size that the mincut algorithm will consider, which can affect the performance of the algorithm.
    int get_minimum_cut_size(const State &state, int k) const;

    // Does not let time limit to exceed and keeps track of the best cut 
    bool did_timeout() const { return timed_out; }
    double get_best_cut_time_ms() const { return best_cut_time_ms; }

    // recursively called function
    optional<vector<NodeId>> nmc(const State &state, int k);

    // Runs the exact algorithm with a time limit. Returns the best cut found within the time limit, or nullopt if no cut is found. The parameters k_approx and M are used to control the initial value of k in the nmc function, which can affect the performance of the algorithm.
    optional<vector<NodeId>> run(int k_approx, int M, int time_limit_ms);
};