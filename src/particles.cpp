#include "particles.h"

#include <chrono>
#include <cmath>
#include <algorithm>

// Timing utils for benchmarking
using clk = std::chrono::high_resolution_clock;

auto us = [](auto a, auto b) {
  return std::chrono::duration_cast<std::chrono::microseconds>(b - a).count();
};

Particles::Particles(int n, float smoothingRadius)
  : h2(smoothingRadius * smoothingRadius)
  , h5(h2 * h2 * smoothingRadius)
  , h8(h5 * h2 * smoothingRadius)
  , poly6(315.0f / (64.0f * PI * h8 * smoothingRadius))   // 3-D poly6
  , spiky(-45.0f / (PI * h5 * smoothingRadius))           // 3-D spiky gradient
{
  float dq  = 0.14f * smoothingRadius;
  float sq  = h2 - dq * dq;
  wDq      = poly6 * sq * sq * sq;
  skinRadius = 0.3f * smoothingRadius;

  numParticles = n;
  numCells1D   = (int)std::ceil(2.0f / smoothingRadius);

  const int numCells = numCells1D * numCells1D * numCells1D;   // 3-D grid
  gridData.reserve(numParticles * 4);
  gridStart.resize(numCells + 1, 0);
  gridCount.resize(numCells, 0);
  indices.resize(numParticles);
  std::iota(indices.begin(), indices.end(), 0);

  positions.reserve(numParticles);
  predictedPositions.reserve(numParticles);
  velocities.reserve(numParticles);
  densities.reserve(numParticles);
  allLambdas.resize(numParticles);
  deltas.resize(numParticles);
  oldPositions.resize(numParticles);

  // Vorticity is a 3-D vector field (curl of velocity)
  vorticity.resize(numParticles, Vec3{0.0f, 0.0f, 0.0f});

  neighbourData.resize(numParticles * MAX_NEIGHBOURS);
  neighbourCount.resize(numParticles, 0);

  InitialiseParticles(numParticles, initSpacing);
  BuildGrid(smoothingRadius);
  BuildNeighbours(smoothingRadius);
  positionsAtLastBuild = predictedPositions;
  restDensity = EstimateRestDensity(smoothingRadius);
}

// ---------------------------------------------------------------------------
void Particles::InitialiseParticles(int n, float spacing)
{
  int particlesPerDim   = (int)std::cbrt((float)n);
  int particlesPerLayer = particlesPerDim * particlesPerDim;
  int totalLayers       = (n - 1) / particlesPerLayer + 1;

  for (int i = 0; i < n; ++i) {
    int xIndex = i % particlesPerDim;
    int yIndex = (i / particlesPerDim) % particlesPerDim;
    int zIndex = i / particlesPerLayer;

    float x = ((xIndex - particlesPerDim / 2.0f + 0.5f) * spacing) + initOffsetX;
    float y = ((yIndex - particlesPerDim / 2.0f + 0.5f) * spacing) + initOffsetY;
    float z = ((zIndex - totalLayers   / 2.0f + 0.5f) * spacing) + initOffsetZ;

     positions.push_back(Vec3{x, y, z});
     predictedPositions.push_back(Vec3{x, y, z});
     velocities.push_back(Vec3{0.0f, 0.0f, 0.0f});
     densities.push_back(0.0f);
  }
}

// ---------------------------------------------------------------------------
Cell Particles::PositionToCoord(Vec3 position, float smoothingRadius)
{
  int cx = (int)((position.x + 1.0f) / smoothingRadius);
  int cy = (int)((position.y + 1.0f) / smoothingRadius);
  int cz = (int)((position.z + 1.0f) / smoothingRadius);
  cx = std::max(0, std::min(numCells1D - 1, cx));
  cy = std::max(0, std::min(numCells1D - 1, cy));
  cz = std::max(0, std::min(numCells1D - 1, cz));
  return Cell{cx, cy, cz};
}

// Flat 3-D cell index
inline int Particles::CellIndex(int cx, int cy, int cz) const
{
  return cx * numCells1D * numCells1D + cy * numCells1D + cz;
}

void Particles::BuildGrid(float smoothingRadius)
{
  const int numCells = numCells1D * numCells1D * numCells1D;
  std::fill(gridCount.begin(), gridCount.end(), 0);

  // pass 1: count
  for (int i = 0; i < numParticles; ++i) {
    Cell c = PositionToCoord(predictedPositions[i], smoothingRadius);
    ++gridCount[CellIndex(c.x, c.y, c.z)];
  }

  gridStart[0] = 0;
  for (int c = 0; c < numCells; ++c)
    gridStart[c + 1] = gridStart[c] + gridCount[c];

  // pass 2: fill
  gridData.resize(gridStart[numCells]);
  std::vector<int> insertPos(gridStart.begin(), gridStart.begin() + numCells);

  for (int i = 0; i < numParticles; ++i) {
    Cell c = PositionToCoord(predictedPositions[i], smoothingRadius);
    int idx = CellIndex(c.x, c.y, c.z);
    gridData[insertPos[idx]++] = i;
  }
}

