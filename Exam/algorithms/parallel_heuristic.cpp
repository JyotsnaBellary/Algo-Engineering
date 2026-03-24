#include "parallel_heuristic.hpp"
#include "maximum_flow.hpp"

#include <set>
#include <algorithm>
#include <limits>

#ifdef _OPENMP
#include <omp.h>
#endif
#include <atomic>

ParallelHeuristic::ParallelHeuristic(Graph graph, const vector<NodeId> &terminals)
    : graph(graph), terminals(terminals), sink(INVALID_NODE) {}

bool ParallelHeuristic::time_limit_reached() {
    if (Clock::now() >= deadline) {
        return true;
    }
    return false;
}

void ParallelHeuristic::initialize_sink()
{
    // add the sink and add edges from all terminals to the sink with INF capacity.
    sink = graph.number_of_nodes();
    Node sink_node;

    sink_node.id = sink;
    sink_node.weight = 1;
    sink_node.terminal = true;
    sink_node.active = true;

    graph.set_node(sink_node);

    terminalToSinkEdges.resize(terminals.size());
    std::fill(terminalToSinkEdges.begin(), terminalToSinkEdges.end(), INVALID_EDGE);

    const int INF = numeric_limits<int>::max() / 4;

    // add one edge from each terminal to sink
    for (int i = 0; i < terminals.size(); i++)
    {
        Edge edge;
        // edge.id = -1; // graph will assign id
        edge.src = terminals[i];
        edge.trg = sink;
        edge.capacity = INF;
        edge.active = true;
        edge.backward_edge_id = INVALID_EDGE;
        graph.set_edge(edge);

        // the newly added edge is the last edge in the graph
        terminalToSinkEdges[i] = graph.number_of_edges() - 1;
    }
}

optional<vector<NodeId>> ParallelHeuristic::merge_node_cut_parallel(const vector<vector<EdgeId>> &isolatingCuts, const vector<pair<int, int>> &cutSizes)
{

    vector<vector<NodeId>> localCuts(cutSizes.size() - 1);

    #pragma omp parallel for schedule(static)
    for (int j = 0; j < (int)cutSizes.size() - 1; j++)
    {
        int idx = cutSizes[j].second;
        auto &bucket = localCuts[j];
        bucket.reserve(isolatingCuts[idx].size());

        for (EdgeId eid : isolatingCuts[idx])
        {
            const Edge &edge = graph.get_edge(eid);

            NodeId u = edge.src;
            NodeId v = edge.trg;
            NodeId chosen = INVALID_NODE;

            if (graph.get_node(u).terminal && !graph.get_node(v).terminal)
            {
                chosen = v;
            }
            else if (graph.get_node(v).terminal && !graph.get_node(u).terminal)
            {
                chosen = u;
            }
            else if (!graph.get_node(u).terminal && !graph.get_node(v).terminal)
            {
                chosen = (graph.get_neighbors(u).size() > graph.get_neighbors(v).size()) ? u : v;
            }

            if (chosen != INVALID_NODE)
            {
                bucket.push_back(chosen);
            }
        }
    }

    set<NodeId> finalCut;
    for (const auto &cutPart : localCuts)
    {
        for (NodeId v : cutPart)
        {
            finalCut.insert(v);
        }
    }

    return vector<NodeId>(finalCut.begin(), finalCut.end());
}

vector<EdgeId> ParallelHeuristic::compute_isolating_cut(NodeId terminal, EdgeId blocked_sink_edge)
{
    // call max flow algorithm
    MaxFlowResult result = MaxFlow::edmondsKarp(graph, terminal, sink, std::nullopt,
        blocked_sink_edge);

    vector<EdgeId> cut = MaxFlow::getMinCutEdges(
        graph,
        result.residualEdges,
        result.residualAdj,
        terminal);

    // optional: remove sink edges from the returned cut
    vector<EdgeId> filteredCut;
    for (EdgeId eid : cut)
    {
        const Edge &edge = graph.get_edge(eid);

        if (edge.trg == sink)
        {
            continue;
        }

        filteredCut.push_back(eid);
    }
    return filteredCut;
}

optional<vector<NodeId>> ParallelHeuristic::run(int time_limit_ms)
{
    for (const Edge &edge : graph.get_edges())
    {
        if (!edge.active)
            continue;

        NodeId u = edge.src;
        NodeId v = edge.trg;

        if (graph.get_node(u).terminal && graph.get_node(v).terminal)
        {
            cout << "Edge " << edge.id << " has its two ends in two different terminal sets." << endl;
            return nullopt;
        }
    }
    timed_out = false;
    deadline = Clock::now() + chrono::milliseconds(time_limit_ms);
    atomic<bool> stop = false;

    initialize_sink();

    isolatingCuts.assign(terminals.size(), {});
    std::vector<std::pair<int, int>> cutSizes(terminals.size());

    #pragma omp parallel for schedule(dynamic)
        for (int i = 0; i < (int)terminals.size(); i++)
        {
            // prin he thread it is running here
            if (stop) {
                continue;
            }

            if (time_limit_reached()) {
                stop = true;
                continue;
            }

        
            vector<EdgeId> cut = compute_isolating_cut(terminals[i], terminalToSinkEdges[i]);
           
        if (time_limit_reached()) {
        stop = true;
        continue;
    }

            isolatingCuts[i] = std::move(cut);
            cutSizes[i] = { (int)isolatingCuts[i].size(), i };
        }

    
    sort(cutSizes.begin(), cutSizes.end());

    if (stop) {
    timed_out = true;
    return nullopt;
}
    return merge_node_cut_parallel(isolatingCuts, cutSizes);
    // take union of the smallest k-1 cuts
}