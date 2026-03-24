#include "file_handler.hpp"
#include <fstream>
#include <iostream>

using namespace std;

Graph FileHandler::readGraph(const string& filePath) {

    ifstream file(filePath);

    if (!file) {
        cerr << "Error opening file" << endl;
        exit(1);
    }

    int number_of_nodes, number_of_edges, terminals;

    file >> number_of_nodes >> number_of_edges >> terminals;

    Graph graph;

    cout << number_of_edges << endl;

    for (NodeId i = 0; i < number_of_nodes; i++) {
        Node node;
        node.id = i;
        node.weight = 1;
        node.terminal = false;
        node.active = true;
        graph.set_node(node);
    }

    cout << "setting up edges now";
    for (int i = 0; i < number_of_edges; i++) {
        Edge edge;
        Edge reverse_edge;

        int src, trg;
        file >> edge.src >> edge.trg;
        edge.capacity = 1;
        edge.active = true;
        reverse_edge.src = edge.trg;
        reverse_edge.trg = edge.src;
        reverse_edge.capacity = reverse_edge.capacity = edge.capacity;
        reverse_edge.active = true;
        // reverse_edge.shortcut = false;
        // cout << edge.src << edge.trg << edge.cost;
        graph.set_edge(edge, reverse_edge);
    }

    // cout << "assigning terminals" << endl;
    // for (int i = 0; i < terminals; i++) {
    //     NodeId t;
    //     file >> t;
    //     graph.mark_terminal(t);
    // }

    return graph;
}

Graph FileHandler::readSparseGraph(const std::string& filePath) {
    std::ifstream file(filePath);

    if (!file) {
        std::cerr << "Error opening file: " << filePath << std::endl;
        std::exit(1);
    }

    int number_of_nodes, number_of_edges;
    file >> number_of_nodes >> number_of_edges;

    Graph graph;

    std::cout << "Nodes: " << number_of_nodes
              << ", Edges: " << number_of_edges << std::endl;

    // Read node lines: node_id latitude longitude
    for (int i = 0; i < number_of_nodes; i++) {
        int node_id;
        double lat, lon;

        file >> node_id >> lat >> lon;

        Node node;
        node.id = node_id;
        node.weight = 1;
        node.terminal = false;
        node.active = true;

        graph.set_node(node);
    }

    std::cout << "Setting up edges now..." << std::endl;

    // Read undirected edges and add both directions
    for (int i = 0; i < number_of_edges; i++) {
        Edge edge;
        Edge reverse_edge;

        int src, trg;
        file >> src >> trg;

        edge.src = src;
        edge.trg = trg;
        edge.capacity = 1;
        edge.active = true;

        reverse_edge.src = trg;
        reverse_edge.trg = src;
        reverse_edge.capacity = 1;
        reverse_edge.active = true;

        graph.set_edge(edge, reverse_edge);
    }

    return graph;
}

GraphInstance FileHandler::readPaceGraph(const std::string& filePath) {
    std::ifstream file(filePath);

    if (!file) {
        std::cerr << "Error opening file: " << filePath << std::endl;
        std::exit(1);
    }

    GraphInstance graphInstance;
    Graph& graph = graphInstance.graph;
    std::string token;
    int number_of_nodes = 0;

    while (file >> token) {
        if (token == "SECTION") {
            std::string section_name;
            file >> section_name;

            if (section_name == "Graph") {
                std::string label;
                file >> label >> number_of_nodes;   // Nodes <n>
                file >> label;                      // Edges
                int number_of_edges;
                file >> number_of_edges;

                for (int i = 0; i < number_of_nodes; ++i) {
                    Node node;
                    node.id = i;
                    node.weight = 1;
                    node.terminal = false;
                    node.active = true;
                    graph.set_node(node);
                }

                for (int i = 0; i < number_of_edges; ++i) {
                    std::string edge_tag;
                    int u, v, w;
                    file >> edge_tag >> u >> v >> w;   // E u v w

                    Edge edge;
                    Edge reverse_edge;

                    edge.src = u - 1;
                    edge.trg = v - 1;
                    edge.capacity = w;
                    edge.active = true;
                    edge.backward_edge_id = INVALID_EDGE;

                    reverse_edge.src = v - 1;
                    reverse_edge.trg = u - 1;
                    reverse_edge.capacity = w;
                    reverse_edge.active = true;
                    reverse_edge.backward_edge_id = INVALID_EDGE;

                    graph.set_edge(edge, reverse_edge);
                }

                file >> token; // END
            }
            else if (section_name == "Terminals") {
                std::string label;
                int terminal_count;
                file >> label >> terminal_count;   // Terminals <k>

                for (int i = 0; i < terminal_count; ++i) {
                    std::string terminal_tag;
                    int t;
                    file >> terminal_tag >> t;     // T <node>
                    graphInstance.terminals.push_back(t - 1);
                    graph.mark_terminal(t - 1);
                }

                file >> token; // END
            }
            else {
                // Skip unknown sections until END
                while (file >> token && token != "END") {
                }
            }
        }
        else if (token == "EOF") {
            break;
        }
    }

    return graphInstance;
}
