#pragma once
#include "benchmark.hpp"

// int main() {
//     // run small with 5 minutes, medium with 10 minutes, large with 20 minutes
//     Benchmark::run_benchmark_pace_graphs(90000, 150000, 210000);
//     return 0;
// }

#include <iostream>
#include <vector>

#include "graph.hpp"
#include "maximum_flow.hpp"
#include "file_handler.hpp"
#include "heuristic.hpp"
#include "parallel_heuristic.hpp"
#include "approximation.hpp"
#include "exact.hpp"
#include <random>
#include <numeric>
#include <stdexcept>
using namespace std;
#include <algorithm>
#include <iostream>
#include <vector>
#include <set>
#include <queue>
#include "graph.hpp"
#include "maximum_flow.hpp"
#include "heuristic.hpp"
#include "file_handler.hpp"

using namespace std;

// class Test {
bool are_adjacent(const Graph& graph, NodeId u, NodeId v) {
    const vector<NodeId>& nu = graph.get_neighboring_nodes(u);
    if (find(nu.begin(), nu.end(), v) != nu.end()) {
        return true;
    }

    const vector<NodeId>& nv = graph.get_neighboring_nodes(v);
    return find(nv.begin(), nv.end(), u) != nv.end();
}

vector<NodeId> choose_random_non_adjacent_terminals(
    const Graph& graph,
    int target_count,
    mt19937& rng,
    int max_attempts = 200
) {
    vector<NodeId> all_nodes(graph.number_of_nodes());
    iota(all_nodes.begin(), all_nodes.end(), 0);

    vector<NodeId> best;

    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        shuffle(all_nodes.begin(), all_nodes.end(), rng);

        vector<NodeId> chosen;
        for (NodeId candidate : all_nodes) {
            bool ok = true;
            for (NodeId t : chosen) {
                if (are_adjacent(graph, candidate, t)) {
                    ok = false;
                    break;
                }
            }

            if (ok) {
                chosen.push_back(candidate);
                if ((int)chosen.size() == target_count) {
                    return chosen;
                }
            }
        }

        if (chosen.size() > best.size()) {
            best = chosen;
        }
    }

    throw runtime_error(
        "Could only find " + to_string(best.size()) +
        " pairwise non-adjacent terminals, but needed " +
        to_string(target_count)
    );
}

void validate_cut(const Graph& graph, const vector<NodeId>& cutNodes, const vector<NodeId>& terminals) {
    set<NodeId> cutSet(cutNodes.begin(), cutNodes.end());

    for (size_t i = 0; i < terminals.size(); i++) {
        for (size_t j = i + 1; j < terminals.size(); j++) {
            NodeId t1 = terminals[i];
            NodeId t2 = terminals[j];

            if (cutSet.count(t1) > 0 || cutSet.count(t2) > 0) {
                continue; // One of the terminals is in the cut, so they are separated
            }

            // Check if t1 and t2 are still connected in the graph without cut nodes
            vector<bool> visited(graph.number_of_nodes(), false);
            queue<NodeId> q;
            q.push(t1);
            visited[t1] = true;

            bool connected = false;

            while (!q.empty()) {
                NodeId current = q.front();
                q.pop();

                if (current == t2) {
                    connected = true;
                    break;
                }

                for (EdgeId eid : graph.get_neighbors(current)) {
                    const Edge& e = graph.get_edge(eid);
                    NodeId neighbor = e.trg;

                    if (!visited[neighbor] && cutSet.count(neighbor) == 0) {
                        visited[neighbor] = true;
                        q.push(neighbor);
                    }
                }
            }

            if (connected) {
                cout << "Error: Terminals " << t1 << " and " << t2 << " are still connected after removing cut nodes!" << endl;
                // Optionally, print the path between t1 and t2 for debugging
                cout << "Path between " << t1 << " and " << t2 << ":\n";
                for (NodeId v = 0; v < graph.number_of_nodes(); v++) {
                    if (visited[v]) {
                        cout << v << " ";
                    }
                }
                cout << endl;
                return;
            }
        }
    }

    cout << "Cut is valid: All terminal pairs are separated." << endl;
}

