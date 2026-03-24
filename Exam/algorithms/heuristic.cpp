#include "heuristic.hpp"
#include "maximum_flow.hpp"

#include <set>
#include <algorithm>
#include <limits>

Heuristic::Heuristic(Graph graph, const vector<NodeId> &terminals)
    : graph(graph), terminals(terminals), sink(INVALID_NODE), prev_index(-1) {}

bool Heuristic::time_limit_reached() {
    if (Clock::now() >= deadline) {
        timed_out = true;
        return true;
    }
    return false;
}

void Heuristic::initialize_sink()
{
    // add the sink and add edges from all terminals to the sink with INF capacity.
    sink = graph.number_of_nodes();
    Node sink_node;

    prev_index = -1;

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

void Heuristic::reset_sink(int terminal_index)
{
    // reactivate previous
    if (prev_index != -1)
    {
        graph.get_edge_ref(terminalToSinkEdges[prev_index]).active = true;
    }

    // deactivate current
    graph.get_edge_ref(terminalToSinkEdges[terminal_index]).active = false;

    prev_index = terminal_index;
}

vector<NodeId> Heuristic::merge_into_node_cut(const vector<vector<EdgeId>> &cuts, const vector<pair<int, int>> &cutSizes)
{

    set<NodeId> finalCut;

    for (size_t j = 0; j + 1 < cutSizes.size(); j++)
    {
        int idx = cutSizes[j].second;
        for (EdgeId eid : isolatingCuts[idx])
        {
            const Edge edge = graph.get_edge(eid);

            NodeId u = edge.src;
            NodeId v = edge.trg;

            // print if one of them is terminal or not and their degree

            if (graph.get_node(u).terminal && !graph.get_node(v).terminal)
            {
                finalCut.insert(v);
            }
            else if (graph.get_node(v).terminal && !graph.get_node(u).terminal)
            {
                finalCut.insert(u);
            }
            else if (!graph.get_node(u).terminal && !graph.get_node(v).terminal)
            {
                // choose higher degree node
                if (graph.get_neighbors(u).size() > graph.get_neighbors(v).size())
                {
                    finalCut.insert(u);
                }
                else
                {
                    finalCut.insert(v);
                }
            }
        }
    }

    return vector<NodeId>(finalCut.begin(), finalCut.end());
}

vector<EdgeId> Heuristic::compute_isolating_cut(NodeId terminal)
{
    // call max flow algorithm
    MaxFlowResult result = MaxFlow::edmondsKarp(graph, terminal, sink);

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

optional<vector<NodeId>> Heuristic::run(int time_limit_ms)
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
    // cout << "Running heuristic with time limit: " << time_limit_ms << " ms" << endl;
    timed_out = false;
    deadline = Clock::now() + chrono::milliseconds(time_limit_ms);

    initialize_sink();

    isolatingCuts.clear();

    std::vector<std::pair<int, int>> cutSizes; // {cut_size, terminal_index}

    // for  each terminal
    for (int i = 0; i < terminals.size(); i++)
    {
        if (time_limit_reached()) {
            if (prev_index != -1) {
                graph.get_edge_ref(terminalToSinkEdges[prev_index]).active = true;
            }
            return nullopt;
        }
        
        reset_sink(i);

        NodeId terminal = terminals[i];

        // check if the terminal is set to true
        // first deactivate its edge to the sink
        vector<EdgeId> cut = compute_isolating_cut(terminal);

        if (time_limit_reached()) {
            if (prev_index != -1) {
                graph.get_edge_ref(terminalToSinkEdges[prev_index]).active = true;
            }
            return nullopt;
        }

        isolatingCuts.push_back(cut);
        cutSizes.push_back({cut.size(), i});
        // compute isolating cut for this terminal
        // store the cut
        // reactivate the edge to the sink
    }

    if (prev_index != -1)
    {
        graph.get_edge_ref(terminalToSinkEdges[prev_index]).active = true;
    }

    sort(cutSizes.begin(), cutSizes.end());

    // cout << "Isolating cut sizes:\n";
    // for (size_t i = 0; i < cutSizes.size(); i++)
    // {
    // int idx = cutSizes[i].second;
    // cout << "Terminal " << terminals[idx] << ": cut size = " << cutSizes[i].first << "\n";
    // }
    return merge_into_node_cut(isolatingCuts, cutSizes);
    // take union of the smallest k-1 cuts
}