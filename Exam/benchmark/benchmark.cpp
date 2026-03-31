#include "benchmark.hpp"

#include "approximation.hpp"
#include "exact.hpp"
#include "file_handler.hpp"
#include "heuristic.hpp"
#include "parallel_heuristic.hpp"
#include "grid_generator.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <queue>
#include <set>
#include <string>
#include <vector>

using namespace std;

namespace
{

    namespace fs = filesystem;

    constexpr int kDefaultTimeLimitMs = 120000;

    string bool_to_csv(bool value)
    {
        return value ? "1" : "0";
    }

    double elapsed_ms(chrono::steady_clock::time_point start,
                      chrono::steady_clock::time_point end)
    {
        return chrono::duration<double, milli>(end - start).count();
    }

    bool has_gr_extension(const fs::path &path)
    {
        return path.has_extension() && path.extension() == ".gr";
    }

    vector<fs::path> list_gr_files(const string &directory)
    {
        vector<fs::path> files;
        for (const auto &entry : fs::directory_iterator(directory))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }
            if (has_gr_extension(entry.path()))
            {
                files.push_back(entry.path());
            }
        }
        sort(files.begin(), files.end());
        return files;
    }

    void fill_run_fields(BenchmarkResult &bench,
                         const RunResult &run,
                         const string &algorithm_name)
    {
        const int cut_size = run.cut_found ? static_cast<int>(run.cut_nodes.size()) : -1;

        if (algorithm_name == "heuristic")
        {
            bench.heuristic_execution_time_ms = run.execution_time_ms;
            bench.heuristic_cut_size = cut_size;
            bench.heuristic_valid = run.valid_cut;
            bench.heuristic_timed_out = run.timed_out;
            return;
        }

        if (algorithm_name == "parallel_heuristic")
        {
            bench.parallel_heuristic_execution_time_ms = run.execution_time_ms;
            bench.parallel_heuristic_cut_size = cut_size;
            bench.parallel_heuristic_valid = run.valid_cut;
            bench.parallel_heuristic_timed_out = run.timed_out;
            return;
        }

        if (algorithm_name == "approximation")
        {
            bench.approximation_execution_time_ms = run.execution_time_ms;
            bench.approximation_cut_size = cut_size;
            bench.approximation_valid = run.valid_cut;
            bench.approximation_timed_out = run.timed_out;
            return;
        }

        if (algorithm_name == "exact")
        {
            bench.exact_execution_time_ms = run.execution_time_ms;
            bench.exact_best_time_ms = run.exact_best_time == -1 ? run.execution_time_ms : run.exact_best_time;
            bench.exact_cut_size = cut_size;
            bench.exact_valid = run.valid_cut;
            bench.exact_timed_out = run.timed_out;
        }
    }

    int best_upper_bound_from(const BenchmarkResult &result)
    {
        int upper_bound = numeric_limits<int>::max();

        if (result.heuristic_cut_size >= 0)
        {
            upper_bound = min(upper_bound, result.heuristic_cut_size);
        }
        if (result.parallel_heuristic_cut_size >= 0)
        {
            upper_bound = min(upper_bound, result.parallel_heuristic_cut_size);
        }
        if (result.approximation_cut_size >= 0)
        {
            upper_bound = min(upper_bound, result.approximation_cut_size);
        }

        if (upper_bound == numeric_limits<int>::max())
        {
            return -1;
        }
        return upper_bound;
    }

    vector<BenchmarkResult> run_track_benchmark(const string &track_name,
                                                const string &track_directory,
                                                const string &csv_filename,
                                                int time_limit_ms)
    {
        vector<BenchmarkResult> results;
        const vector<fs::path> instances = list_gr_files(track_directory);

        // for (const auto& instance_path : instances) {
        for (size_t idx = 0; idx < instances.size(); ++idx)
        {
            const auto &instance_path = instances[idx];

            cout << "Benchmarking " << track_name << " instance "
                 << instance_path.filename().string() << '\n';

            GraphInstance instance = FileHandler::readPaceGraph(instance_path.string());

            BenchmarkResult bench;
            bench.instance_name = instance_path.filename().string();
            bench.track_name = track_name;
            bench.num_nodes = instance.graph.number_of_nodes();
            bench.num_edges = instance.graph.number_of_edges() / 2;
            bench.num_terminals = static_cast<int>(instance.terminals.size());

            cout << "--------------------------------------------------" << endl;
            cout << "Running Heuristic..." << endl;

            fill_run_fields(bench,
                            Benchmark::run_heuristic(instance.graph, instance.terminals, time_limit_ms),
                            "heuristic");

            cout << "--------------------------------------------------" << endl;
            cout << "Running Parallel Heuristic..." << endl;

            fill_run_fields(bench,
                            Benchmark::run_parallel_heuristic(instance.graph, instance.terminals, time_limit_ms),
                            "parallel_heuristic");

            if (track_name != "Track3")
            {
                cout << "--------------------------------------------------" << endl;
                cout << "Running Approximation" << endl;

                fill_run_fields(bench,
                                Benchmark::run_approximation(instance.graph, instance.terminals, time_limit_ms),
                                "approximation");

                const int upper_bound = best_upper_bound_from(bench);
                if (upper_bound >= 0)
                {
                    cout << "--------------------------------------------------" << endl;
                    cout << "Running Exact..." << endl;
                    fill_run_fields(bench,
                                    Benchmark::run_exact(instance.graph,
                                                         instance.terminals,
                                                         upper_bound,
                                                         upper_bound,
                                                         time_limit_ms),
                                    "exact");
                }
            }

            Benchmark::calculate_ratios(bench);
            results.push_back(bench);

            cout << "========================================================" << endl;

            if ((idx + 1) % 2 == 0)
            {
                Benchmark::write_to_csv(results, csv_filename);
                results.clear();
            }
        }

        if (!results.empty())
        {
            Benchmark::write_to_csv(results, csv_filename);
            results.clear();
        }

        return results;
    }

} // namespace