void test_approximation() {
    FileHandler fh;
    Graph base_graph = fh.readSparseGraph("../input_data/osm1.txt");
    // Graph graph = fh.readGraph("../input_data/example_2.txt");

    // cout << "Original graph:\n";
    // graph.print_graph();
    // cout << "\n";

    // choose terminals
    // vector<NodeId> terminals = {1, 3, 7};
    // vector<NodeId> terminals = {3, 7, 300, 450, 100, 70, 371, 400, 13, 12, 430};

    // test for random terminal sets. Ensure there are no edges directly between the terminals 
    vector<int> terminal_sizes = {10, 20, 30, 40, 50, 60, 80, 100};
    mt19937 rng(random_device{}());

    for (int round = 0; round < (int)terminal_sizes.size(); ++round) {
        int k = terminal_sizes[round];
        Graph graph = base_graph;  // fresh copy for each round

        cout << "\n========================================\n";
        cout << "Round " << (round + 1) << " | terminals = " << k << "\n";

        vector<NodeId> terminals = choose_random_non_adjacent_terminals(graph, k, rng);

        cout << "Terminals: ";
        for (NodeId t : terminals) {
            cout << t << " ";
        }
        cout << "\n";

        bool valid_terminal_set = true;
        for (int i = 0; i < (int)terminals.size(); ++i) {
            for (int j = i + 1; j < (int)terminals.size(); ++j) {
                if (are_adjacent(graph, terminals[i], terminals[j])) {
                    valid_terminal_set = false;
                    cout << "Invalid terminal set: " << terminals[i]
                         << " and " << terminals[j]
                         << " are adjacent.\n";
                }
            }
        }

        if (!valid_terminal_set) {
            cout << "Skipping round because terminal generation failed.\n";
            continue;
        }

        for (NodeId t : terminals) {
            graph.mark_terminal(t);
        }

        int time_limit_ms = 180000; // 2 minute time limit for approximation
        Approximation approximation(graph, terminals);
        optional<vector<NodeId>> cutNodes = approximation.run(time_limit_ms);

        cout << "Approximation multiway cut nodes:\n";
        if (cutNodes.has_value()) {
            // for (NodeId nodeId : cutNodes.value()) {
            //     cout << nodeId << " ";
            // }
            cout << "cut size: " << cutNodes->size() << "\n";
        validate_cut(graph, cutNodes.has_value() ? cutNodes.value() : vector<NodeId>(), terminals);

        } else {
            cout << "Failed to find a valid cut.";
        }

        cout << "\n";

    }
}

void test_heuristic() {
    FileHandler fh;
    Graph base_graph = fh.readSparseGraph("../input_data/osm1.txt");
    // Graph graph = fh.readGraph("../input_data/example_2.txt");

    // cout << "Original graph:\n";
    // graph.print_graph();
    // cout << "\n";

    // choose terminals
    // vector<NodeId> terminals = {1, 3, 7};
    // vector<NodeId> terminals = {3, 7, 300, 450, 100, 70, 371, 400, 13, 12, 430};

    // test for random terminal sets. Ensure there are no edges directly between the terminals 
    vector<int> terminal_sizes = {10, 20, 30, 40, 50, 60, 80, 100};
    mt19937 rng(random_device{}());

    for (int round = 0; round < (int)terminal_sizes.size(); ++round) {
        int k = terminal_sizes[round];
        Graph graph = base_graph;  // fresh copy for each round

        cout << "\n========================================\n";
        cout << "Round " << (round + 1) << " | terminals = " << k << "\n";

        vector<NodeId> terminals = choose_random_non_adjacent_terminals(graph, k, rng);

        cout << "Terminals: ";
        for (NodeId t : terminals) {
            cout << t << " ";
        }
        cout << "\n";

        bool valid_terminal_set = true;
        for (int i = 0; i < (int)terminals.size(); ++i) {
            for (int j = i + 1; j < (int)terminals.size(); ++j) {
                if (are_adjacent(graph, terminals[i], terminals[j])) {
                    valid_terminal_set = false;
                    cout << "Invalid terminal set: " << terminals[i]
                         << " and " << terminals[j]
                         << " are adjacent.\n";
                }
            }
        }

        if (!valid_terminal_set) {
            cout << "Skipping round because terminal generation failed.\n";
            continue;
        }

        for (NodeId t : terminals) {
            graph.mark_terminal(t);
        }

        int time_limit_ms = 10; // 1 second time limit for heuristic
        Heuristic heuristic(graph, terminals);
        optional<vector<NodeId>> cutNodes = heuristic.run(time_limit_ms);

        cout << "Heuristic multiway cut nodes:\n";
        for (NodeId nodeId : cutNodes.value_or(vector<NodeId>())) {
            cout << nodeId << " ";
        }
        cout << "\n";

        validate_cut(graph, cutNodes.value_or(vector<NodeId>()), terminals);
    }
}

