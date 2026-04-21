#include "test.hpp"

#include <iostream>
#include <vector>

#include "graph.hpp"
#include "benchmark.hpp"
#include "maximum_flow.hpp"
#include "file_handler.hpp"
#include "approximation.hpp"
#include "exact.hpp"
#include "file_handler.hpp"
#include "heuristic.hpp"
#include "parallel_heuristic.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>

namespace
{

    namespace fs = filesystem;

    string resolve_input_path(const string &filename)
    {
        const vector<fs::path> candidates = {
            fs::path("input_data") / filename,
            fs::path("..") / "input_data" / filename,
        };

        for (const fs::path &candidate : candidates)
        {
            if (fs::exists(candidate))
            {
                return candidate.string();
            }
        }

        throw runtime_error("Could not locate input file: " + filename);
    }

} // namespace

enum class GraphKind
{
    Sparse,
    Pace
};

// for user runs
struct GraphOption
{
    string label;
    string path;
    GraphKind kind;
};

// for user test
struct TestSelection
{
    bool heuristic = false;
    bool parallel_heuristic = false;
    bool approximation = false;
    bool exact = false;
};

Graph load_selected_graph()
{
    FileHandler fh;

    vector<GraphOption> options = {
        {"osm1", "osm1.txt", GraphKind::Sparse},
        {"osm2", "osm2.txt", GraphKind::Sparse},
        {"osm5", "osm5.txt", GraphKind::Sparse},
        {"instance053", "SteinerTree-PACE-2018-instances/Track1/instance053.gr", GraphKind::Pace},
        {"instance085", "SteinerTree-PACE-2018-instances/Track1/instance085.gr", GraphKind::Pace},
        {"instance099", "SteinerTree-PACE-2018-instances/Track1/instance099.gr", GraphKind::Pace},
    };

    cout << "\nChoose graph:\n";
    for (int i = 0; i < (int)options.size(); ++i)
    {
        cout << (i + 1) << ". " << options[i].label << "\n";
    }
    cout << "Choice: ";

    int choice;
    cin >> choice;

    const auto &selected = options.at(choice - 1);

    if (selected.kind == GraphKind::Sparse)
    {
        return fh.readSparseGraph(resolve_input_path(selected.path));
    }

    return fh.readPaceGraph(resolve_input_path(selected.path)).graph;
}

// for running tests with increasing k size
bool ask_yes_no(const string &prompt)
{
    char answer;
    cout << prompt << " (y/n): ";
    cin >> answer;
    return answer == 'y' || answer == 'Y';
}

// Unit test for max flow and min cut
void test_maxflow()
{
    FileHandler fh;
    Graph graph = fh.readGraph(resolve_input_path("example_2.txt"));

    cout << "Original graph:\n";
    cout << "\n";

    NodeId source = 7;
    NodeId sink = 1;

    MaxFlowResult result = MaxFlow::edmondsKarp(graph, source, sink);

    vector<EdgeId> cutEdges =
        MaxFlow::getMinCutEdges(graph,
                                result.residualEdges,
                                result.residualAdj,
                                source);

    // Print the edges in the minimum cut
    cout << "Minimum cut edges:\n";
    for (EdgeId edgeId : cutEdges)
    {       
        cout << "Edge " << edgeId
             << ": " << graph.get_edge(edgeId).src
             << " -- " << graph.get_edge(edgeId).trg
             << " | capacity: " << graph.get_edge(edgeId).capacity
             << "\n";
    }
}