bool Benchmark::test_validity(const Graph &graph,
                              const vector<NodeId> &cutNodes,
                              const vector<NodeId> &terminals)
{
    set<NodeId> cut_set(cutNodes.begin(), cutNodes.end());
    for (NodeId t : terminals)

    {
        if (cut_set.count(t))
        {
            cout << "Invalid cut: terminal " << t << " is included in the cut.\n";
            return false;
        }
    }
    for (size_t i = 0; i < terminals.size(); ++i)
    {
        for (size_t j = i + 1; j < terminals.size(); ++j)
        {
            const NodeId t1 = terminals[i];
            const NodeId t2 = terminals[j];

            // if (cut_set.count(t1) > 0 || cut_set.count(t2) > 0) {
            //     continue;
            // }

            vector<bool> visited(graph.number_of_nodes(), false);
            vector<NodeId> parent(graph.number_of_nodes(), INVALID_NODE);
            queue<NodeId> q;

            q.push(t1);
            visited[t1] = true;

            bool connected = false;

            while (!q.empty())
            {
                const NodeId current = q.front();
                q.pop();

                if (current == t2)
                {
                    connected = true;
                    break;
                }

                for (EdgeId eid : graph.get_neighbors(current))
                {
                    const Edge &e = graph.get_edge(eid);
                    const NodeId neighbor = e.trg;

                    if (!visited[neighbor] && cut_set.count(neighbor) == 0)
                    {
                        visited[neighbor] = true;
                        parent[neighbor] = current;
                        q.push(neighbor);
                    }
                }
            }

            if (connected)
            {
                cout << "Invalid cut: terminals " << t1
                     << " and " << t2
                     << " are still connected.\n";

                vector<NodeId> path;
                for (NodeId cur = t2; cur != INVALID_NODE; cur = parent[cur])
                {
                    path.push_back(cur);
                }
                reverse(path.begin(), path.end());

                cout << "Path: ";
                for (NodeId v : path)
                {
                    cout << v << " ";
                }
                cout << "\n";

                return false;
            }
        }
    }

    cout << "Cut is valid: all terminal pairs are separated.\n";
    return true;
}

// Run the approximation algorithm and return the results in a RunResult struct. The approximation algorithm is expected to run faster than the exact algorithm, so it should be given the same time limit as the exact algorithm.
void Benchmark::calculate_ratios(BenchmarkResult &result)
{
    if (result.num_nodes > 1)
    {
        result.graph_density =
            (2.0 * result.num_edges) / (result.num_nodes * (result.num_nodes - 1.0));
    }

    if (result.exact_cut_size > 0)
    {
        if (result.heuristic_cut_size > 0)
        {
            result.heuristic_exact_ratio =
                static_cast<double>(result.heuristic_cut_size) / result.exact_cut_size;
        }
        if (result.parallel_heuristic_cut_size > 0)
        {
            result.parallel_heuristic_exact_ratio =
                static_cast<double>(result.parallel_heuristic_cut_size) / result.exact_cut_size;
        }
        if (result.approximation_cut_size > 0)
        {
            result.approximation_exact_ratio =
                static_cast<double>(result.approximation_cut_size) / result.exact_cut_size;
        }
    }

    if (result.num_nodes > 0)
    {
        if (result.heuristic_cut_size > 0)
        {
            result.cut_to_node_heuristic_ratio =
                static_cast<double>(result.heuristic_cut_size) / result.num_nodes;
        }
        if (result.parallel_heuristic_cut_size > 0)
        {
            result.cut_to_node_parallel_heuristic_ratio =
                static_cast<double>(result.parallel_heuristic_cut_size) / result.num_nodes;
        }
        if (result.approximation_cut_size > 0)
        {
            result.cut_to_node_approximation_ratio =
                static_cast<double>(result.approximation_cut_size) / result.num_nodes;
        }
        if (result.exact_cut_size > 0)
        {
            result.cut_to_node_exact_ratio =
                static_cast<double>(result.exact_cut_size) / result.num_nodes;
        }
    }
}

