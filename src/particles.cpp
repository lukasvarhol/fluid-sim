#include "particles.h"
#include "colors.h"

#include <chrono>
#include <cmath>
#include <algorithm>

using clk = std::chrono::high_resolution_clock;

auto us = [](auto a, auto b) {
    return std::chrono::duration_cast<std::chrono::microseconds>(b - a).count();
};

Particles::Particles(int n, float smoothingRadius)
    : h2(smoothingRadius * smoothingRadius)
    , h5(h2 * h2 * smoothingRadius)
    , h8(h5 * h2 * smoothingRadius)
    , poly6_norm(315.0f / (64.0f * PI * h8 * smoothingRadius))   // 3-D poly6
    , spiky_norm(-45.0f / (PI * h5 * smoothingRadius))            // 3-D spiky gradient
{
    float dq  = 0.14f * smoothingRadius;
    float sq  = h2 - dq * dq;
    W_dq      = poly6_norm * sq * sq * sq;
    skinRadius = 0.3f * smoothingRadius;

    nParticles = n;
    nCells1D   = (int)std::ceil(2.0f / smoothingRadius);

    const int nCells = nCells1D * nCells1D * nCells1D;   // 3-D grid
    gridData.reserve(nParticles * 4);
    gridStart.resize(nCells + 1, 0);
    gridCount.resize(nCells, 0);
    indices.resize(nParticles);
    std::iota(indices.begin(), indices.end(), 0);

    positions.reserve(nParticles);
    predictedPositions.reserve(nParticles);
    velocities.reserve(nParticles);
    densities.reserve(nParticles);
    colors.reserve(nParticles);
    allLambdas.resize(nParticles);
    deltas.resize(nParticles);
    oldPositions.resize(nParticles);

    // Vorticity is a 3-D vector field (curl of velocity)
    vorticity.resize(nParticles, Vec3{0.0f, 0.0f, 0.0f});

    neighbourData.resize(nParticles * MAX_NEIGHBOURS);
    neighbourCount.resize(nParticles, 0);

    initialiseParticles(nParticles, INIT_SPACING);
    buildGrid(smoothingRadius);
    buildNeighbours(smoothingRadius);
    positionsAtLastBuild = predictedPositions;
    restDensity = estimateRestDensity(smoothingRadius);
}

// ---------------------------------------------------------------------------
void Particles::initialiseParticles(int n, float spacing)
{
    int particlesPerDim   = (int)std::cbrt((float)n);
    int particlesPerLayer = particlesPerDim * particlesPerDim;
    int totalLayers       = (n - 1) / particlesPerLayer + 1;

    for (int i = 0; i < n; ++i) {
        int xIndex = i % particlesPerDim;
        int yIndex = (i / particlesPerDim) % particlesPerDim;
        int zIndex = i / particlesPerLayer;

        float x = ((xIndex - particlesPerDim / 2.0f + 0.5f) * spacing) + INIT_OFFSET_X;
        float y = ((yIndex - particlesPerDim / 2.0f + 0.5f) * spacing) + INIT_OFFSET_Y;
        float z = ((zIndex - totalLayers   / 2.0f + 0.5f) * spacing) + INIT_OFFSET_Z;

        positions.push_back(Vec3{x, y, z});
        predictedPositions.push_back(Vec3{x, y, z});
        velocities.push_back(Vec3{0.0f, 0.0f, 0.0f});
        colors.push_back(Vec3{0.0f, 0.0f, 1.0f});
        densities.push_back(0.0f);
    }
}

// ---------------------------------------------------------------------------
Cell Particles::positionToCoord(Vec3 position, float smoothingRadius)
{
    int cx = (int)((position.x + 1.0f) / smoothingRadius);
    int cy = (int)((position.y + 1.0f) / smoothingRadius);
    int cz = (int)((position.z + 1.0f) / smoothingRadius);
    cx = std::max(0, std::min(nCells1D - 1, cx));
    cy = std::max(0, std::min(nCells1D - 1, cy));
    cz = std::max(0, std::min(nCells1D - 1, cz));
    return Cell{cx, cy, cz};
}

// Flat 3-D cell index
inline int Particles::cellIndex(int cx, int cy, int cz) const
{
    return cx * nCells1D * nCells1D + cy * nCells1D + cz;
}

