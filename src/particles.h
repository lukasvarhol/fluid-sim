#pragma once
#include <vector>

namespace physics3d { class ObstacleSystem; }
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
    std::vector<Vec3>  positions;
    std::vector<Vec3>  predictedPositions;
    std::vector<Vec3>  velocities;
    std::vector<float> densities;
    std::vector<Vec3>  colors;
    std::vector<float> allLambdas;
    std::vector<Vec3>  deltas;
    std::vector<Vec3>  oldPositions;
    std::vector<Vec3> vorticity;
    std::vector<float> omegaMag;  

    std::vector<int> neighbourData;   // size nParticles * MAX_NEIGHBOURS
    std::vector<int> neighbourCount;  // size nParticles 
    std::vector<int> indices;
    std::vector<Vec3> positionsAtLastBuild;
    float skinRadius;
    bool needsRebuild = true;

    float h2, h5, h8, poly6_norm, spiky_norm, W_dq;

    Particles(int nParticles, float smoothingRadius);
    void update(float dt, float smoothingRadius, float radiusPx,
            const int g_fb_w, const int g_fb_h,
            Vec3 rayOrigin, Vec3 rayDir, float mouseStrength,
            physics3d::ObstacleSystem* obstacles = nullptr);
    void reset(float smoothingRadius);
  void resizeParticles(int nNewParticles, float fSmoothingRadius, float spacing, float ox, float oy, float oz);

private:
    int   nCells1D;
    float restDensity;

    std::vector<int> gridData;   
    std::vector<int> gridStart;  
    std::vector<int> gridCount;  

  void  clampToBoundaries(Vec3* pos, float radiusPx, const int g_fb_w, const int g_fb_h);
  void  initialiseParticles(int nParticles, float spacing);
  float spikyKernelGrad(float smoothingRadius, float distance);
  Vec3  spikyKernelGradVec(Vec3 a, Vec3 b, float smoothingRadius);
  float poly6Kernel(float smoothingRadius, float distance);
  float calculateDistance(Vec3 posA, Vec3 posB);
  float calculateDensity(size_t particleIdx, float smoothingRadius);
  Cell  positionToCoord(Vec3 position, float smoothingRadius);
  void  buildGrid(float smoothingRadius);
  void  buildNeighbours(float smoothingRadius);
  float calculateLambda(size_t particleIdx, float smoothingRadius);
  float estimateRestDensity(float smoothingRadius);
  float scorr(Vec3 pi, Vec3 pj, float h);
  Vec3  getColor(Vec3& vel);
  bool needsNeighbourRebuild();
  inline int cellIndex(int cx, int cy, int cz) const;
  
};