// Run the approximation algorithm and return the results in a RunResult struct. The approximation algorithm is expected to run faster than the exact algorithm, so it should be given the same time limit as the exact algorithm.
RunResult Benchmark::run_approximation(Graph graph,
                                       const vector<NodeId> &terminals,
                                       int time_limit_ms)
{
    RunResult result;
    Approximation approximation(graph, terminals);

    const auto start = chrono::steady_clock::now();
    optional<vector<NodeId>> cut = approximation.run(time_limit_ms);
    const auto end = chrono::steady_clock::now();

    result.execution_time_ms = elapsed_ms(start, end);
    result.timed_out = approximation.did_timeout();
    result.cut_found = cut.has_value();

    if (result.cut_found)
    {
        result.cut_nodes = *cut;
        result.valid_cut = Benchmark::test_validity(graph, result.cut_nodes, terminals);
    }

    return result;
}

RunResult Benchmark::run_exact(Graph graph,
                               const vector<NodeId> &terminals,
                               int k_approx,
                               int M,
                               int time_limit_ms)
{
    RunResult result;
    Exact exact(graph, terminals);

    const auto start = chrono::steady_clock::now();
    optional<vector<NodeId>> cut = exact.run(k_approx, M, time_limit_ms);
    const auto end = chrono::steady_clock::now();

    result.execution_time_ms = elapsed_ms(start, end);
    result.timed_out = exact.did_timeout();
    result.cut_found = cut.has_value();

    if (result.cut_found)
    {
        result.cut_nodes = *cut;
        result.exact_best_time = exact.get_best_cut_time_ms();
        result.valid_cut = Benchmark::test_validity(graph, result.cut_nodes, terminals);
    }

    return result;
}

// Run the heuristic and return the results in a RunResult struct. The heuristic is expected to run faster than the exact algorithm, so it should be given the same time limit as the exact algorithm.
RunResult Benchmark::run_heuristic(Graph graph,
                                   const vector<NodeId> &terminals,
                                   int time_limit_ms)
{
    RunResult result;
    Heuristic heuristic(graph, terminals);

    const auto start = chrono::steady_clock::now();
    optional<vector<NodeId>> cut = heuristic.run(time_limit_ms);
    const auto end = chrono::steady_clock::now();

    result.execution_time_ms = elapsed_ms(start, end);
    result.timed_out = heuristic.did_timeout();
    result.cut_found = cut.has_value();

    if (result.cut_found)
    {
        result.cut_nodes = *cut;
        result.valid_cut = Benchmark::test_validity(graph, result.cut_nodes, terminals);
    }

    return result;
}

// Run parallel heuristic and return the results in a RunResult struct. The parallel heuristic is expected to run faster than the regular heuristic, so it should be given the same time limit as the regular heuristic.
RunResult Benchmark::run_parallel_heuristic(Graph graph,
                                            const vector<NodeId> &terminals,
                                            int time_limit_ms)
{

    RunResult result;
    ParallelHeuristic parallel_heuristic(graph, terminals);

    const auto start = chrono::steady_clock::now();
    optional<vector<NodeId>> cut = parallel_heuristic.run(time_limit_ms);
    const auto end = chrono::steady_clock::now();

    result.execution_time_ms = elapsed_ms(start, end);
    result.timed_out = parallel_heuristic.did_timeout();
    result.cut_found = cut.has_value();
    if (result.cut_found)
    {
        result.cut_nodes = *cut;
        result.valid_cut = Benchmark::test_validity(graph, result.cut_nodes, terminals);
    }
    return result;
}

