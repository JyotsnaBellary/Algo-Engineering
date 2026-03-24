#pragma once 
#include "types.hpp"

struct Edge {
    EdgeId id;
    int src;
    int trg;
    int capacity;
    bool active;

    EdgeId backward_edge_id;          // original opposite direction, if you want it
    // EdgeId reverse_residual_edge_id;  // probably not needed here
};

struct ResidualEdge {
    EdgeId id;
    int src;
    int trg;
    int residual_capacity;
    bool active;

    EdgeId rev;               // reverse residual edge
    EdgeId original_edge_id;  // optional, useful for mapping back
};

// public:
    //     Edge(int id, int u, int v, int weight = 1)
    //         : id(id),
    //           u(u),
    //           v(v),
    //           weight(weight) {}

        // int getId() const { return id; }
        // int getU() const { return u; }
        // int getV() const { return v; }
        // int getWeight() const { return weight; }

        // void setWeight(int w) { weight = w; }