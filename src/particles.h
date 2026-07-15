
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
#include "../benchmark/profiler.h"
#include "objects3d/sdf_collision.h"

#ifdef USE_TBB
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#else
#include <execution>
#endif
#include <algorithm>
#include <random>

#ifdef USE_CUDA
#include <cuda_runtime.h>
#include "cuda_buffers.cuh"
#include "particles.cuh"
#endif

extern bool isBenchmarking;
extern int currentFrame;
extern bool runParallel;

extern bool useTriangleCollisions;
extern std::vector<TriCollider> gTriColliders;
extern std::vector<Vec3> gClosestPoints;

struct Particles
{

  template<typename F>
  void ParallelFor(int n, F&& func) {
#ifdef USE_TBB
    tbb::parallel_for(tbb::blocked_range<int>(0, n),
        [&](const tbb::blocked_range<int>& range) {
            for (int i = range.begin(); i < range.end(); ++i)
                func(i);
        });
#else
    if (!runParallel) {
      std::for_each(indices.begin(), indices.begin() + n,
        [&](int i) { func(i); });
    } else {
      std::for_each(std::execution::par_unseq, indices.begin(), indices.begin() + n,
                    [&](int i) { func(i); });
    }
#endif
  }

  int numParticles;
  int activeParticles;
  int nextRecycleIdx  = 0;
  float tricklerAccum = 0.0f;
  int numCells1D;

  std::vector<Vec3>  positions;
  std::vector<Vec3>  predictedPositions;
  std::vector<Vec3>  velocities;
  std::vector<float> densities;
  std::vector<float> allLambdas;
  std::vector<Vec3>  deltas;
  std::vector<Vec3>  oldPositions;
  std::vector<Vec3> vorticity;
  std::vector<float> omegaMag;

  std::vector<int> gridData;
  std::vector<int> gridStart;
  std::vector<int> gridCount;

  std::vector<int> neighbourData;   // size nParticles * MAX_NEIGHBOURS
  std::vector<int> neighbourCount;  // size nParticles 
  std::vector<int> indices;
  std::vector<Vec3> positionsAtLastBuild;
  float skinRadius;
  bool needsRebuild = true;

  float h2, h5, h8, poly6, spiky, wDq;

  Particles(int numParticles, float smoothingRadius);
  void Update(float dt, float smoothingRadius, float radiusPx,
                const int g_fb_w,
	      const int g_fb_h, Vec3 rayOrigin, Vec3 rayDir, float mouseStrength, SDFCollider* colliders, AppState* as);
  void Reset(float smoothingRadius, AppState* as);
  void ResizeParticles(int newParticles, float smoothingRadius, float spacing, float ox, float oy, float oz, AppState* as);
  void ResetTrickler();

  

private:
  
  float restDensity;
  std::mt19937 rng{std::random_device{}()};


  void  ClampToBoundaries(Vec3* pos, float radiusPx, const int g_fb_w, const int g_fb_h);
  void  InitialiseParticles(int numParticles, float spacing);
  float CalculateDistance(Vec3 posA, Vec3 posB);
  float CalculateDensity(size_t particleIdx, float smoothingRadius);
  void  BuildGrid(float smoothingRadius);
  void  BuildNeighbours(float smoothingRadius);
  float CalculateLambda(size_t particleIdx, float smoothingRadius);
  float EstimateRestDensity(float smoothingRadius);
  bool  NeedsNeighbourRebuild();
  void  TickTrickler(float dt);
};

DEVICE_CALLABLE
inline Cell PositionToCoord(Vec3 position, float smoothingRadius,
                            int numCells1D) {
  int cx = (int)((position.x + 1.0f) / smoothingRadius);
  int cy = (int)((position.y + 1.0f) / smoothingRadius);
  int cz = (int)((position.z + 1.0f) / smoothingRadius);
  cx = max(0, min(numCells1D - 1, cx));
  cy = max(0, min(numCells1D - 1, cy));
  cz = max(0, min(numCells1D - 1, cz));
  return Cell{cx, cy, cz};
}

// Flat 3-D cell index
DEVICE_CALLABLE
inline int CellIndex(int cx, int cy, int cz, int numCells1D)
{
  return cx * numCells1D * numCells1D + cy * numCells1D + cz;
}

DEVICE_CALLABLE
inline float Scorr(Vec3 pi, Vec3 pj, float h2, float poly6, float wdq, float scorr)
{
  Vec3  diff = pi - pj;
  float d2   = diff.Dot(diff);
  if (d2 >= h2) return 0.0f;

  float sq    = h2 - d2;
  float W     = poly6 * sq * sq * sq;
  float ratio = W / wdq;
  float r2    = ratio  * ratio;
  float r4    = r2 * r2;
  return -scorr * r4;
}
