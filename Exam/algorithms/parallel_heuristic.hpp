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

    void initialize_sink();
    bool time_limit_reached();


    vector<EdgeId> compute_isolating_cut(NodeId terminal, EdgeId blocked_sink_edge);
    optional<vector<NodeId>> merge_node_cut_parallel(const vector<vector<EdgeId>>& cuts, const vector<pair<int,int>>& cutSizes);
    bool did_timeout() const { return timed_out; }
    optional<vector<NodeId>> run(int time_limit_ms);
};