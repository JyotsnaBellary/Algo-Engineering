#include "benchmark.hpp"

int main() {
    // run small with 5 minutes, medium with 10 minutes, large with 20 minutes
    Benchmark::run_benchmark_pace_graphs(90000, 150000, 210000);
    return 0;
}
