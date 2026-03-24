#pragma once
#include <vector> 
#include <iostream> 
#include "edge.hpp"
#include "node.hpp"

using namespace std;

class Graph {
    private:
        vector<Node> nodes;
        vector<Edge> edges;

        vector<vector<EdgeId>> adjacency_list;
        vector<vector<NodeId>> neighboring_nodes;
    public:
        Graph();
        Graph(int number_of_nodes);

        // setters
        void set_node(const Node& node);
        void set_edge(Edge &edge, Edge &rev_edge);

        void set_edge(Edge& edge);

        // Adds edge to out_adjacency_list
        void add_edge_adj(Edge &edge);
        void add_neighbor_node(Edge &edge);
        void mark_terminal(NodeId id);

        // Graph Properties
        int number_of_nodes() const;
        int number_of_edges() const;

        // Getters
        const Node& get_node(NodeId nodeId) const;
        const Edge& get_edge(EdgeId edgeId) const;
        Edge& get_edge_ref(EdgeId edgeId);
        const vector<Node>& get_nodes() const;
        const vector<Edge>& get_edges() const;
        const vector<vector<EdgeId>>& get_adjacency_list() const;
        const vector<EdgeId>& get_neighbors(NodeId id) const;
        const vector<NodeId>& get_neighboring_nodes(NodeId id) const;

        void print_graph() const;
        bool valid_node(NodeId u) const;
};