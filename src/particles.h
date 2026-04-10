#pragma once
#include <vector>
#include <iostream>
#include <unordered_map>
#include <array>
#include "linear_algebra.h"
#include "helpers.h"
#include "cell.h"
#include <numeric>
#include "particle_config.h"
#ifdef USE_TBB
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#else
#include <execution>
#endif
#include <algorithm>


struct Particles
{
    template<typename F>
void parallelFor(int n, F&& func) {
#ifdef USE_TBB
    tbb::parallel_for(tbb::blocked_range<int>(0, n),
        [&](const tbb::blocked_range<int>& range) {
            for (int i = range.begin(); i < range.end(); ++i)
                func(i);
        });
#else
    std::for_each(std::execution::par_unseq,
        indices.begin(), indices.end(),
        [&](int i) { func(i); });
#endif
}

    int nParticles;
    std::vector<Vec2>  positions;
    std::vector<Vec2>  predictedPositions;
    std::vector<Vec2>  velocities;
    std::vector<float> densities;
    std::vector<Vec3>  colors;
    std::vector<float> allLambdas;
    std::vector<Vec2>  deltas;
    std::vector<Vec2>  oldPositions;
    std::vector<float> vorticity;
    std::vector<float> omegaMag;  

    std::vector<int> neighbourData;   // size nParticles * MAX_NEIGHBOURS
    std::vector<int> neighbourCount;  // size nParticles 
    std::vector<int> indices;
    std::vector<Vec2> positionsAtLastBuild;
    float skinRadius;
    bool needsRebuild = true;

    float h2, h5, h8, poly6_norm, spiky_norm, W_dq;

    Particles(int nParticles, float smoothingRadius);
    void update(float dt, float smoothingRadius, const float radiusPx,
        const float g_fb_w, const float g_fb_h,
        Vec2 mousePos = { 0.0f, 0.0f }, float mouseStrength = 0.0f);
    void reset(float smoothingRadius);

private:
    int   nCells1D;
    float restDensity;

    std::vector<int> gridData;   
    std::vector<int> gridStart;  
    std::vector<int> gridCount;  

    void  clampToBoundaries(Vec2* pos, const float radiusPx, const float g_fb_w, const float g_fb_h);
    void  initialiseParticles(int nParticles, float spacing);
    float spikyKernelGrad(float smoothingRadius, float distance);
    Vec2  spikyKernelGradVec(Vec2 a, Vec2 b, float smoothingRadius);
    float poly6Kernel(float smoothingRadius, float distance);
    float calculateDistance(Vec2 posA, Vec2 posB);
    float calculateDensity(size_t particleIdx, float smoothingRadius);
    Cell  positionToCoord(Vec2 position, float smoothingRadius);
    void  buildGrid(float smoothingRadius);
    void  buildNeighbours(float smoothingRadius);
    float calculateLambda(size_t particleIdx, float smoothingRadius);
    float estimateRestDensity(float smoothingRadius);
    float scorr(Vec2 pi, Vec2 pj, float h);
    Vec3  getColor(Vec2& vel);
    bool needsNeighbourRebuild();
};