vector<NodeId> choose_random_non_adjacent_terminals(
    const Graph &graph,
    int target_count,
    mt19937 &rng,
    int max_attempts)
{
    vector<NodeId> all_nodes(graph.number_of_nodes());
    iota(all_nodes.begin(), all_nodes.end(), 0);

    vector<NodeId> best_found;

    for (int attempt = 0; attempt < max_attempts; ++attempt)
    {
        shuffle(all_nodes.begin(), all_nodes.end(), rng);

        vector<NodeId> chosen;

        for (NodeId candidate : all_nodes)
        {
            bool is_adjacent_to_existing_terminal = false;

            for (NodeId terminal : chosen)
            {
                if (are_adjacent(graph, candidate, terminal))
                {
                    is_adjacent_to_existing_terminal = true;
                    break;
                }
            }

            if (!is_adjacent_to_existing_terminal)
            {
                chosen.push_back(candidate);

                if ((int)chosen.size() == target_count)
                {
                    return chosen;
                }
            }
        }

        if (chosen.size() > best_found.size())
        {
            best_found = chosen;
        }
    }

    throw runtime_error(
        "Could only find " + to_string(best_found.size()) +
        " pairwise non-adjacent terminals, but needed " +
        to_string(target_count));
}

// are any two chosen terminals adjacent in the graph?
bool are_adjacent(const Graph &graph, NodeId u, NodeId v)
{
    const vector<NodeId> &neighbors_u = graph.get_neighboring_nodes(u);
    return find(neighbors_u.begin(), neighbors_u.end(), v) != neighbors_u.end();
}

// Test with increasing terminal size on the same graph 
void test_all_with_increasing_terminal_sizes()
{
    TestSelection selection;

    // Let the user choose which algorithms to run.
    selection.heuristic = ask_yes_no("Run heuristic?");
    selection.parallel_heuristic = ask_yes_no("Run parallel heuristic?");
    selection.approximation = ask_yes_no("Run approximation?");
    selection.exact = ask_yes_no("Run exact?");

    if (!selection.heuristic &&
        !selection.parallel_heuristic &&
        !selection.approximation &&
        !selection.exact)
    {
        cout << "No tests selected.\n";
        return;
    }

    FileHandler fh;
    Graph base_graph = fh.readSparseGraph(resolve_input_path("osm1.txt"));

    vector<int> terminal_sizes = {10, 20, 30, 40, 50, 60, 80, 100};
    mt19937 rng(12345); // Fixed seed so runs are reproducible.

    for (size_t round = 0; round < terminal_sizes.size(); ++round)
    {
        int k = terminal_sizes[round];

        cout << "\n========================================\n";
        cout << "Round " << (round + 1) << " | terminals = " << k << "\n";

        // Generate one terminal set for this k.
        // All selected algorithms use the same set so results are comparable.
        vector<NodeId> terminals =
            choose_random_non_adjacent_terminals(base_graph, k, rng);

        cout << "Terminals: ";
        for (NodeId t : terminals)
        {
            cout << t << " ";
            base_graph.mark_terminal(t);
        }
        cout << "\n";

        int k_heuristic;
        int k_approx;
        if (selection.heuristic)
        {
            cout << "\n--- Heuristic ---\n";
            k_heuristic = test_heuristic(base_graph, terminals);
        }

        if (selection.parallel_heuristic)
        {
            cout << "\n--- Parallel Heuristic ---\n";
            k_heuristic = test_parallel_heuristic(base_graph, terminals);
        }

        if (selection.approximation)
        {
            cout << "\n--- Approximation ---\n";
            k_approx = test_approximation(base_graph, terminals);
        }

        if (selection.exact)
        {
            cout << "\n--- Exact ---\n";
            test_exact(base_graph, terminals, k_approx, k_heuristic);
        }
    }
}

