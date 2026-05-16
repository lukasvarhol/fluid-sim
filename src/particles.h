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
#include "objects3d/object_collision.h"

#ifdef USE_TBB
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#else
#include <execution>
#endif
#include <algorithm>
#include <random>

extern bool isBenchmarking;
extern int currentFrame;
extern bool runParallel;

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

  std::vector<Vec3>  positions;
  std::vector<Vec3>  predictedPositions;
  std::vector<Vec3>  velocities;
  std::vector<float> densities;
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

  float h2, h5, h8, poly6, spiky, wDq;

  Particles(int numParticles, float smoothingRadius);
  void Update(float dt, float smoothingRadius, float radiusPx,
                const int g_fb_w,
	      const int g_fb_h, Vec3 rayOrigin, Vec3 rayDir, float mouseStrength, const std::vector<OBBCollider>& obbs);
  void Reset(float smoothingRadius);
  void ResizeParticles(int newParticles, float smoothingRadius, float spacing, float ox, float oy, float oz);
  void ResetTrickler();

private:
  int   numCells1D;
  float restDensity;
  std::mt19937 rng{std::random_device{}()};

  std::vector<int> gridData;   
  std::vector<int> gridStart;  
  std::vector<int> gridCount;  

  void  ClampToBoundaries(Vec3* pos, float radiusPx, const int g_fb_w, const int g_fb_h);
  void  InitialiseParticles(int numParticles, float spacing);
  float CalculateDistance(Vec3 posA, Vec3 posB);
  float CalculateDensity(size_t particleIdx, float smoothingRadius);
  Cell  PositionToCoord(Vec3 position, float smoothingRadius);
  void  BuildGrid(float smoothingRadius);
  void  BuildNeighbours(float smoothingRadius);
  float CalculateLambda(size_t particleIdx, float smoothingRadius);
  float EstimateRestDensity(float smoothingRadius);
  float Scorr(Vec3 pi, Vec3 pj, float h);
  bool  NeedsNeighbourRebuild();
  void  TickTrickler(float dt);
  inline int CellIndex(int cx, int cy, int cz) const;
};
