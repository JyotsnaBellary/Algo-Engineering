#pragma once
#ifndef GRID_GENERATOR_HPP
#define GRID_GENERATOR_HPP

#include "graph.hpp"
#include "file_handler.hpp"
#include <string>
#include <vector>
#include <optional>

// struct GraphInstance {
//     Graph graph;
//     std::vector<NodeId> terminals;
// };

class GridGenerator {
public:
    static optional<GraphInstance> generate_grid_instance(int rows,
                                                int cols,
                                                int num_terminals,
                                                unsigned int seed);

    static void write_instance_to_file(const GraphInstance& instance,
                                       const std::string& filename);
static bool has_edge_between(const Graph& graph, NodeId u, NodeId v);

private:
    static NodeId node_id(int r, int c, int cols);
};

#endif