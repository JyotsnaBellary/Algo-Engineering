#pragma once
#include <chrono>

#include "graph.hpp"
#include <vector>
#include <optional>
using namespace std;
using Clock = chrono::steady_clock;

class ParallelHeuristic {
private:
    Graph graph;
    vector<NodeId> terminals;
    vector<vector<EdgeId>> isolatingCuts;
    vector<EdgeId> terminalToSinkEdges;
    NodeId sink;
    Clock::time_point deadline;

    bool timed_out = false;
    int prev_index;

public:
    ParallelHeuristic(Graph graph, const vector<NodeId>& terminals);

    // initialize sinknode
    void initialize_sink();
    bool time_limit_reached();

    // Computes the isolating cut for the given terminal and stores it in the isolatingCuts vector. The isolating cut is a minimum cut that separates the given terminal from all other terminals in the graph. It is computed by temporarily connecting all other terminals to a super sink with infinite capacity edges, and then running a max flow algorithm from the given terminal to the super sink. The edges in the resulting minimum cut are stored in the isolatingCuts vector at the index corresponding to the given terminal.
    vector<EdgeId> compute_isolating_cut(NodeId terminal, EdgeId blocked_sink_edge);
    
    // Get the nodes from the edge cut that is computed in parallel
    optional<vector<NodeId>> merge_node_cut_parallel(const vector<vector<EdgeId>>& cuts, const vector<pair<int,int>>& cutSizes);
    
    // return true if timelimit exceeds
    bool did_timeout() const { return timed_out; }

    // main function to compute the parallel heuristic cut
    optional<vector<NodeId>> run(int time_limit_ms);
};