int test_heuristic(Graph &base_graph, vector<NodeId> &terminals, int time_limit_ms)
{
    Graph graph = base_graph; // fresh copy so this test is isolated

    for (NodeId t : terminals)
    {
        graph.mark_terminal(t);
    }

    Heuristic heuristic(graph, terminals);

    const auto start = chrono::steady_clock::now();
    optional<vector<NodeId>> cut_nodes = heuristic.run(time_limit_ms);
    const auto end = chrono::steady_clock::now();

    double execution_time_ms =
        chrono::duration<double, milli>(end - start).count();

    // cout << "Terminals: ";
    for (NodeId t : terminals)
    {
        graph.mark_terminal(t);
    }

    if (!cut_nodes.has_value())
    {
        cout << "Heuristic returned no cut.\n";
        cout << "Execution time (ms): " << execution_time_ms << "\n";
        return -1;
    }

    cout << "Execution time (ms): " << execution_time_ms << "\n";
    cout << "Cut size: " << cut_nodes->size() << "\n";

    cout << "Cut nodes: ";
    for (NodeId node : cut_nodes.value())
    {
        cout << node << " ";
    }
    cout << "\n";

    bool valid = Benchmark::test_validity(graph, cut_nodes.value(), terminals);
    cout << "Valid: " << (valid ? "yes" : "no") << "\n";

    return cut_nodes->size();
}

int test_parallel_heuristic(Graph &base_graph, vector<NodeId> &terminals, int time_limit_ms)
{
    Graph graph = base_graph; // Fresh copy so this run is isolated.

    for (NodeId t : terminals)
    {
        graph.mark_terminal(t);
    }

    ParallelHeuristic parallel_heuristic(graph, terminals);

    const auto start = chrono::steady_clock::now();
    optional<vector<NodeId>> cut_nodes = parallel_heuristic.run(time_limit_ms);
    const auto end = chrono::steady_clock::now();

    double execution_time_ms =
        chrono::duration<double, milli>(end - start).count();

    // cout << "Terminals: ";
    // for (NodeId t : terminals) {
    //     cout << t << " ";
    // }
    // cout << "\n";

    if (!cut_nodes.has_value())
    {
        cout << "Parallel heuristic returned no cut.\n";
        cout << "Execution time (ms): " << execution_time_ms << "\n";
        return -1;
    }

    cout << "Cut nodes (" << cut_nodes->size() << "): ";
    for (NodeId node : cut_nodes.value())
    {
        cout << node << " ";
    }
    cout << "\n";

    cout << "Cut size: " << cut_nodes->size() << "\n";
    cout << "Execution time (ms): " << execution_time_ms << "\n";

    bool valid = Benchmark::test_validity(graph, cut_nodes.value(), terminals);
    cout << "Valid: " << (valid ? "yes" : "no") << "\n";

    return cut_nodes->size();
}

int test_approximation(Graph &base_graph, vector<NodeId> &terminals, int time_limit_ms)
{
    Graph graph = base_graph; // Fresh copy so this run is isolated.

    for (NodeId t : terminals)
    {
        graph.mark_terminal(t);
    }

    Approximation approximation(graph, terminals);

    const auto start = chrono::steady_clock::now();
    optional<vector<NodeId>> cut_nodes = approximation.run(time_limit_ms);
    const auto end = chrono::steady_clock::now();

    double execution_time_ms =
        chrono::duration<double, milli>(end - start).count();

    // cout << "Terminals: ";
    // for (NodeId t : terminals) {
    //     cout << t << " ";
    // }
    // cout << "\n";

    if (!cut_nodes.has_value())
    {
        cout << "Approximation returned no cut.\n";
        cout << "Execution time (ms): " << execution_time_ms << "\n";
        return -1;
    }

    cout << "Cut nodes (" << cut_nodes->size() << "): ";
    for (NodeId node : cut_nodes.value())
    {
        cout << node << " ";
    }
    cout << "\n";

    cout << "Cut size: " << cut_nodes->size() << "\n";
    cout << "Execution time (ms): " << execution_time_ms << "\n";

    bool valid = Benchmark::test_validity(graph, cut_nodes.value(), terminals);
    cout << "Valid: " << (valid ? "yes" : "no") << "\n";

    return cut_nodes->size();
}