void Particles::buildGrid(float smoothingRadius)
{
    const int nCells = nCells1D * nCells1D * nCells1D;

    std::fill(gridCount.begin(), gridCount.end(), 0);

    // pass 1: count
    for (int i = 0; i < nParticles; ++i) {
        Cell c = positionToCoord(predictedPositions[i], smoothingRadius);
        ++gridCount[cellIndex(c.x, c.y, c.z)];
    }

    gridStart[0] = 0;
    for (int c = 0; c < nCells; ++c)
        gridStart[c + 1] = gridStart[c] + gridCount[c];

    // pass 2: fill
    gridData.resize(gridStart[nCells]);
    std::vector<int> insertPos(gridStart.begin(), gridStart.begin() + nCells);

    for (int i = 0; i < nParticles; ++i) {
        Cell c = positionToCoord(predictedPositions[i], smoothingRadius);
        int idx = cellIndex(c.x, c.y, c.z);
        gridData[insertPos[idx]++] = i;
    }
}

// ---------------------------------------------------------------------------
void Particles::buildNeighbours(float smoothingRadius)
{
    parallelFor(nParticles, [&](int i) {
        int  count        = 0;
        int* myNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
        Cell cell         = positionToCoord(predictedPositions[i], smoothingRadius);

        for (int ox = -1; ox <= 1; ++ox)
        for (int oy = -1; oy <= 1; ++oy)
        for (int oz = -1; oz <= 1; ++oz)           // full 3-D stencil
        {
            int cx = cell.x + ox;
            int cy = cell.y + oy;
            int cz = cell.z + oz;
            if (cx < 0 || cy < 0 || cz < 0 ||
                cx >= nCells1D || cy >= nCells1D || cz >= nCells1D) continue;

            int idx = cellIndex(cx, cy, cz);
            for (int k = gridStart[idx]; k < gridStart[idx + 1]; ++k) {
                int j = gridData[k];
                Vec3  diff = predictedPositions[j] - predictedPositions[i];
                float d2   = diff.dot(diff);
                if (d2 <= h2 && count < MAX_NEIGHBOURS)
                    myNeighbours[count++] = j;
            }
        }
        neighbourCount[i] = count;
    });
}

// ---------------------------------------------------------------------------
float Particles::calculateLambda(size_t i, float smoothingRadius)
{
    // self-contribution (distance = 0)
    float density    = poly6_norm * h2 * h2 * h2;
    Vec3  gradI      = {0.0f, 0.0f, 0.0f};
    float denominator = 0.0f;

    const Vec3& pi = predictedPositions[i];

    int* myNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
    for (int k = 0; k < neighbourCount[i]; ++k) {
        int j = myNeighbours[k];
        if (j == (int)i) continue;

        Vec3  diff = pi - predictedPositions[j];
        float d2   = diff.dot(diff);
        if (d2 >= h2 || d2 < 1e-12f) continue;

        float sq = h2 - d2;
        density += poly6_norm * sq * sq * sq;

        float d   = std::sqrt(d2);
        float s   = spiky_norm * (smoothingRadius - d) * (smoothingRadius - d);
        Vec3  grad = diff * (s / (d * restDensity));
        gradI      += grad;
        denominator += grad.dot(grad);
    }

    denominator += gradI.dot(gradI);

    float Ci = (density / restDensity) - 1.0f;
    return -Ci / (denominator + RELAXATION_F);
}

