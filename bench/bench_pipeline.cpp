#include <benchmark/benchmark.h>

static void BM_NoOpPipeline(benchmark::State& state) {
    for (auto _ : state) { /* no-op */ }
}
BENCHMARK(BM_NoOpPipeline);