// ---------------------------------------------------------------------------
void Particles::BuildNeighbours(float smoothingRadius)
{
  ParallelFor(numParticles, [&](int i) {
    int count = 0;
    int *myNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
    Cell cell = PositionToCoord(predictedPositions[i], smoothingRadius);

    for (int ox = -1; ox <= 1; ++ox)
      for (int oy = -1; oy <= 1; ++oy)
        for (int oz = -1; oz <= 1; ++oz) {
          int cx = cell.x + ox;
          int cy = cell.y + oy;
          int cz = cell.z + oz;
          if (cx < 0 || cy < 0 || cz < 0 || cx >= numCells1D || cy >= numCells1D ||
              cz >= numCells1D)
            continue;

          int idx = CellIndex(cx, cy, cz);
          for (int k = gridStart[idx]; k < gridStart[idx + 1]; ++k) {
            int j = gridData[k];
            Vec3 diff = predictedPositions[j] - predictedPositions[i];
            float d2 = diff.Dot(diff);
            if (d2 <= h2 && count < MAX_NEIGHBOURS)
              myNeighbours[count++] = j;
          }
        }
    neighbourCount[i] = count;
  });
}

// ---------------------------------------------------------------------------
float Particles::CalculateLambda(size_t i, float smoothingRadius)
{
  // self-contribution (distance = 0)
  float density    = poly6 * h2 * h2 * h2;
  Vec3  gradI      = {0.0f, 0.0f, 0.0f};
  float denominator = 0.0f;

  const Vec3& pI = predictedPositions[i];

  int* myNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
  for (int k = 0; k < neighbourCount[i]; ++k) {
    int j = myNeighbours[k];
    if (j == (int)i) continue;

    Vec3  diff = pI - predictedPositions[j];
    float d2   = diff.Dot(diff);
    if (d2 >= h2 || d2 < 1e-12f) continue;

    float sq = h2 - d2;
    density += poly6 * sq * sq * sq;

    float d   = std::sqrt(d2);
    float s   = spiky * (smoothingRadius - d) * (smoothingRadius - d);
    Vec3  grad = diff * (s / (d * restDensity));
    gradI      += grad;
    denominator += grad.Dot(grad);
  }

  denominator += gradI.Dot(gradI);
  float Ci = (density / restDensity) - 1.0f;
  return -Ci / (denominator + relaxation);
}