// Writes the given benchmark results to a CSV file. If the file doesn't exist, it creates it and writes the header. If it already exists, it appends to it.
void Benchmark::write_to_csv(const vector<BenchmarkResult> &results,
                             const string &filename)
{
    bool write_header = false;

    {
        ifstream in(filename);
        write_header = !in.good() || in.peek() == ifstream::traits_type::eof();
    }

    ofstream out(filename, ios::app);
    if (!out)
    {
        cerr << "Failed to open CSV file: " << filename << '\n';
        return;
    }
    // I want out put in out put folder ../output_data/track1_results.csv
    cout << "Writing CSV to: " << fs::absolute(filename) << '\n';

    if (write_header)
    {
        out << "instance_name,track_name,num_nodes,num_edges,num_terminals,"
            << "graph_density,"
            << "heuristic_execution_time_ms,heuristic_cut_size,heuristic_valid,heuristic_timed_out,"
            << "parallel_heuristic_execution_time_ms,parallel_heuristic_cut_size,parallel_heuristic_valid,parallel_heuristic_timed_out,"
            << "approximation_execution_time_ms,approximation_cut_size,approximation_valid,approximation_timed_out,"
            << "exact_execution_time_ms,exact_best_time,exact_cut_size,exact_valid,exact_timed_out,"
            << "heuristic_exact_ratio,parallel_heuristic_exact_ratio,approximation_exact_ratio,"
            << "cut_to_node_heuristic_ratio,cut_to_node_parallel_heuristic_ratio,cut_to_node_approximation_ratio,cut_to_node_exact_ratio\n";
    }
    out << fixed << setprecision(6);
    for (const auto &result : results)
    {
        out << result.instance_name << ','
            << result.track_name << ','
            << result.num_nodes << ','
            << result.num_edges << ','
            << result.num_terminals << ','
            << result.graph_density << ','
            << result.heuristic_execution_time_ms << ','
            << result.heuristic_cut_size << ','
            << bool_to_csv(result.heuristic_valid) << ','
            << bool_to_csv(result.heuristic_timed_out) << ','
            << result.parallel_heuristic_execution_time_ms << ','
            << result.parallel_heuristic_cut_size << ','
            << bool_to_csv(result.parallel_heuristic_valid) << ','
            << bool_to_csv(result.parallel_heuristic_timed_out) << ','
            << result.approximation_execution_time_ms << ','
            << result.approximation_cut_size << ','
            << bool_to_csv(result.approximation_valid) << ','
            << bool_to_csv(result.approximation_timed_out) << ','
            << result.exact_execution_time_ms << ','
            << result.exact_best_time_ms << ','
            << result.exact_cut_size << ','
            << bool_to_csv(result.exact_valid) << ','
            << bool_to_csv(result.exact_timed_out) << ','
            << result.heuristic_exact_ratio << ','
            << result.parallel_heuristic_exact_ratio << ','
            << result.approximation_exact_ratio << ','
            << result.cut_to_node_heuristic_ratio << ','
            << result.cut_to_node_parallel_heuristic_ratio << ','
            << result.cut_to_node_approximation_ratio << ','
            << result.cut_to_node_exact_ratio << '\n';
    }
}

void Benchmark::run_benchmark_small(int time_limit_ms)
{
    const string directory = "../input_data/SteinerTree-PACE-2018-instances/Track1";
    const vector<BenchmarkResult> results =
        run_track_benchmark("Track1", directory, "../output_data/track1_results.csv", time_limit_ms);
}

void Benchmark::run_benchmark_medium(int time_limit_ms)
{
    const string directory = "../input_data/SteinerTree-PACE-2018-instances/Track2";
    const vector<BenchmarkResult> results =
        run_track_benchmark("Track2", directory, "../output_data/track2_results.csv", time_limit_ms);
}

void Benchmark::run_benchmark_large(int time_limit_ms)
{
    const string directory = "../input_data/SteinerTree-PACE-2018-instances/Track3";
    const vector<BenchmarkResult> results =
        run_track_benchmark("Track3", directory, "../output_data/track3_results.csv", time_limit_ms);
}

void Benchmark::run_benchmark_pace_graphs(int small_time_limit_ms, int medium_time_limit_ms, int large_time_limit_ms)
{
    run_benchmark_small(small_time_limit_ms);
    run_benchmark_medium(medium_time_limit_ms);
    run_benchmark_large(large_time_limit_ms);
}


