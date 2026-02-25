#include <benchmark/benchmark.h>
#include "particles.h"

// Pick fixed framebuffer dims so boundary math is consistent.
static constexpr float FB_W = 1920.0f;
static constexpr float FB_H = 1080.0f;
static constexpr float DT   = 1.0f / 360.0f;
static constexpr unsigned int RADIUS_PX = 7;

static void BM_ParticlesUpdate(benchmark::State& state) {
    const int N = static_cast<int>(state.range(0));

    Particles p(static_cast<unsigned int>(N), RADIUS_PX);

    // Optional: warm up a little to avoid cold-start effects
    for (int i = 0; i < 60; ++i) {
        p.update(DT, FB_W, FB_H);
    }

    for (auto _ : state) {
        p.update(DT, FB_W, FB_H);
        benchmark::DoNotOptimize(p); // prevent whole-call optimization
    }

    // Report throughput as "particles updated per second"
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(N));
}

BENCHMARK(BM_ParticlesUpdate)
    ->Arg(200)
    ->Arg(500)
    ->Arg(1000)
    ->Arg(2000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();