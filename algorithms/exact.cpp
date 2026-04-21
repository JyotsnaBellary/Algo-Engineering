#include "exact.hpp"
#include "maximum_flow.hpp"

#include <set>
#include <algorithm>
#include <limits>

using namespace std;

Exact::Exact(Graph graph, const vector<NodeId> &terminals)
    : graph(graph), terminals(terminals) {}

bool Exact::time_limit_reached() {
    if (Clock::now() >= deadline) {
        timed_out = true;
        return true;
    }
    return false;
}


optional<vector<NodeId>> Exact::nmc(const State &state, int k)
{
    if (time_limit_reached()) {
        return nullopt;
    }

    if (k < 0) {
        return nullopt;
    }

    if (state.groups.size() <= 1) {
        return vector<NodeId>{};
    }

    vector<int> group_of(graph.number_of_nodes(), -1);
    for (int i = 0; i < state.groups.size(); i++) {
        for (NodeId v : state.groups[i]) {
            group_of[v] = i;
        }
    }

    // 1. if an edge has its two ends in two different terminal sets then return “No”;
    for (const Edge &edge : graph.get_edges())
    {
        if (state.deleted[edge.src] || state.deleted[edge.trg]) {
        continue;
        }

        int group_of_src = group_of[edge.src];
        int group_of_trg = group_of[edge.trg];

        if (group_of_src != -1 && group_of_trg != -1 && group_of_src != group_of_trg)
        {
            cout << "Edge " << edge.id << " has its two ends in two different terminal sets. Returning No." << endl;
            return nullopt;
        }
    }

    // 2. if a non-terminal w has two neighbors in two different terminal sets then return w + NMC(G − w, {T1 , . . . , T l }, k − 1); ‡
    for (Node node : graph.get_nodes())
    {
        if (group_of[node.id] != -1 || state.deleted[node.id] || state.locked[node.id])
        {
            continue;
        }

        vector<NodeId> neighbors = graph.get_neighboring_nodes(node.id);
        int first_group = -1;
        int must_be_in_node_cut = false;
        for (NodeId neighbor : neighbors)
        {
            if (state.deleted[neighbor])
            {
                continue;
            }
            int group_of_neighbor = group_of[neighbor];
            if (group_of_neighbor == -1)
                continue;

            if (first_group == -1)
            {
                first_group = group_of_neighbor;
            }
            else if (first_group != group_of_neighbor)
            {
                must_be_in_node_cut = true;
                break;
            }
        }

        if (must_be_in_node_cut) {
            State next_state = state;
            next_state.deleted[node.id] = true;
            auto result = nmc(next_state, k - 1);

            if(result) {
                result->push_back(node.id);
                return result;
            } else {
                return nullopt;
            }
        }
    }

    // 3.find the size m1 of a minimum V-cut between T1 and ⋃lj =2 Tj ;
    int m1 = get_minimum_cut_size(state, k);

    // 4. if m1 > k then return “No”;
    if (m1 > k) {
        return nullopt;
    }

    // 5. if (m1 = 0 and l = 2) then return ∅;
    if (m1 == 0 && state.groups.size() == 2) {
        return vector<NodeId>{};
    }

    // 5.1 if (m1 = 0 and l > 2) then return NMC(G, {T2, . . . , T l }, k) ;
    if (m1 == 0 && state.groups.size() > 2) {
        State next_state = state;
        next_state.groups.erase(next_state.groups.begin());
        return nmc(next_state, k);

    }

    // 6. else pick a non-terminal u that has a neighbor in T1; let T ′1 = T1 + u;
    else {
        NodeId u = get_node_with_terminal_neighbor(0, state, group_of);
        if (u == INVALID_NODE) {
            cout << "No suitable node found for terminal group 0." << endl;
            return nullopt;
        }
        State merged_state = state;
        merged_state.groups[0].push_back(u);
        merged_state.locked[u] = true;

        // 6.1 if the size of a minimum V-cut between T ′1 and ⋃j..l, j =2 Tj is equal to m1 then return NMC(G, {T ′1, T 2, . . . , T l }, k) ;
        int new_m1 = get_minimum_cut_size(merged_state, m1);

        if (new_m1 == m1) {
            return nmc(merged_state, k);
        }

        // 6.2 else S = u + NMC(G − u, {T1, T 2, . . . , T l }, k − 1);
        else {
            State next_state_deleted = state;
            next_state_deleted.deleted[u] = true;

            auto S = nmc(next_state_deleted, k - 1);
            // if S is not “No” then return S;
            if(S) {
                S->push_back(u);
                return S;
            }
            else {
                // 6.3 else return NMC(G, {T ′1, T 2, . . . , T l }, k) .
                return nmc(merged_state, k);
            }
        }
    }
    return nullopt;
}

