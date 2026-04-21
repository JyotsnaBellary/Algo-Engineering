#pragma once

#include "graph.hpp"
#include <vector>
#include <optional>

struct DualLPSolution {
    vector<double> d;                    // size = number_of_nodes, terminals will stay 0
    vector<vector<double>> y;       // y[u][j]
    double objective_value = 0.0;
};

enum NodeState : uint8_t {
    IN_REGION = 0,
    UNIQUE_BOUNDARY = 1,
    MULTI_BOUNDARY = 2,
    OUTSIDE = 3
};

class Approximation {
private:
    Graph graph;
    vector<NodeId> terminals;
    bool timed_out = false;

    //The dual can be viewed as an assignment of non-negative ‘length’ (or ‘distance’) labels, d_v,to the non-terminal nodes. These labels induce a distance function between pairs of nodes.
    // Define the length of a path to be the sum of the labels of the nodes along the path. The
    // distance between two nodes is the length of the shortest path between them.
    vector<double> d;

    // a variable y_u,j, for every node u and each j =1,...,k, which is supposed
    // to represent the distance between terminal sj and node u
    vector<vector<double>> y; // y[u][j]

    // For each terminal, grow a region with nodes at distance 0.
    vector<vector<bool>> in_region;
    vector<vector<NodeId>> region_nodes;

    // to keep track if it is in a region, unique or multiple boundaries
    vector<NodeState> node_state;

vector<vector<bool>> in_boundary;        // [terminal_index][node]
vector<int> boundary_count;              // [node]
vector<int> first_owner;                 // [node] first terminal whose boundary contains it
vector<int> half_boundary_weight;        // [terminal_index]

    // In either Unique or multiple boundaries
    vector<bool> in_M;                       // [node]

public:
    Approximation(Graph graph, const vector<NodeId>& terminals);

    // solve the dual LP 
    optional<DualLPSolution> calculate_optimal_solution(DualLPSolution& sol, int time_limit_ms);
    bool did_timeout() const { return timed_out; }

    // Returns a cut that separates all terminals, or nullopt if no cut is found within the time limit.
    optional<vector<NodeId>> run(int time_limit_ms = 12000);
};