// ---------------------------------------------------------------------------
void Particles::update(float dt, float smoothingRadius, float radiusPx,
            const int g_fb_w, const int g_fb_h,
            Vec3 rayOrigin, Vec3 rayDir, float mouseStrength)
{
    const float mouseRadius  = mouseStrength > 0.0f ? PUSH_RAD : PULL_RAD;
    const float mouseRadius2 = mouseRadius * mouseRadius;

    // 1. apply gravity + mouse force, predict positions
    oldPositions = positions;
    for (int i = 0; i < nParticles; ++i) {
        velocities[i] += Vec3{0.0f, gravity, 0.0f} * dt;   // gravity along –Y

        if (mouseStrength != 0.0f) {
	  Vec3  toParticle = positions[i] - rayOrigin;
	  // project onto ray, then get perpendicular distance
	  float along      = toParticle.dot(rayDir);
	  Vec3  closest    = rayOrigin + rayDir * along;
	  Vec3  perp       = positions[i] - closest;
	  float d2         = perp.dot(perp);

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
    buildGrid(smoothingRadius);
    if (needsNeighbourRebuild()) {
        buildNeighbours(smoothingRadius);
        positionsAtLastBuild = predictedPositions;
    }

    // 3. PBF solver
    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        parallelFor(nParticles, [&](int i) {
            allLambdas[i] = calculateLambda(i, smoothingRadius);
        });

        parallelFor(nParticles, [&](int i) {
            Vec3        sum = {0.0f, 0.0f, 0.0f};
            const Vec3& pi  = predictedPositions[i];

            int* myNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
            for (int k = 0; k < neighbourCount[i]; ++k) {
                int j = myNeighbours[k];
                if (j == i) continue;

                Vec3  diff = pi - predictedPositions[j];
                float d2   = diff.dot(diff);
                if (d2 < 1e-12f || d2 >= h2) continue;

                float d   = std::sqrt(d2);
                float s   = spiky_norm * (smoothingRadius - d) * (smoothingRadius - d) / d;
                float corr       = scorr(pi, predictedPositions[j], smoothingRadius);
                float lambdaSum  = allLambdas[i] + allLambdas[j] + corr;
                sum += diff * (s * lambdaSum);
            }
            deltas[i] = sum / restDensity;
        });

        parallelFor(nParticles, [&](int i) {
            predictedPositions[i] += deltas[i];
            clampToBoundaries(&predictedPositions[i], radiusPx, g_fb_w, g_fb_h);
        });
    }

    // 4. velocity update
    parallelFor(nParticles, [&](int i) {
        velocities[i] = (predictedPositions[i] - positions[i]) / dt;
        colors[i]     = getColor(velocities[i]);
        positions[i]  = predictedPositions[i];
    });

    // 5. XSPH viscosity
    std::vector<Vec3> newVelocities = velocities;

    parallelFor(nParticles, [&](int i) {
        Vec3  xsph = {0.0f, 0.0f, 0.0f};
        float wSum = 0.0f;

        int* myNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
        for (int k = 0; k < neighbourCount[i]; ++k) {
            int j = myNeighbours[k];
            if (j == i) continue;

            Vec3  diff = predictedPositions[i] - predictedPositions[j];
            float d2   = diff.dot(diff);
            if (d2 >= h2) continue;

            float sq = h2 - d2;
            float w  = poly6_norm * sq * sq * sq;
            xsph += (velocities[j] - velocities[i]) * w;
            wSum += w;
        }

        if (wSum > 1e-6f) xsph = xsph * (1.0f / wSum);

        Vec3  dv  = xsph * xsphC;
        float mag = dv.magnitude();
        if (mag > maxDv) dv = dv * (maxDv / mag);

        newVelocities[i] = velocities[i] + dv;
    });

    // 6. Vorticity confinement — 3-D
    // Pass 1: compute vorticity (curl) as a Vec3
    parallelFor(nParticles, [&](int i) {
        Vec3        omega = {0.0f, 0.0f, 0.0f};
        const Vec3& vi    = newVelocities[i];

        int* myNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
        for (int k = 0; k < neighbourCount[i]; ++k) {
            int j = myNeighbours[k];
            if (j == i) continue;

            Vec3  diff = predictedPositions[i] - predictedPositions[j];
            float d2   = diff.dot(diff);
            if (d2 < 1e-12f || d2 >= h2) continue;

            float d     = std::sqrt(d2);
            float s     = (smoothingRadius - d) * (smoothingRadius - d) / d;
            Vec3  gradW = diff * s;          // ∝ ∇W (direction only, unnormalised)

            Vec3 vij = newVelocities[j] - vi;

            // curl: ω += vij × ∇W
            omega.x += vij.y * gradW.z - vij.z * gradW.y;
            omega.y += vij.z * gradW.x - vij.x * gradW.z;
            omega.z += vij.x * gradW.y - vij.y * gradW.x;
        }
        vorticity[i] = omega;
    });

    // Pass 2: apply vorticity confinement force f = ε (N × ω)
    parallelFor(nParticles, [&](int i) {
        Vec3 eta = {0.0f, 0.0f, 0.0f};

        int* myNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
        for (int k = 0; k < neighbourCount[i]; ++k) {
            int j = myNeighbours[k];
            if (j == i) continue;

            Vec3  diff = predictedPositions[i] - predictedPositions[j];
            float d2   = diff.dot(diff);
            if (d2 < 1e-12f || d2 >= h2) continue;

            float d     = std::sqrt(d2);
            float s     = (smoothingRadius - d) * (smoothingRadius - d) / d;
            Vec3  gradW = diff * s;

            float omegaMag = vorticity[j].magnitude();
            eta += gradW * omegaMag;          // ∇|ω|
        }

        float etaMag = eta.magnitude();
        if (etaMag < 1e-6f) return;

        Vec3 N = eta * (1.0f / etaMag);       // location vector

        // f = ε (N × ω_i)
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
void Particles::clampToBoundaries(Vec3* pos, float radiusPx,
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
float Particles::calculateDensity(size_t i, float smoothingRadius)
{
    float density = poly6_norm * h2 * h2 * h2;  // self

    int* myNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
    for (int k = 0; k < neighbourCount[i]; ++k) {
        int j = myNeighbours[k];
        if (j == (int)i) continue;

        Vec3  diff = predictedPositions[i] - predictedPositions[j];
        float d2   = diff.dot(diff);
        if (d2 >= h2) continue;

        float sq = h2 - d2;
        density += poly6_norm * sq * sq * sq;
    }
    return density;
}

float Particles::estimateRestDensity(float smoothingRadius)
{
    size_t center = nParticles / 2;
    return calculateDensity(center, smoothingRadius);
}

// ---------------------------------------------------------------------------
void Particles::reset(float smoothingRadius)
{
    positions.clear();
    predictedPositions.clear();
    velocities.clear();
    densities.clear();
    colors.clear();

    std::fill(gridCount.begin(),      gridCount.end(),      0);
    std::fill(neighbourCount.begin(), neighbourCount.end(), 0);

    initialiseParticles(nParticles, INIT_SPACING);
    buildGrid(smoothingRadius);
    buildNeighbours(smoothingRadius);
    restDensity = estimateRestDensity(smoothingRadius);
}

// ---------------------------------------------------------------------------
float Particles::spikyKernelGrad(float smoothingRadius, float distance)
{
    if (distance > smoothingRadius) return 0.0f;
    return spiky_norm * (smoothingRadius - distance) * (smoothingRadius - distance);
}

Vec3 Particles::spikyKernelGradVec(Vec3 a, Vec3 b, float smoothingRadius)
{
    Vec3  diff     = a - b;
    float distance = diff.magnitude();
    if (distance <= 1e-6f) return Vec3{0.0f, 0.0f, 0.0f};
    float scalar = spikyKernelGrad(smoothingRadius, distance);
    return diff * (scalar / distance);
}

float Particles::poly6Kernel(float smoothingRadius, float distance)
{
    if (distance > smoothingRadius) return 0.0f;
    float sq = h2 - distance * distance;
    return poly6_norm * sq * sq * sq;
}

float Particles::calculateDistance(Vec3 a, Vec3 b)
{
    return (a - b).magnitude();
}

float Particles::scorr(Vec3 pi, Vec3 pj, float h)
{
    Vec3  diff = pi - pj;
    float d2   = diff.dot(diff);
    if (d2 >= h2) return 0.0f;

    float sq    = h2 - d2;
    float W     = poly6_norm * sq * sq * sq;
    float ratio = W / W_dq;
    float r2    = ratio  * ratio;
    float r4    = r2 * r2;
    return -k * r4;
}

// ---------------------------------------------------------------------------
Vec3 Particles::getColor(Vec3& vel)
{
    float magnitude = vel.magnitude();
    float s = std::clamp(magnitude / MAX_SPEED, 0.0f, 1.0f);

    for (size_t i = 0; i + 1 < ColorStops.size(); ++i) {
        if (s >= ColorStops[i].pos && s <= ColorStops[i + 1].pos) {
            float span = ColorStops[i + 1].pos - ColorStops[i].pos;
            float t    = (span > 0.0f) ? (s - ColorStops[i].pos) / span : 0.0f;
            Vec3 lower = {ColorStops[i].r,   ColorStops[i].g,   ColorStops[i].b};
            Vec3 upper = {ColorStops[i+1].r, ColorStops[i+1].g, ColorStops[i+1].b};
            return lerp(lower, upper, t);
        }
    }
    return {1.0f, 1.0f, 1.0f};
}

// ---------------------------------------------------------------------------
bool Particles::needsNeighbourRebuild()
{
    if (positionsAtLastBuild.size() != (size_t)nParticles) return true;
    for (int i = 0; i < nParticles; ++i) {
        Vec3 diff = predictedPositions[i] - positionsAtLastBuild[i];
        if (diff.dot(diff) > skinRadius * skinRadius) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
void Particles::resizeParticles(int nNewParticles, float fSmoothingRadius,
                                 float spacing, float ox, float oy, float oz)
{
    INIT_OFFSET_X = ox;
    INIT_OFFSET_Y = oy;
    INIT_OFFSET_Z = oz;
    INIT_SPACING  = spacing;
    nParticles    = nNewParticles;

    positions.clear();          positions.reserve(nNewParticles);
    predictedPositions.clear(); predictedPositions.reserve(nNewParticles);
    velocities.clear();         velocities.reserve(nNewParticles);
    densities.clear();          densities.reserve(nNewParticles);
    colors.clear();             colors.reserve(nNewParticles);

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
    const int nCells = nCells1D * nCells1D * nCells1D;
    gridStart.assign(nCells + 1, 0);
    gridCount.assign(nCells, 0);

    initialiseParticles(nNewParticles, INIT_SPACING);
    buildGrid(fSmoothingRadius);
    buildNeighbours(fSmoothingRadius);
    positionsAtLastBuild = predictedPositions;
    restDensity = estimateRestDensity(fSmoothingRadius);
}
