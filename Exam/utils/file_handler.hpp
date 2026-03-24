#pragma once

#include <string>
#include "../data_structure/graph.hpp"

struct GraphInstance {
    Graph graph;
    std::vector<NodeId> terminals;
};

class FileHandler {

public:
    static Graph readGraph(const string& filename);
    static Graph readSparseGraph(const string& filename);
    static  GraphInstance readPaceGraph(const string& filename);
};