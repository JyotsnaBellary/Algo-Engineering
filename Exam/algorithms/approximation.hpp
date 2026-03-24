#pragma once

#include "graph.hpp"
#include <vector>
#include <optional>

struct DualLPSolution {
    std::vector<double> d;                    // size = number_of_nodes, terminals will stay 0
    std::vector<std::vector<double>> y;       // y[u][j]
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

//     a variable y_u,j, for every node u and each j =1,...,k, which is supposed
// to represent the distance between terminal sj and node u
    vector<vector<double>> y; // y[u][j]

    vector<std::vector<bool>> in_region;
    vector<std::vector<NodeId>> region_nodes;
    vector<NodeState> node_state;

vector<vector<bool>> in_boundary;        // [terminal_index][node]
vector<int> boundary_count;              // [node]
vector<int> first_owner;                 // [node] first terminal whose boundary contains it
vector<int> half_boundary_weight;        // [terminal_index]
vector<bool> in_M;                       // [node]

public:
    Approximation(Graph graph, const vector<NodeId>& terminals);

    optional<DualLPSolution> calculate_optimal_solution(DualLPSolution& sol, int time_limit_ms);

    // void calculate_region(int terminal_index, const DualLPSolution& lp_solution, vector<bool>& in_region);

    // void calculate_boundary(const vector<NodeId>& region, vector<NodeId>& boundary);

    // void calculate_half_boundary(NodeId terminal, const DualLPSolution& lp_solution, vector<NodeId>& half_boundary);
    bool did_timeout() const { return timed_out; }
    optional<vector<NodeId>> run(int time_limit_ms = 12000);
};