int test_exact(Graph &base_graph, vector<NodeId> &terminals, int approx_k, int heuristic_k, int time_limit_ms)
{
    Graph graph = base_graph; // Fresh copy so this run is isolated.

    for (NodeId t : terminals)
    {
        graph.mark_terminal(t);
    }

    // Adjust these if you want a different exact setup.
    int M = heuristic_k;

    Exact exact(graph, terminals);

    const auto start = chrono::steady_clock::now();
    optional<vector<NodeId>> cut_nodes = exact.run(approx_k, M, time_limit_ms);
    const auto end = chrono::steady_clock::now();

    double execution_time_ms =
        chrono::duration<double, milli>(end - start).count();

    // cout << "Terminals: ";
    // for (NodeId t : terminals) {
    //     cout << t << " ";
    // }
    // cout << "\n";

    if (!cut_nodes.has_value())
    {
        cout << "Exact returned no cut.\n";
        cout << "Execution time (ms): " << execution_time_ms << "\n";
        return -1;
    }

    cout << "Cut nodes (" << cut_nodes->size() << "): ";
    for (NodeId node : cut_nodes.value())
    {
        cout << node << " ";
    }
    cout << "\n";

    cout << "Cut size: " << cut_nodes->size() << "\n";
    cout << "Execution time (ms): " << execution_time_ms << "\n";
    cout << "Best Cut found after: " << exact.get_best_cut_time_ms() << "\n";

    bool valid = Benchmark::test_validity(graph, cut_nodes.value(), terminals);
    cout << "Valid: " << (valid ? "yes" : "no") << "\n";

    return cut_nodes->size();
}

// test with increasing terminal size on the same graph
vector<int> choose_terminal_sizes()
{
    while (true)
    {
        cout << "\nTerminal mode\n";
        cout << "1. Increasing terminal sizes\n";
        cout << "2. One custom terminal size\n";
        cout << "Choice: ";

        int choice;
        cin >> choice;

        switch (choice)
        {
        case 1:
            return {10, 20, 30, 40, 50, 60, 80, 100};

        case 2:
        {
            int k;
            cout << "Enter number of terminals: ";
            cin >> k;
            return {k};
        }

        default:
            cout << "Invalid choice.\n";
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');

        }
    }
}
// for running tests with increasing k size
void run_selected_independent_tests(
    const TestSelection &selection,
    Graph &base_graph,
    const vector<int> &terminal_sizes,
    int time_limit_ms)
{
    mt19937 rng(12345);

    for (size_t round = 0; round < terminal_sizes.size(); ++round)
    {
        int k = terminal_sizes[round];

        cout << "\n========================================\n";
        cout << "Round " << (round + 1) << " | terminals = " << k << "\n";

        vector<NodeId> terminals =
            choose_random_non_adjacent_terminals(base_graph, k, rng);

        int heuristic_k = -1;
        int approx_k = -1;

        cout << "Chosen terminals: ";
        for (NodeId t : terminals)
        {
            cout << t << " ";
        }
        cout << "\n";

        if (selection.heuristic)
        {
            cout << "\n--- Heuristic ---\n";
            heuristic_k = test_heuristic(base_graph, terminals, time_limit_ms);
        }

        if (selection.parallel_heuristic)
        {
            cout << "\n--- Parallel Heuristic ---\n";
            test_parallel_heuristic(base_graph, terminals, time_limit_ms);
        }

        if (selection.approximation)
        {
            cout << "\n--- Approximation ---\n";
            approx_k = test_approximation(base_graph, terminals, time_limit_ms);
        }

        if (selection.exact)
        {
            if (heuristic_k < 0)
            {
                cout << "\n--- Heuristic (required for exact upper bound) ---\n";
                heuristic_k = test_heuristic(base_graph, terminals, time_limit_ms);
            }

            if (approx_k < 0)
            {
                cout << "\n--- Approximation (required for exact upper bound) ---\n";
                approx_k = test_approximation(base_graph, terminals, time_limit_ms);
            }

            if (heuristic_k < 0 && approx_k < 0)
            {
                cout << "Skipping exact because no valid upper bound was found.\n";
                continue;
            }

            cout << "\n--- Exact ---\n";
            test_exact(base_graph, terminals, approx_k, heuristic_k, time_limit_ms);
        }
    }
}