optional<vector<NodeId>> Exact::run(int k_approx, int M, int time_limit_ms)
{
    timed_out = false;
    deadline = Clock::now() + chrono::milliseconds(time_limit_ms);

    run_start = Clock::now();
    best_cut_time_ms = -1.0;
    best_cut.reset();

    State initial_state;
    initial_state.deleted = vector<bool>(graph.number_of_nodes(), false);
    initial_state.locked = vector<bool>(graph.number_of_nodes(), false);

    for (size_t i = 0; i < terminals.size(); ++i)
    {
        initial_state.groups.push_back({terminals[i]});
        initial_state.locked[terminals[i]] = true;
    }

    int k = min(k_approx, M); // Ensure K doesn't exceed M

    while (k >= 0 && !time_limit_reached()) {
        auto result = nmc(initial_state, k);

        if (timed_out) {
            break;
        }

        if (!result) {
            break;
        }

        if (!best_cut || result->size() < best_cut->size()) {
            best_cut = result;
            best_cut_time_ms =
        chrono::duration<double, milli>(Clock::now() - run_start).count();
        }

        k = int(best_cut->size()) - 1;

    }

    return best_cut;
}

// Builds the cut network for the fiven state. 
CutNetwork Exact::build_cut_network(const State &state) const {
    const int original_nodes = graph.number_of_nodes();
    const int nodes_in_cut_network = 2 * original_nodes + 2; // + super source and super sink
    const NodeId super_source = original_nodes * 2;
    const NodeId super_sink = original_nodes * 2 + 1;
    const int INF = 100000000;
    Graph cut_graph;

    // Set nodes in the cut network
    for (NodeId i = 0; i < nodes_in_cut_network; ++i) {
        Node node;
        node.id = i;
        node.weight = 1;
        node.terminal = false;
        node.active = true;
        cut_graph.set_node(node);
    }

    // Define helper lambdas to get vin and vout for a given node in the original graph
    auto vin = [](NodeId v) { return 2 * v; };
    auto vout = [](NodeId v) { return 2 * v + 1; };

    // For each node in the original graph, create a split edge from vin to vout with capacity 1 if the node is not locked, or INF if it is locked. If the node is deleted, skip it.
    for (NodeId v = 0; v < original_nodes; ++v) {
        if (state.deleted[v]) {
            continue;
        }

        Edge split_edge;
        split_edge.src = vin(v);
        split_edge.trg = vout(v);
        split_edge.capacity = state.locked[v] ? INF : 1;
        split_edge.active = true;
        cut_graph.set_edge(split_edge);
    }

    // Transform edges in the original graph into edges from vout of the source to vin of the target with capacity INF. Additionally, a super source is connected to vin of all terminals in the first group with capacity INF, and vout of all terminals in other groups are connected to a super sink with capacity INF.
    for (const Edge& e : graph.get_edges()) {
        if (state.deleted[e.src] || state.deleted[e.trg]) {
            continue;
        }

        Edge edge;
        edge.src = vout(e.src);
        edge.trg = vin(e.trg);
        edge.capacity = INF;
        edge.active = true;
        edge.backward_edge_id = INVALID_EDGE;
        cut_graph.set_edge(edge);
    }

    // Connect super source to vin of all terminals in the first group with capacity INF, and vout of all terminals in other groups to super sink with capacity INF.
    for (NodeId t : state.groups[0]) {
        Edge edge;
        edge.src = super_source;
        edge.trg = vin(t);
        edge.capacity = INF;
        edge.active = true;
        cut_graph.set_edge(edge);
    }
    
    // connect all other terminal group of nodes to the Sink Node
    for (int i = 1; i < (int)state.groups.size(); ++i) {
        for (NodeId t : state.groups[i]) {
            Edge edge;
            edge.src = vout(t);
            edge.trg = super_sink;
            edge.capacity = INF;
            edge.active = true;
            cut_graph.set_edge(edge);
        }
    }

    CutNetwork network;
    network.cut_graph = cut_graph;
    network.super_source = super_source;
    network.super_sink = super_sink;
    return network;

}

// Get minimum cut using Edmonds Karp 
int Exact::get_minimum_cut_size(const State &state, int k) const {

    CutNetwork cut_network = build_cut_network(state);
    MaxFlowResult result = MaxFlow::edmondsKarp(cut_network.cut_graph, cut_network.super_source, cut_network.super_sink, k);

    return result.maxFlow;

}

// Return a node with terminal neighbor
NodeId Exact::get_node_with_terminal_neighbor(const int terminal_group, const State &state, const vector<int> &group_of) const {
    for (NodeId t : state.groups[terminal_group]) {
        for (NodeId nb : graph.get_neighboring_nodes(t)) {
            if (state.deleted[nb]) continue;
            if (state.locked[nb]) continue;
            if (group_of[nb] != -1) continue;

            return nb;
        }
    }
    cout << "No non-terminal node with a neighbor in terminal group " << terminal_group << " found." << endl;
    return INVALID_NODE;
}
