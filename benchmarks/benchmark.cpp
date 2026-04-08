#include <benchmark/benchmark.h>
#include "particles.h"

static constexpr float FB_W = 1920.0f;
static constexpr float FB_H = 1080.0f;
static constexpr float DT = 1.0f / 360.0f;
static constexpr float SMOOTHING_R = 0.07f;
static constexpr float RADIUS_PX = 7.0f;

// ---------------------------------------------------------------------------
// BM_ParticlesUpdate
// Isolates the simulation step. Scales N to expose O(n) vs O(n^2).
// ---------------------------------------------------------------------------
static void BM_ParticlesUpdate(benchmark::State& state)
{
    const int N = static_cast<int>(state.range(0));
    Particles p(static_cast<unsigned int>(N), SMOOTHING_R);

    // warm-up: let the sim settle before timing
    for (int i = 0; i < 60; ++i)
        p.update(DT, SMOOTHING_R, RADIUS_PX, FB_W, FB_H);

    for (auto _ : state)
    {
        p.update(DT, SMOOTHING_R, RADIUS_PX, FB_W, FB_H);
        benchmark::DoNotOptimize(p.positions.data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(N));
}

BENCHMARK(BM_ParticlesUpdate)
->Arg(200)
->Arg(500)
->Arg(1000)
->Arg(2000)
->Arg(4000)
->Unit(benchmark::kMillisecond);

// ---------------------------------------------------------------------------
// BM_MainFrame_CPUOnly
// Simulates a full frame at 60fps with a fixed-step accumulator.
// The matrix math is intentionally kept to avoid the optimizer eliminating
// the position reads -- but it no longer calls create_matrix_transform
// (which expects Vec3) on a Vec2.
// ---------------------------------------------------------------------------
static void BM_MainFrame_CPUOnly(benchmark::State& state)
{
    const int N = static_cast<int>(state.range(0));
    Particles p(static_cast<unsigned int>(N), SMOOTHING_R);

    static float accumulator = 0.0f;
    constexpr float FIXED_DT = 1.0f / 280.0f;
    const float     dt_to_sim = 1.0f / 60.0f;

    // warm-up
    for (int i = 0; i < 120; ++i)
    {
        accumulator += dt_to_sim;
        while (accumulator >= FIXED_DT)
        {
            p.update(FIXED_DT, SMOOTHING_R, RADIUS_PX, FB_W, FB_H);
            accumulator -= FIXED_DT;
        }
    }
    accumulator = 0.0f;

    for (auto _ : state)
    {
        accumulator += dt_to_sim;
        int updates = 0;
        while (accumulator >= FIXED_DT)
        {
            p.update(FIXED_DT, SMOOTHING_R, RADIUS_PX, FB_W, FB_H);
            accumulator -= FIXED_DT;
            ++updates;
        }

        // sink: prevent optimizer from discarding position/color reads
        float sink = 0.0f;
        for (size_t i = 0; i < p.positions.size(); ++i)
        {
            sink += p.positions[i].x + p.positions[i].y
                + p.colors[i].x;
        }
        benchmark::DoNotOptimize(sink);
        benchmark::DoNotOptimize(updates);
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(N));
}

BENCHMARK(BM_MainFrame_CPUOnly)
->Arg(200)
->Arg(500)
->Arg(1000)
->Arg(2000)
->Arg(4000)
->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();