// Let the user choose a time limit for the tests.
int choose_time_limit_ms()
{
    while (true)
    {
        cout << "\nChoose time limit:\n";
        cout << "1. 10 minutes\n";
        cout << "2. 2 minutes\n";
        cout << "3. 3 minutes\n";
        cout << "4. 5 minutes\n";
        cout << "5. 7 minutes\n";
        cout << "Choice: ";

        int choice;
        cin >> choice;

        switch (choice)
        {
        case 1:
            return 600000;
        case 2:
            return 120000;
        case 3:
            return 180000;
        case 4:
            return 300000;
        case 5:
            return 420000;
        default:
            cout << "Invalid choice.\n";
        }
    }
}

// for running user tests
void independent_tests_menu()
{
    while (true)
    {
        cout << "\nIndependent Tests Menu\n";
        cout << "1. Run a comparitive Analysis\n";
        cout << "2. Run heuristic\n";
        cout << "3. Run parallel heuristic\n";
        cout << "4. Run approximation\n";
        cout << "5. Run exact\n";
        cout << "0. Back\n";
        cout << "Choice: ";

        int choice;
        cin >> choice;

        if (choice == 0)
        {
            return;
        }

        Graph base_graph = load_selected_graph();

        TestSelection selection;
        int time_limit_ms = choose_time_limit_ms();
        vector<int> terminal_sizes = choose_terminal_sizes();

        switch (choice)
        {
        case 1:
            selection.heuristic = true;
            selection.parallel_heuristic = true;
            selection.approximation = true;
            selection.exact = true;
            break;
        case 2:
            selection.heuristic = true;
            break;
        case 3:
            selection.parallel_heuristic = true;
            break;
        case 4:
            selection.approximation = true;

            break;
        case 5:
            selection.exact = true;
            break;

        default:
            cout << "Invalid choice.\n";
        }
        run_selected_independent_tests(selection, base_graph, terminal_sizes, time_limit_ms);
    }
}

void benchmark_menu()
{
    while (true)
    {
        cout << "\nBenchmark Menu\n";
        cout << "1. Run all tracks\n";
        cout << "2. Run Track1 only\n";
        cout << "3. Run Track2 only\n";
        cout << "4. Run Track3 only\n";
        cout << "5. Run for grid only\n";
        cout << "0. Back\n";
        cout << "Choice: ";

        int choice;
        cin >> choice;

        switch (choice)
        {
        case 1:
            // 2 min, 3 min, 5 min
            Benchmark::run_benchmark_pace_graphs(120000, 180000, 300000);
            break;
        case 2:
            Benchmark::run_benchmark_small(240000);
            break;
        case 3:
            Benchmark::run_benchmark_medium(420000);
            break;
        case 4:
            Benchmark::run_benchmark_large(600000);
            break;
        case 5:
            Benchmark::run_benchmark_grid_graph();
        case 0:
            return;
        default:
            cout << "Invalid choice.\n";
        }
    }
}

int run_test_menu()
{
    while (true)
    {
        cout << "\nMain Menu\n";
        cout << "1. Run independent tests on smaller graphs\n";
        cout << "2. Run benchmark\n";
        cout << "0. Exit\n";
        cout << "Choice: ";

        int choice;
        cin >> choice;

        switch (choice)
        {
        case 1:
            independent_tests_menu();
            break;
        case 2:
            benchmark_menu();
            break;
        case 0:
            cout << "Exiting.\n";
            return 0;
        default:
            cout << "Invalid choice.\n";
        }
    }
}
