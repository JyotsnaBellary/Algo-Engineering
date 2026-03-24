#include "maximum_flow.hpp"
#include <queue>
#include <climits>

using namespace std;

void MaxFlow::buildResidualNetwork(const Graph& graph,
    vector<ResidualEdge>& residualEdges,
    vector<vector<EdgeId>>& residualAdj, 
    optional<EdgeId> blocked_edge) 
    {

         residualAdj.resize(graph.number_of_nodes());

    for (const Edge& e : graph.get_edges()) {

        if (!e.active) continue;
        if (blocked_edge.has_value() && e.id == blocked_edge.value()) continue;

        EdgeId fid = residualEdges.size();
        EdgeId rid = fid + 1;

        ResidualEdge fwd;
        fwd.id = fid;
        fwd.src = e.src;
        fwd.trg = e.trg;
        fwd.residual_capacity = e.capacity;
        fwd.active = true;
        fwd.rev = rid;
        fwd.original_edge_id = e.id;

        ResidualEdge rev;
        rev.id = rid;
        rev.src = e.trg;
        rev.trg = e.src;
        rev.residual_capacity = 0;
        rev.active = true;
        rev.rev = fid;
        rev.original_edge_id = e.id;

        residualEdges.push_back(fwd);
        residualEdges.push_back(rev);

        residualAdj[fwd.src].push_back(fid);
        residualAdj[rev.src].push_back(rid);
    }
}

bool MaxFlow::bfs(const vector<ResidualEdge>& edges,
    const vector<vector<EdgeId>>& adj, NodeId s, NodeId t, vector<EdgeId>& parentEdge) {
    vector<bool> visited(adj.size(), false);

    queue<NodeId> q;
    q.push(s);
    visited[s] = true;

    // No node has a parent edge initially
    parentEdge.assign(adj.size(), INVALID_EDGE);   
    
    // cout << "BFS from " << s << " to " << t << endl;
    while(!q.empty()) {
        NodeId popped_node = q.front();
        q.pop();

        for(EdgeId edge_id: adj[popped_node]) {
            const ResidualEdge& edge = edges[edge_id];

            // Ignore inactive edges
            if (!edge.active) {
                continue;
            }

            // Ignore edges with no remaining residual capacity
            if (edge.residual_capacity <= 0) {
                continue;
            }

            NodeId v = edge.trg;

            // If v is not visited yet, discover it
            if (!visited[v]) {
                visited[v] = true;
                parentEdge[v] = edge_id;
                q.push(v);

                // As soon as we reach t, augmenting path exists
                if (v == t) {
                    return true;
                }
            }
            
        }
    }

    // Sink not reachable
    return false;
}

MaxFlowResult MaxFlow::edmondsKarp(const Graph& graph, NodeId source, NodeId sink, optional<int> limit, optional<EdgeId> blocked_edge_id) {

    MaxFlowResult result;

    buildResidualNetwork(graph, result.residualEdges, result.residualAdj, blocked_edge_id);

    int maxFlow = 0;
    vector<EdgeId> parentEdge;

    // Keep finding augmenting paths while they exist
    while (bfs(result.residualEdges, result.residualAdj, source, sink, parentEdge)) {

        // Step 1: find bottleneck capacity on the path
        int pathFlow = INT_MAX;
        NodeId current = sink;

        while (current != source) {
            EdgeId edge_id = parentEdge[current];
            const ResidualEdge& edge = result.residualEdges[edge_id];

            pathFlow = min(pathFlow, edge.residual_capacity);

            current = edge.src;
        }

        // Step 2: update residual capacities along the path
        current = sink;

        while (current != source) {
            EdgeId edge_id = parentEdge[current];
            ResidualEdge& edge = result.residualEdges[edge_id];

            ResidualEdge& reverse_edge = result.residualEdges[edge.rev];

            // Reduce residual capacity on the forward edge
            edge.residual_capacity -= pathFlow;
            reverse_edge.residual_capacity += pathFlow;

            current = edge.src;
        }

        // Step 3: add bottleneck to total flow
        maxFlow += pathFlow;

        if (limit.has_value() && maxFlow > limit.value()) {
            break;
        }
    }
    result.maxFlow = maxFlow;
    return result;
}

vector<EdgeId> MaxFlow::getMinCutEdges(Graph& graph, const vector<ResidualEdge>& edges,
    const vector<vector<EdgeId>>& adj, NodeId source) 
    {
    vector<bool> visited(graph.number_of_nodes(), false);
    queue<NodeId> q;

    q.push(source);
    visited[source] = true;

    // BFS in residual graph using only active edges with positive residual capacity
    while (!q.empty()) {
        NodeId current_node = q.front();
        q.pop();

        for (EdgeId edge_id : adj[current_node]) {
            const auto& edge = edges[edge_id];

            if (!edge.active) {
                continue;
            }

            if (edge.residual_capacity <= 0) {
                continue;
            }

            NodeId next_node = edge.trg;

            if (!visited[next_node]) {
                visited[next_node] = true;
                q.push(next_node);
            }
        }
    }

    vector<EdgeId> cutEdges;

    // Collect edges going from reachable to unreachable side
    for (const Edge& edge : graph.get_edges()) {
        if (!edge.active) {
            continue;
        }

        if (visited[edge.src] && !visited[edge.trg]) {
            cutEdges.push_back(edge.id);
        }
    }

    return cutEdges;
}