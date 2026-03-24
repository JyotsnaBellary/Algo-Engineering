#pragma once

#include "graph.hpp"
#include <vector>
#include <optional>

using namespace std;

struct MaxFlowResult {
    int maxFlow;
    std::vector<ResidualEdge> residualEdges;
    std::vector<std::vector<EdgeId>> residualAdj;
};

class MaxFlow {
    private:
        // BFs?
        // int maxFlow;
        // std::vector<Edge> residualEdges;
        // std::vector<std::vector<EdgeId>> residualAdj;
    
    static void buildResidualNetwork(const Graph& graph,
                                     vector<ResidualEdge>& residualEdges,
                                     vector<vector<EdgeId>>& residualAdj,
                                     optional<EdgeId> blocked_edge = nullopt);

    static bool bfs(const vector<ResidualEdge>& edges,
                    const vector<vector<EdgeId>>& adjacency_list,
                    NodeId s,
                    NodeId t,
                    std::vector<EdgeId>& parentEdge);
    public:
        
        static MaxFlowResult edmondsKarp(const Graph& graph, NodeId s, NodeId t, optional<int> limit = nullopt, optional<EdgeId> blocked_edge_id = nullopt);

        static vector<EdgeId> getMinCutEdges(Graph& graph, const vector<ResidualEdge>& edges,
    const vector<vector<EdgeId>>& adj,  NodeId s);
};