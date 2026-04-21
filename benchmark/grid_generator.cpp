#include "grid_generator.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <set>
#include <optional>
#include <stdexcept>

using namespace std;

NodeId GridGenerator::node_id(int r, int c, int cols) {
    return r * cols + c;
}

bool GridGenerator::has_edge_between(const Graph& graph, NodeId u, NodeId v) {
    for (EdgeId eid : graph.get_neighbors(u)) {
        const Edge& e = graph.get_edge(eid);
        if (e.trg == v) {
            return true;
        }
    }
    return false;
}

optional<GraphInstance> GridGenerator::generate_grid_instance(int rows,
                                                              int cols,
                                                              int num_terminals,
                                                              unsigned int seed) {
    if (rows <= 0 || cols <= 0) {
        return nullopt;
    }

    const int n = rows * cols;
    if (num_terminals <= 1 || num_terminals > n) {
        return nullopt;
    }

    Graph graph;

    for (NodeId i = 0; i < n; ++i) {
        Node node;
        node.id = i;
        node.weight = 1;
        node.terminal = false;
        node.active = true;
        graph.set_node(node);
    }

    int edge_id = 0;

    auto add_undirected_edge = [&](NodeId u, NodeId v) {
        Edge e1;
        e1.id = edge_id++;
        e1.src = u;
        e1.trg = v;
        e1.capacity = 1;
        e1.active = true;
        graph.set_edge(e1);

        Edge e2;
        e2.id = edge_id++;
        e2.src = v;
        e2.trg = u;
        e2.capacity = 1;
        e2.active = true;
        graph.set_edge(e2);
    };

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            NodeId u = node_id(r, c, cols);

            if (c + 1 < cols) {
                add_undirected_edge(u, node_id(r, c + 1, cols));
            }
            if (r + 1 < rows) {
                add_undirected_edge(u, node_id(r + 1, c, cols));
            }
        }
    }

    // auto has_edge_between = [&](NodeId u, NodeId v) {
    //     for (EdgeId eid : graph.get_neighbors(u)) {
    //         const Edge& e = graph.get_edge(eid);
    //         if (e.trg == v) {
    //             return true;
    //         }
    //     }
    //     return false;
    // };

    vector<NodeId> all_nodes(n);
    iota(all_nodes.begin(), all_nodes.end(), 0);

    mt19937 rng(seed);
    vector<NodeId> terminals;
    const int max_attempts = 50;
    bool success = false;

    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        shuffle(all_nodes.begin(), all_nodes.end(), rng);
        terminals.clear();

        for (NodeId candidate : all_nodes) {
            bool valid_terminal = true;

            for (NodeId chosen_terminal : terminals) {
                if (has_edge_between(graph, candidate, chosen_terminal)) {
                    valid_terminal = false;
                    // cout << "Candidate " << candidate
                    //      << " is adjacent to already chosen terminal " << chosen_terminal
                    //      << ". Skipping.\n";
                    break;
                }
            }

            if (valid_terminal) {
                terminals.push_back(candidate);
                graph.mark_terminal(candidate);
                if (static_cast<int>(terminals.size()) == num_terminals) {
                    success = true;
                    break;
                }

            }
        }

        if (success) {
            break;
        }
    }

    if (!success) {
        return nullopt;
    }


    // for (NodeId t : terminals) {
    //     Node node = graph.get_node(t);
    //     node.terminal = true;
    //     cout << "Chosen terminal: " << t << "\n";
    //     graph.set_node(node);
    // }

    GraphInstance instance;
    instance.graph = graph;
    instance.terminals = terminals;
    return instance;
}

void GridGenerator::write_instance_to_file(const GraphInstance& instance,
                                           const string& filename) {
    ofstream out(filename);
    if (!out) {
        cerr << "Failed to open file for writing: " << filename << '\n';
        return;
    }

    const int num_nodes = instance.graph.number_of_nodes();
    const int num_edges = instance.graph.number_of_edges() / 2;
    const int num_terminals = static_cast<int>(instance.terminals.size());

    out << num_nodes << " " << num_edges << " " << num_terminals << "\n";

    for (NodeId i = 0; i < num_nodes; ++i) {
        out << i << " 0 0\n";
    }

    set<pair<NodeId, NodeId>> written;
    for (NodeId u = 0; u < num_nodes; ++u) {
        for (EdgeId eid : instance.graph.get_neighbors(u)) {
            const Edge& e = instance.graph.get_edge(eid);
            NodeId v = e.trg;

            if (u < v && written.count({u, v}) == 0) {
                out << u << " " << v << "\n";
                written.insert({u, v});
            }
        }
    }

    out << "Terminals\n";
    for (NodeId t : instance.terminals) {
        out << t << "\n";
    }
}