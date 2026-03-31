#pragma once

#include "graph.hpp"
#include <vector>
#include <optional>

using namespace std;

struct MaxFlowResult {
    int maxFlow;
    vector<ResidualEdge> residualEdges;
    vector<vector<EdgeId>> residualAdj;
};

class MaxFlow {
    private:
    
    // Builds the residual network for the given graph. If blocked_edge is provided, that edge will be treated as if it has infinite capacity (effectively blocking it in the original graph).
    static void buildResidualNetwork(const Graph& graph,
                                     vector<ResidualEdge>& residualEdges,
                                     vector<vector<EdgeId>>& residualAdj,
                                     optional<EdgeId> blocked_edge = nullopt);
                                    
    // Uses BFS to find an augmenting path in the residual network. Returns true if a path is found, and fills parentEdge with the edges used to reach each node.                                 
    static bool bfs(const vector<ResidualEdge>& edges,
                    const vector<vector<EdgeId>>& adjacency_list,
                    NodeId s,
                    NodeId t,
                    vector<EdgeId>& parentEdge);
    public:
        // Computes the maximum flow from s to t in the given graph using the Edmonds-Karp algorithm. If limit is provided, it will stop and return the best found solution once the flow reaches that limit. If blocked_edge_id is provided, that edge will be treated as if it has infinite capacity (effectively blocking it in the original graph).
        static MaxFlowResult edmondsKarp(const Graph& graph, NodeId s, NodeId t, optional<int> limit = nullopt, optional<EdgeId> blocked_edge_id = nullopt);

        // After computing the max flow, this function can be used to find the edges that are in the minimum cut. It takes the original graph, the residual edges and adjacency list from the max flow computation, and the source node s. It returns a vector of edge IDs that are in the minimum cut.
        static vector<EdgeId> getMinCutEdges(Graph& graph, const vector<ResidualEdge>& edges, const vector<vector<EdgeId>>& adj,  NodeId s);
};