// ---------------------------------------------------------------------------
void Particles::Update(float dt, float smoothingRadius, float radiusPx,
            const int g_fb_w, const int g_fb_h,
            Vec3 rayOrigin, Vec3 rayDir, float mouseStrength)
{
  const float mouseRadius  = mouseStrength > 0.0f ? pushRadius : pullRadius;
  const float mouseRadius2 = mouseRadius * mouseRadius;

  // 1. apply gravity + mouse force, predict positions
  oldPositions = positions;
  for (int i = 0; i < numParticles; ++i) {
    velocities[i] += Vec3{0.0f, gravity, 0.0f} * dt;   // gravity along –Y

    if (mouseStrength != 0.0f) {
      Vec3  toParticle = positions[i] - rayOrigin;
       // project onto ray, then get perpendicular distance
      float along      = toParticle.Dot(rayDir);
      Vec3  closest    = rayOrigin + rayDir * along;
      Vec3  perp       = positions[i] - closest;
      float d2         = perp.Dot(perp);

      if (d2 < mouseRadius2 && d2 > 1e-6f) {
	float d       = std::sqrt(d2);
	float falloff = 1.0f - (d / mouseRadius);
	Vec3  dir     = perp * (1.0f / d);   // push/pull away from ray axis
        velocities[i] += dir * (mouseStrength * falloff * dt);
      }
    }
    predictedPositions[i] = positions[i] + velocities[i] * dt;
  }

  // 2. spatial data structures
  BuildGrid(smoothingRadius);
  if (NeedsNeighbourRebuild()) {
    BuildNeighbours(smoothingRadius);
    positionsAtLastBuild = predictedPositions;
  }

  // 3. PBF solver
  for (int iter = 0; iter < numIterations; ++iter) {
    ParallelFor(numParticles, [&](int i) {
      allLambdas[i] = CalculateLambda(i, smoothingRadius);
    });

    ParallelFor(numParticles, [&](int i) {
      Vec3        sum = {0.0f, 0.0f, 0.0f};
      const Vec3& pi  = predictedPositions[i];

      int* myNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
      for (int k = 0; k < neighbourCount[i]; ++k) {
        int j = myNeighbours[k];
        if (j == i) continue;

        Vec3  diff = pi - predictedPositions[j];
        float d2   = diff.Dot(diff);
        if (d2 < 1e-12f || d2 >= h2) continue;

        float d   = std::sqrt(d2);
        float s   = spiky * (smoothingRadius - d) * (smoothingRadius - d) / d;
        float corr       = Scorr(pi, predictedPositions[j], smoothingRadius);
        float lambdaSum  = allLambdas[i] + allLambdas[j] + corr;
        sum += diff * (s * lambdaSum);
      }
      deltas[i] = sum / restDensity;
    });

    ParallelFor(numParticles, [&](int i) {
      predictedPositions[i] += deltas[i];
      ClampToBoundaries(&predictedPositions[i], radiusPx, g_fb_w, g_fb_h);
    });
  }

  // 4. velocity update
  ParallelFor(numParticles, [&](int i) {
    velocities[i] = (predictedPositions[i] - positions[i]) / dt;
    positions[i]  = predictedPositions[i];
  });

  // 5. XSPH viscosity
  std::vector<Vec3> newVelocities = velocities;

  ParallelFor(numParticles, [&](int i) {
    Vec3  xsph = {0.0f, 0.0f, 0.0f};
    float wSum = 0.0f;

    int* myNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
    for (int k = 0; k < neighbourCount[i]; ++k) {
      int j = myNeighbours[k];
      if (j == i) continue;

      Vec3  diff = predictedPositions[i] - predictedPositions[j];
      float d2   = diff.Dot(diff);
      if (d2 >= h2) continue;

      float sq = h2 - d2;
      float w  = poly6 * sq * sq * sq;
      xsph += (velocities[j] - velocities[i]) * w;
      wSum += w;
    }

    if (wSum > 1e-6f) xsph = xsph * (1.0f / wSum);

    Vec3  dv  = xsph * xsphC;
    float mag = dv.Magnitude();
    if (mag > maxDv) dv = dv * (maxDv / mag);

    newVelocities[i] = velocities[i] + dv;
  });

  // 6. Vorticity confinement 
  // Pass 1: compute vorticity 
  ParallelFor(numParticles, [&](int i) {
    Vec3 omega = {0.0f, 0.0f, 0.0f};
    const Vec3& vi    = newVelocities[i];

    int* myNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
    for (int k = 0; k < neighbourCount[i]; ++k) {
      int j = myNeighbours[k];
      if (j == i) continue;

      Vec3  diff = predictedPositions[i] - predictedPositions[j];
      float d2   = diff.Dot(diff);
      if (d2 < 1e-12f || d2 >= h2) continue;

      float d     = std::sqrt(d2);
      float s     = (smoothingRadius - d) * (smoothingRadius - d) / d;
      Vec3  gradW = diff * s;      
      Vec3 vij = newVelocities[j] - vi;
       
      omega.x += vij.y * gradW.z - vij.z * gradW.y;
      omega.y += vij.z * gradW.x - vij.x * gradW.z;
      omega.z += vij.x * gradW.y - vij.y * gradW.x;
    }
    vorticity[i] = omega;
  });

  // Pass 2: apply vorticity confinement force 
  ParallelFor(numParticles, [&](int i) {
    Vec3 eta = {0.0f, 0.0f, 0.0f};

    int* myNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
    for (int k = 0; k < neighbourCount[i]; ++k) {
      int j = myNeighbours[k];
      if (j == i) continue;

      Vec3  diff = predictedPositions[i] - predictedPositions[j];
      float d2   = diff.Dot(diff);
      if (d2 < 1e-12f || d2 >= h2) continue;

      float d     = std::sqrt(d2);
      float s     = (smoothingRadius - d) * (smoothingRadius - d) / d;
      Vec3  gradW = diff * s;

      float omegaMag = vorticity[j].Magnitude();
      eta += gradW * omegaMag;
    }

    float etaMag = eta.Magnitude();
    if (etaMag < 1e-6f) return;

    Vec3 N = eta * (1.0f / etaMag);       // location vector

    const Vec3& omega = vorticity[i];
    Vec3 f_vorticity = {
    N.y * omega.z - N.z * omega.y,
    N.z * omega.x - N.x * omega.z,
    N.x * omega.y - N.y * omega.x
    };
    newVelocities[i] += f_vorticity * (vorticityEpsilon * dt);
  });
  velocities = newVelocities;
}

