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

    // Computes the isolating cut for the given terminal and stores it in the isolatingCuts vector. The isolating cut is a minimum cut that separates the given terminal from all other terminals in the graph. It is computed by temporarily connecting all other terminals to a super sink with infinite capacity edges, and then running a max flow algorithm from the given terminal to the super sink. The edges in the resulting minimum cut are stored in the isolatingCuts vector at the index corresponding to the given terminal.
    vector<EdgeId> compute_isolating_cut(NodeId terminal);

    // Get the nodes from the edge cut that is computed 
    vector<NodeId> merge_into_node_cut(const vector<vector<EdgeId>>& cuts, const vector<pair<int,int>>& cutSizes);
    
    // Runs the heuristic algorithm with a time limit. The heuristic works by first computing isolating cuts for each terminal, and then merging these cuts into a single node cut. The merging process is guided by the sizes of the isolating cuts, with the goal of minimizing the size of the final node cut. The function returns an optional vector of NodeIds that represents the cut found by the heuristic. If the heuristic fails to find a cut within the time limit, it returns nullopt.
    optional<vector<NodeId>> run(int time_limit_ms = 120000);
    
    // Returns true if the heuristic algorithm exceeded the time limit during its execution, and false otherwise. This function can be used by the caller to check if the results returned by the heuristic are valid or if they were produced under a time constraint that may have affected their quality.
    bool did_timeout() const { return timed_out; }


};