void test_parallel_heuristic() {
    FileHandler fh;
    Graph base_graph = fh.readSparseGraph("../input_data/osm1.txt");
    // Graph graph = fh.readGraph("../input_data/example_2.txt");

    // cout << "Original graph:\n";
    // graph.print_graph();
    // cout << "\n";

    // choose terminals
    // vector<NodeId> terminals = {1, 3, 7};
    // vector<NodeId> terminals = {3, 7, 300, 450, 100, 70, 371, 400, 13, 12, 430};

    // test for random terminal sets. Ensure there are no edges directly between the terminals 
    vector<int> terminal_sizes = {10, 20, 30, 40, 50, 60, 80, 100};
    mt19937 rng(random_device{}());

    for (int round = 0; round < (int)terminal_sizes.size(); ++round) {
        int k = terminal_sizes[round];
        Graph graph = base_graph;  // fresh copy for each round

        cout << "\n========================================\n";
        cout << "Round " << (round + 1) << " | terminals = " << k << "\n";

        vector<NodeId> terminals = choose_random_non_adjacent_terminals(graph, k, rng);

        cout << "Terminals: ";
        for (NodeId t : terminals) {
            cout << t << " ";
        }
        cout << "\n";

        bool valid_terminal_set = true;
        for (int i = 0; i < (int)terminals.size(); ++i) {
            for (int j = i + 1; j < (int)terminals.size(); ++j) {
                if (are_adjacent(graph, terminals[i], terminals[j])) {
                    valid_terminal_set = false;
                    cout << "Invalid terminal set: " << terminals[i]
                         << " and " << terminals[j]
                         << " are adjacent.\n";
                }
            }
        }

        if (!valid_terminal_set) {
            cout << "Skipping round because terminal generation failed.\n";
            continue;
        }

        for (NodeId t : terminals) {
            graph.mark_terminal(t);
        }

        ParallelHeuristic parallel_heuristic(graph, terminals);
        optional<vector<NodeId>> cutNodes = parallel_heuristic.run(90000);

        cout << "parallel Heuristic multiway cut nodes:\n";
        for (NodeId nodeId : cutNodes.value()) {
            cout << nodeId << " ";
        }
        cout << "\n";

        validate_cut(graph, cutNodes.value(), terminals);
    }
}


