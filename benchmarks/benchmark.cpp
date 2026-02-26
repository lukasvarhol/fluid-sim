#include <benchmark/benchmark.h>
#include "particles.h"

static constexpr float FB_W = 1920.0f;
static constexpr float FB_H = 1080.0f;
static constexpr float DT   = 1.0f / 360.0f;
static constexpr unsigned int RADIUS_PX = 7;

struct FrameCPUResult {
    float sink;     
    int updates;    
};

static FrameCPUResult FrameCPUOnly(
    Particles& particles,
    float dt_to_sim,
    int fb_w,
    int fb_h,
    float radius_px
) {
    static float accumulator = 0.0f;
    constexpr float FIXED_DT = 1.0f / 280.0f;

    accumulator += dt_to_sim;

    int updates = 0;
    while (accumulator >= FIXED_DT) {
        particles.update(FIXED_DT, fb_w, fb_h);
        accumulator -= FIXED_DT;
        ++updates;
    }

    float sx = (2.0f * radius_px) / (float)fb_w;
    float sy = (2.0f * radius_px) / (float)fb_h;
    Vec3 quad_scaling = {sx, sy, 1.0f};

    float sink = 0.0f;
    for (size_t i = 0; i < particles.positions.size(); ++i) {
        Mat4 model = Mat4_multiply(
            create_matrix_transform(particles.positions[i]),
            create_matrix_scaling(quad_scaling)
        );
        sink += model.entries[0] + particles.colors[i].x;
    }

    return {sink, updates};
}

static void BM_MainFrame_CPUOnly(benchmark::State& state) {
    const int N = (int)state.range(0);
    Particles p((unsigned)N, /*radius_px=*/7);

    const float radius_px = 7.0f;
    const float dt_to_sim = 1.0f / 60.0f; 

    for (int i = 0; i < 120; ++i) {
        auto r = FrameCPUOnly(p, dt_to_sim, FB_W, FB_H, radius_px);
        benchmark::DoNotOptimize(r.sink);
    }

    for (auto _ : state) {
        auto r = FrameCPUOnly(p, dt_to_sim, FB_W, FB_H, radius_px);
        benchmark::DoNotOptimize(r.sink);
        benchmark::DoNotOptimize(r.updates);
    }

    state.SetItemsProcessed(state.iterations() * (int64_t)N);
}


static void BM_ParticlesUpdate(benchmark::State& state) {
    const int N = static_cast<int>(state.range(0));

    Particles p(static_cast<unsigned int>(N), RADIUS_PX);

    for (int i = 0; i < 60; ++i) {
        p.update(DT, FB_W, FB_H);
    }

    for (auto _ : state) {
        p.update(DT, FB_W, FB_H);
        benchmark::DoNotOptimize(p); 
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(N));
}

BENCHMARK(BM_ParticlesUpdate)
    ->Arg(200)
    ->Arg(500)
    ->Arg(1000)
    ->Arg(2000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_MainFrame_CPUOnly)
  ->Arg(200)->Arg(500)->Arg(1000)->Arg(2000)
  ->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();