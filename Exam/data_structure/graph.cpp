#include "graph.hpp"
#include <string>

using namespace std;

Graph::Graph()
        : nodes(),
          edges(),
          adjacency_list(),
          neighboring_nodes() {}

void Graph::set_node(const Node& node) {
    // nodes[node.id] = node;
    nodes.push_back(node);
    adjacency_list.push_back({});
    neighboring_nodes.push_back({});
}

void Graph::set_edge(Edge& edge) {
    if (edge.src >= nodes.size() || edge.trg >= nodes.size()) {
        throw out_of_range("set_edge: invalid node id in edge");
    }
    EdgeId eid = static_cast<EdgeId>(edges.size());
    edge.id = eid;
    edges.emplace_back(edge);
    add_edge_adj(edges.back());
}

// Edge Mutator
void Graph::set_edge(Edge &edge, Edge &rev_edge)
{
    EdgeId eid = static_cast<EdgeId>(edges.size());
    edge.id = eid; // ensure edge has a valid id
    edges.emplace_back(edge);

    rev_edge.id = eid + 1; // ensure reverse edge has a valid id
    edges.emplace_back(rev_edge);

    edge.backward_edge_id = rev_edge.id;
    rev_edge.backward_edge_id = edge.id;

    add_edge_adj(edge);
    add_edge_adj(rev_edge);
}

// Adds edge to adjacency_list
void Graph::add_edge_adj(Edge &edge)
{
        if (!valid_node(edge.src))
            throw out_of_range("add_edge_adj: invalid node id " + to_string(edge.src));
        if (edge.id < 0 || edge.id >= static_cast<EdgeId>(edges.size()))
            throw out_of_range("add_edge_adj: invalid edge id " + to_string(edge.id));
        adjacency_list[edge.src].push_back(edge.id);
        add_neighbor_node(edge);
}

// Adds node to adjacency_list
void Graph::add_neighbor_node(Edge &edge)
{
        if (!valid_node(edge.src))
            throw out_of_range("add_edge_adj: invalid node id " + to_string(edge.src));
        if (edge.id < 0 || edge.id >= static_cast<EdgeId>(edges.size()))
            throw out_of_range("add_edge_adj: invalid edge id " + to_string(edge.id));
        neighboring_nodes[edge.src].push_back(edge.trg);
}

void Graph::mark_terminal(NodeId id) {
    if (id >= nodes.size()) {
        throw out_of_range("Node id out of range");
    }

    nodes[id].terminal = true;
}

// Graph Properties

int Graph::number_of_nodes() const { 
    return (int)nodes.size(); 
}

int Graph::number_of_edges() const { 
    return (int)edges.size();
}

// Getters
const Node& Graph::get_node(NodeId nodeId) const {
    return nodes[nodeId];
}

const Edge& Graph::get_edge(EdgeId edgeId) const {
    return edges[edgeId];
}

Edge& Graph::get_edge_ref(EdgeId edgeId) {
    return edges[edgeId];
}

const vector<Node>& Graph::get_nodes() const {
    return nodes;
}

const vector<Edge>& Graph::get_edges() const {
    return edges;
}

const vector<EdgeId>& Graph::get_neighbors(NodeId id) const {
    if (!valid_node(id)) {
        throw out_of_range("get_neighbors: invalid node id " + to_string(id));
    }
    return adjacency_list[id];
}

const vector<NodeId>& Graph::get_neighboring_nodes(NodeId id) const {
    if (!valid_node(id)) {
        throw out_of_range("get_neighbors: invalid node id " + to_string(id));
    }
    return neighboring_nodes[id];
}

// Print for Debugging
void Graph::print_graph() const {
    cout << "Graph:\n";
    cout << "Number of nodes: " << nodes.size() << "\n";
    cout << "Number of edges: " << edges.size() << "\n\n";

    cout << "Nodes:\n";
    for (const auto& node : nodes) {
        cout << "Node " << node.id
                  << " | weight: " << node.weight
                  << " | terminal: " << node.terminal
                  << " | active: " << node.active << "\n";
    }

    cout << "\nEdges:\n";
    for (const auto& edge : edges) {
        cout << "Edge " << edge.id
                  << ": " << edge.src << " -- " << edge.trg
                  << " | weight: " << edge.capacity << "\n";
    }

    cout << "\nAdjacency List:\n";
    for (NodeId src = 0; src < static_cast<NodeId>(nodes.size()); ++src)
    {
        cout << "[" << src << "] : [";
        const auto &A = adjacency_list[src];
        for (size_t i = 0; i < A.size(); ++i)
        {
            EdgeId eid = A[i];
            cout << edges[eid].trg;
            if (i + 1 < A.size())
                cout << " ";
        }
        cout << "]\n";
    }
}

bool Graph::valid_node(NodeId src) const
        {
            return src >= 0 && src < number_of_nodes();
        }