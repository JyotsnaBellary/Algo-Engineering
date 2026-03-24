#pragma once

#include "graph.hpp"
#include <vector>
#include <chrono>
#include <optional>
using namespace std;
using Clock = chrono::steady_clock;

class Heuristic {
private:
    Graph graph;
    vector<NodeId> terminals;
    vector<vector<EdgeId>> isolatingCuts;

    NodeId sink;
    vector<EdgeId> terminalToSinkEdges;
    int prev_index;

    Clock::time_point deadline;
    bool timed_out = false;
public:
    Heuristic(Graph graph, const vector<NodeId>& terminals);

    void initialize_sink();

    void reset_sink(int terminal);
    bool time_limit_reached();

    vector<EdgeId> compute_isolating_cut(NodeId terminal);
    vector<NodeId> merge_into_node_cut(const vector<vector<EdgeId>>& cuts, const vector<pair<int,int>>& cutSizes);
    // optionally set parallel to true
    optional<vector<NodeId>> run(int time_limit_ms = 120000);
    bool did_timeout() const { return timed_out; }


};