BenchmarkResult Benchmark::run_single_generated_instance(const GraphInstance &instance,
                                                         const string &instance_name,
                                                         const string &track_name,
                                                         int time_limit_ms,
                                                         bool run_exact_algo,
                                                         bool run_approx_algo)
{
    BenchmarkResult bench;
    bench.instance_name = instance_name;
    bench.track_name = track_name;
    bench.num_nodes = instance.graph.number_of_nodes();
    bench.num_edges = instance.graph.number_of_edges() / 2;
    bench.num_terminals = static_cast<int>(instance.terminals.size());

    cout << "--------------------------------------------------" << endl;
    cout << "Running Heuristic..." << endl;
    fill_run_fields(bench,
                    Benchmark::run_heuristic(instance.graph, instance.terminals, time_limit_ms),
                    "heuristic");

    cout << "--------------------------------------------------" << endl;
    cout << "Running Parallel Heuristic..." << endl;
    fill_run_fields(bench,
                    Benchmark::run_parallel_heuristic(instance.graph, instance.terminals, time_limit_ms),
                    "parallel_heuristic");

    if (run_approx_algo)
    {
        cout << "--------------------------------------------------" << endl;
        cout << "Running Approximation..." << endl;
        fill_run_fields(bench,
                        Benchmark::run_approximation(instance.graph, instance.terminals, time_limit_ms),
                        "approximation");
    }

    if (run_exact_algo)
    {
        const int upper_bound = best_upper_bound_from(bench);
        if (upper_bound >= 0)
        {
            cout << "--------------------------------------------------" << endl;
            cout << "Running Exact..." << endl;
            fill_run_fields(bench,
                            Benchmark::run_exact(instance.graph,
                                                 instance.terminals,
                                                 upper_bound,
                                                 upper_bound,
                                                 time_limit_ms),
                            "exact");
        }
    }

    Benchmark::calculate_ratios(bench);
    return bench;
}

// bool has_edge_between(const Graph& graph, NodeId u, NodeId v) {
//     const vector<NodeId>& neighbors = graph.get_neighboring_nodes(u);
//     return find(neighbors.begin(), neighbors.end(), v) != neighbors.end();
// }

void Benchmark::run_benchmark_grid_graph()
{
    const string csv_filename = "../output_data/grid_output.csv";
    vector<BenchmarkResult> results;

    // smaller sizes first because exact may become expensive
    const vector<pair<int, int>> grid_sizes = {
        {10, 10}, // 100
        {20, 25}, // 500
        {25, 40}, // 1000
        {30, 50}, // 1500
        {40, 50}  // 2000
    };

    const vector<int> terminal_counts = {3, 5, 10, 20, 30, 50};
    const int instances_per_setting = 5;
    const int time_limit_ms = 120000;

    unsigned int seed_base = 42;

    for (const auto &[rows, cols] : grid_sizes)
    {
        const int n = rows * cols;

        for (int k : terminal_counts)
        {
            if (k >= n)
            {
                continue;
            }

            for (int rep = 0; rep < instances_per_setting; ++rep)
            {
                unsigned int seed = seed_base + rows * 10000 + cols * 100 + k * 10 + rep;

                auto instance_opt = GridGenerator::generate_grid_instance(rows, cols, k, seed);

                if (!instance_opt.has_value())
                {
                    cout << "Skipping grid " << rows << "x" << cols
                         << " with k=" << k
                         << " because not enough non-adjacent terminals could be chosen.\n";
                    continue;
                }

                GraphInstance instance = *instance_opt;
                const string instance_name =
                    "grid_" + to_string(rows) + "x" + to_string(cols) +
                    "_k" + to_string(k) +
                    "_rep" + to_string(rep);

                for (size_t i = 0; i < instance.terminals.size(); ++i)
                {
                    for (size_t j = i + 1; j < instance.terminals.size(); ++j)
                    {
                        if (GridGenerator::has_edge_between(instance.graph, instance.terminals[i], instance.terminals[j]))
                        {
                            cout << "ERROR: terminals " << instance.terminals[i]
                                 << " and " << instance.terminals[j]
                                 << " are adjacent!\n";
                        }
                    }
                }
                cout << "Benchmarking " << instance_name << '\n';

                // run exact only on smaller grids
                bool run_exact_algo = true;
                bool run_approx_algo = true;

                BenchmarkResult bench =
                    run_single_generated_instance(instance,
                                                  instance_name,
                                                  "Grid",
                                                  time_limit_ms,
                                                  run_exact_algo,
                                                  run_approx_algo);

                results.push_back(bench);

                if (results.size() >= 10)
                {
                    Benchmark::write_to_csv(results, csv_filename);
                    results.clear();
                }
            }
        }
    }

    if (!results.empty())
    {
        Benchmark::write_to_csv(results, csv_filename);
    }
}