// ---------------------------------------------------------------------------
void Particles::ClampToBoundaries(Vec3* pos, float radiusPx,
                                   const int g_fb_w, const int g_fb_h)
{
  // x, y clamped to NDC [-1, 1] with particle-radius margin
  float halfX = radiusPx / (float)g_fb_w;
  float halfY = radiusPx / (float)g_fb_h;

  pos->x = std::max(-1.0f + halfX, std::min(1.0f - halfX, pos->x));
  pos->y = std::max(-1.0f + halfY, std::min(1.0f - halfY, pos->y));

  // z clamped to the same NDC box; adjust bounds to taste
  const float halfZ = halfX;   // assume roughly cubic domain
  pos->z = std::max(-1.0f + halfZ, std::min(1.0f - halfZ, pos->z));
}

// ---------------------------------------------------------------------------
float Particles::CalculateDensity(size_t i, float smoothingRadius)
{
  float density = poly6 * h2 * h2 * h2;  // self

  int* myNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
  for (int k = 0; k < neighbourCount[i]; ++k) {
    int j = myNeighbours[k];
    if (j == (int)i) continue;

    Vec3  diff = predictedPositions[i] - predictedPositions[j];
    float d2   = diff.Dot(diff);
    if (d2 >= h2) continue;

    float sq = h2 - d2;
    density += poly6 * sq * sq * sq;
  }
  return density;
}

float Particles::EstimateRestDensity(float smoothingRadius)
{
  size_t center = numParticles / 2;
  return CalculateDensity(center, smoothingRadius);
}

// ---------------------------------------------------------------------------
void Particles::Reset(float smoothingRadius)
{
  positions.clear();
  predictedPositions.clear();
  velocities.clear();
  densities.clear();

  std::fill(gridCount.begin(),      gridCount.end(),      0);
  std::fill(neighbourCount.begin(), neighbourCount.end(), 0);

  InitialiseParticles(numParticles, initSpacing);
  BuildGrid(smoothingRadius);
  BuildNeighbours(smoothingRadius);
  restDensity = EstimateRestDensity(smoothingRadius);
}

// ---------------------------------------------------------------------------
float Particles::CalculateDistance(Vec3 a, Vec3 b)
{
  return (a - b).Magnitude();
}

float Particles::Scorr(Vec3 pi, Vec3 pj, float h)
{
  Vec3  diff = pi - pj;
  float d2   = diff.Dot(diff);
  if (d2 >= h2) return 0.0f;

  float sq    = h2 - d2;
  float W     = poly6 * sq * sq * sq;
  float ratio = W / wDq;
  float r2    = ratio  * ratio;
  float r4    = r2 * r2;
  return -scorrCoefficient * r4;
}

// ---------------------------------------------------------------------------
bool Particles::NeedsNeighbourRebuild()
{
  if (positionsAtLastBuild.size() != (size_t)numParticles) return true;
  for (int i = 0; i < numParticles; ++i) {
    Vec3 diff = predictedPositions[i] - positionsAtLastBuild[i];
    if (diff.Dot(diff) > skinRadius * skinRadius) return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
void Particles::ResizeParticles(int nNewParticles, float fSmoothingRadius,
                                 float spacing, float ox, float oy, float oz)
{
    initOffsetX = ox;
    initOffsetY = oy;
    initOffsetZ = oz;
    initSpacing  = spacing;
    numParticles    = nNewParticles;

    positions.clear();          positions.reserve(nNewParticles);
    predictedPositions.clear(); predictedPositions.reserve(nNewParticles);
    velocities.clear();         velocities.reserve(nNewParticles);
    densities.clear();          densities.reserve(nNewParticles);

    allLambdas.resize(nNewParticles);
    deltas.resize(nNewParticles);
    oldPositions.resize(nNewParticles);
    vorticity.resize(nNewParticles, Vec3{0.0f, 0.0f, 0.0f});
    neighbourData.resize(nNewParticles * MAX_NEIGHBOURS);
    neighbourCount.resize(nNewParticles, 0);
    indices.resize(nNewParticles);
    std::iota(indices.begin(), indices.end(), 0);

    // Resize grid arrays for new nCells1D (smoothing radius unchanged here,
    // but guard in case caller changes it later)
    const int nCells = numCells1D * numCells1D * numCells1D;
    gridStart.assign(nCells + 1, 0);
    gridCount.assign(nCells, 0);

    InitialiseParticles(nNewParticles, initSpacing);
    BuildGrid(fSmoothingRadius);
    BuildNeighbours(fSmoothingRadius);
    positionsAtLastBuild = predictedPositions;
    restDensity = EstimateRestDensity(fSmoothingRadius);
}