void test_exact() {
    FileHandler fh;
    Graph base_graph = fh.readSparseGraph("../input_data/osm1.txt");
    // Graph graph = fh.readGraph("../input_data/example_2.txt");

    // cout << "Original graph:\n";
    // graph.print_graph();
    // cout << "\n";

    // choose terminals
    // vector<NodeId> terminals = {1, 3, 7};
    // vector<NodeId> terminals = {3, 7, 300, 450, 100, 70, 371, 400, 13, 12, 430};

    // test for random terminal sets. Ensure there are no edges directly between the terminals 
    vector<int> terminal_sizes = {20, 30, 40, 50, 60, 80, 100};
    mt19937 rng(random_device{}());

    for (int round = 0; round < (int)terminal_sizes.size(); ++round) {
        int k = terminal_sizes[round];
        Graph graph = base_graph;  // fresh copy for each round

        cout << "\n========================================\n";
        cout << "Round " << (round + 1) << " | terminals = " << k << "\n";

        vector<NodeId> terminals = choose_random_non_adjacent_terminals(graph, k, rng);

        cout << "Terminals: ";
        for (NodeId t : terminals) {
            cout << t << " ";
        }
        cout << "\n";

        bool valid_terminal_set = true;
        for (int i = 0; i < (int)terminals.size(); ++i) {
            for (int j = i + 1; j < (int)terminals.size(); ++j) {
                if (are_adjacent(graph, terminals[i], terminals[j])) {
                    valid_terminal_set = false;
                    cout << "Invalid terminal set: " << terminals[i]
                         << " and " << terminals[j]
                         << " are adjacent.\n";
                }
            }
        }

        if (!valid_terminal_set) {
            cout << "Skipping round because terminal generation failed.\n";
            continue;
        }

        for (NodeId t : terminals) {
            graph.mark_terminal(t);
        }

        Approximation approximation(graph, terminals);
        Exact exact(graph, terminals);
        optional<vector<NodeId>> cutNodes = approximation.run();

        cout << "Approximation multiway cut nodes size :" << cutNodes.has_value() ? cutNodes.value().size() : 0 ;
        // for (NodeId nodeId : cutNodes) {
        //     cout << nodeId << " ";
        // }
        cout << "\n";

        // validate_cut(graph, cutNodes, terminals);

        // run exact now 
        optional<vector<NodeId>> exactCutNodes = exact.run(cutNodes.has_value() ? cutNodes.value().size() : 0, 100, 60000);
        cout << "Exact multiway cut nodes:\n";
        if (!exactCutNodes) {
        cout << "No cut found within the given parameters.\n";
        }
        else {
            cout << "Exact multiway cut nodes size :" << exactCutNodes->size() << endl;
            validate_cut(graph, *exactCutNodes, terminals);

        }
    }
}

void test_exact_simple() {
    FileHandler fh;
    cout << "calling simple";
    // Graph graph = fh.readGraph("../input_data/example_2.txt");
    GraphInstance base_graph = fh.readPaceGraph("../input_data/SteinerTree-PACE-2018-instances/Track2/instance171.gr");
    cout << "test simple";
    vector<NodeId> terminals = base_graph.terminals;
    Graph graph = base_graph.graph;

    for (NodeId t : terminals) {
        graph.mark_terminal(t);
    }

    Approximation approximation(graph, terminals);
        Exact exact(graph, terminals);
        optional<vector<NodeId>> cutNodes = approximation.run(90000);

        cout << "Approximation multiway cut nodes size :" << cutNodes.value().size() << ":";
        // for (NodeId nodeId : cutNodes) {
        //     cout << nodeId << " ";
        // }
        // cout << "\n";

        // validate_cut(graph, cutNodes, terminals);

        cout << "starting now" << endl;
        // run exact now 
        optional<vector<NodeId>> exactCutNodes = exact.run(301, 400, 90000);
    // Exact exact(graph, terminals);
    // optional<vector<NodeId>> cutNodes = exact.run(5, 10);

    cout << "Exact multiway cut nodes:\n";
    if (!exactCutNodes) {
        cout << "No cut found within the given parameters.\n";
    }
    else {
        // for (NodeId nodeId : *exactCutNodes) {
        //     cout << nodeId << " ";
        // }
        cout << exactCutNodes.value().size();
        cout << "\n";
        validate_cut(graph, *exactCutNodes, terminals);

    }

}

void test_maxflow() {
    FileHandler fh;
    Graph graph = fh.readGraph("../input_data/example_2.txt");

    cout << "Original graph:\n";
    // graph.print_graph();
    cout << "\n";

    NodeId source = 7;
    NodeId sink = 1;

    MaxFlowResult result = MaxFlow::edmondsKarp(graph, source, sink);

    cout << "Max flow from " << source << " to " << sink << " = "
         << result.maxFlow << "\n\n";

    vector<EdgeId> cutEdges =
        MaxFlow::getMinCutEdges(graph,
                                result.residualEdges,
                                result.residualAdj,
                                source);

    cout << "Min cut edges:\n";
    for (EdgeId eid : cutEdges) {
        const Edge& e = graph.get_edge(eid);
        cout << "Edge " << e.id << ": "
             << e.src << " -> " << e.trg
             << " (capacity = " << e.capacity << ")\n";
    }
}

// int main() {
//     // test_maxflow();
//     cout << "\n==============================\n\n";
//     // test_approximation();
//     test_exact_simple();
//     // test_parallel_heuristic();
//     // test_heuristic();
//     cout << "\n==============================\n\n";
//     return 0;
// }

// }