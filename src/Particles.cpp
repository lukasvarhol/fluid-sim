#include "particles.h"
#include "colors.h"

#include <chrono>

using clk = std::chrono::high_resolution_clock;

auto us = [](auto a, auto b) {
    return std::chrono::duration_cast<std::chrono::microseconds>(b - a).count();
    };

Particles::Particles(int n, float smoothingRadius)
    : h2(smoothingRadius * smoothingRadius)
    , h5(h2 * h2 * smoothingRadius)
    , h8(h5 * h2 * smoothingRadius)
    , poly6_norm(4.0f / (PI * h8))
    , spiky_norm(-30.0f / (PI * h5))
{
    float dq = 0.14f * smoothingRadius;  // paper recommends 0.1-0.3h
    float sq = h2 - dq * dq;
    W_dq = poly6_norm * sq * sq * sq;
    skinRadius = 0.3f * smoothingRadius;  // tune this
    positionsAtLastBuild = predictedPositions;

    nParticles = n;
    nCells1D = (int)std::ceil(2.0f / smoothingRadius);

    const int nCells = nCells1D * nCells1D;
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
    vorticity.resize(nParticles, 0.0f);

    neighbourData.resize(nParticles * MAX_NEIGHBOURS);
    neighbourCount.resize(nParticles, 0);

    initialiseParticles(nParticles, INIT_SPACING);
    buildGrid(smoothingRadius);
    buildNeighbours(smoothingRadius);
    restDensity = estimateRestDensity(smoothingRadius);
}

// ---------------------------------------------------------------------------
void Particles::initialiseParticles(int n, float spacing)
{
    int particlesPerRow = (int)std::sqrt((float)n);
    int particlesPerCol = (n - 1) / particlesPerRow + 1;

    for (int i = 0; i < n; ++i)
    {
        float x = ((i % particlesPerRow - particlesPerRow / 2.0f + 0.5f) * spacing) + INIT_OFFSET_X;
        float y = ((i / particlesPerRow - particlesPerCol / 2.0f + 0.5f) * spacing) + INIT_OFFSET_Y;
        positions.push_back(Vec2{x, y});
        predictedPositions.push_back(Vec2{x, y});
        velocities.push_back(Vec2{0.0f, 0.0f});
        colors.push_back(Vec3{0.0f, 0.0f, 1.0f});
        densities.push_back(0.0f);
    }
}

// ---------------------------------------------------------------------------
Cell Particles::positionToCoord(Vec2 position, float smoothingRadius)
{
    int cx = (int)((position.x + 1.0f) / smoothingRadius);
    int cy = (int)((position.y + 1.0f) / smoothingRadius);
    cx = std::max(0, std::min(nCells1D - 1, cx));
    cy = std::max(0, std::min(nCells1D - 1, cy));
    return Cell{cx, cy};
}

void Particles::buildGrid(float smoothingRadius)
{
    const int nCells = nCells1D * nCells1D;

    // reset counts
    std::fill(gridCount.begin(), gridCount.end(), 0);

    // pass 1: count particles per cell
    for (int i = 0; i < nParticles; ++i)
    {
        Cell c = positionToCoord(predictedPositions[i], smoothingRadius);
        int cellIdx = c.x * nCells1D + c.y;
        ++gridCount[cellIdx];
    }

    gridStart[0] = 0;
    for (int c = 0; c < nCells; ++c)
        gridStart[c + 1] = gridStart[c] + gridCount[c];

    // pass 2: fill gridData
    gridData.resize(gridStart[nCells]);
    std::vector<int> insertPos(gridStart.begin(), gridStart.begin() + nCells);

    for (int i = 0; i < nParticles; ++i)
    {
        Cell c = positionToCoord(predictedPositions[i], smoothingRadius);
        int cellIdx = c.x * nCells1D + c.y;
        gridData[insertPos[cellIdx]++] = i;
    }
}

// ---------------------------------------------------------------------------
// Flat neighbour list: count → prefix sum → fill
// ---------------------------------------------------------------------------
void Particles::buildNeighbours(float smoothingRadius)
{
        parallelFor(nParticles, [&](int i) {
            int count = 0;
            int* myNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
            Cell cell = positionToCoord(predictedPositions[i], smoothingRadius);

            for (int ox = -1; ox <= 1; ++ox)
                for (int oy = -1; oy <= 1; ++oy)
                {
                    int cx = cell.x + ox;
                    int cy = cell.y + oy;
                    if (cx < 0 || cy < 0 || cx >= nCells1D || cy >= nCells1D) continue;

                    int cellIdx = cx * nCells1D + cy;
                    for (int k = gridStart[cellIdx]; k < gridStart[cellIdx + 1]; ++k)
                    {
                        int j = gridData[k];
                        Vec2 diff = predictedPositions[j] - predictedPositions[i];
                        float d2 = diff.dot(diff);
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
    // self-contribution to density (distance = 0)
    float density = poly6_norm * h2 * h2 * h2;

    Vec2  gradI = {0.0f, 0.0f};
    float denominator = 0.0f;

    const Vec2& pi = predictedPositions[i];

    int* myNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
    for (int k = 0; k < neighbourCount[i]; ++k)
    {
        int j = myNeighbours[k];
        if (j == (int)i) continue;

        Vec2 diff = pi - predictedPositions[j];
        float d2 = diff.dot(diff);
        if (d2 >= h2 || d2 < 1e-12f) continue;

        // poly6 density contribution
        float sq = h2 - d2;
        density += poly6_norm * sq * sq * sq;

        // spiky gradient (sqrt only for particles inside radius)
        float d = std::sqrt(d2);
        float s = spiky_norm * (smoothingRadius - d) * (smoothingRadius - d);
        Vec2 grad = diff * (s / (d * restDensity));
        gradI += grad;
        denominator += grad.dot(grad);
    }

    denominator += gradI.dot(gradI);

    float Ci = (density / restDensity) - 1.0f;
    return -Ci / (denominator + RELAXATION_F);
}
// ---------------------------------------------------------------------------
void Particles::update(float dt, float smoothingRadius, float radiusPx,
    const int g_fb_w, const int g_fb_h,
    Vec2 mousePos, float mouseStrength)
{
    const float mouseRadius = mouseStrength < 0.0f? PUSH_RAD : PULL_RAD;
    const float mouseRadius2 = mouseRadius * mouseRadius;

    // 1. apply gravity, predict positions
    oldPositions = positions;
    for (int i = 0; i < nParticles; ++i)
    {
      
      velocities[i] += Vec2{0.0f, gravity} * dt;


        if (mouseStrength != 0.0f) {
            Vec2 diff = mousePos - positions[i];
            float d2 = diff.dot(diff);
            if (d2 < mouseRadius2 && d2 > 1e-6f) {
                float d = std::sqrt(d2);
                float falloff = 1.0f - (d / mouseRadius);
                Vec2 dir = diff * (1.0f / d);
                velocities[i] += dir * (mouseStrength * falloff * dt);
            }
        }
        predictedPositions[i]  = positions[i] + velocities[i] * dt;
    }

    // 2. build grid and neighbour list once

    buildGrid(smoothingRadius);
    if (needsNeighbourRebuild()) {
        buildNeighbours(smoothingRadius);
        positionsAtLastBuild = predictedPositions;
    }

    // 3. solver
    const int solverIterations = NUM_ITERATIONS;
    for (int iter = 0; iter < solverIterations; ++iter)
    {
        // lambda
        parallelFor(nParticles, [&](int i) {
                allLambdas[i] = calculateLambda(i, smoothingRadius);
            });

        // delta
        parallelFor(nParticles, [&](int i) {
            Vec2 sum = {0.0f, 0.0f};
            const Vec2& pi = predictedPositions[i];

            int* myNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
            for (int k = 0; k < neighbourCount[i]; ++k)
            {
                int j = myNeighbours[k];
                if (j == i) continue;

                Vec2 diff = pi - predictedPositions[j];
                float d2 = diff.dot(diff);
                if (d2 < 1e-12f || d2 >= h2) continue;

                float d = std::sqrt(d2);
                float s = spiky_norm * (smoothingRadius - d) * (smoothingRadius - d) / d;
                float corr = scorr(pi, predictedPositions[j], smoothingRadius);
                float lambdaSum = allLambdas[i] + allLambdas[j] + corr; 
                sum += diff * (s * lambdaSum);
            }
            deltas[i] = sum / restDensity;
            });

        // apply deltas
        parallelFor(nParticles, [&](int i) {
            predictedPositions[i] += deltas[i];
            clampToBoundaries(&predictedPositions[i], radiusPx, g_fb_w, g_fb_h);
            });
    }


    // 4. update velocities and positions
    parallelFor(nParticles, [&](int i) {
        velocities[i] = (predictedPositions[i] - positions[i]) / dt;
        colors[i] = getColor(velocities[i]);
        positions[i]  = predictedPositions[i];
        });

    //// 5. XSPH velocity smoothing
std::vector<Vec2> newVelocities = velocities;

parallelFor(nParticles, [&](int i) {
    Vec2 xsph = { 0.0f, 0.0f };
    float wSum = 0.0f;

    int* myNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
    for (int k = 0; k < neighbourCount[i]; ++k)
    {
        int j = myNeighbours[k];
        if (j == i) continue;

        Vec2 diff = predictedPositions[i] - predictedPositions[j];
        float d2 = diff.dot(diff);
        if (d2 >= h2) continue;

        float sq = h2 - d2;
        float w = poly6_norm * sq * sq * sq;
        xsph += (velocities[j] - velocities[i]) * w;
        wSum += w;
    }

    if (wSum > 1e-6f)
        xsph = xsph * (1.0f / wSum);

    Vec2 dv = xsph * xsphC;
    float mag = dv.magnitude();
    if (mag > maxDv)
        dv = dv * (maxDv / mag);

    newVelocities[i] = velocities[i] + dv;
    });

    // pass 1
    parallelFor(nParticles, [&](int i) {
            float omega = 0.0f;
            const Vec2& vi = newVelocities[i];

            int* myNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
            for (int k = 0; k < neighbourCount[i]; ++k)
            {
                int j = myNeighbours[k];
                if (j == i) continue;

                Vec2 diff = predictedPositions[i] - predictedPositions[j];
                float d2 = diff.dot(diff);
                if (d2 < 1e-12f || d2 >= h2) continue;

                float d = std::sqrt(d2);
                float s = (smoothingRadius - d) * (smoothingRadius - d) / d;
                Vec2 gradW = diff * s;

                Vec2 vij = newVelocities[j] - vi;
                omega += vij.x * gradW.y - vij.y * gradW.x;
            }
            vorticity[i] = omega;
        });

// pass 2
    parallelFor(nParticles, [&](int i) {
            Vec2 eta = { 0.0f, 0.0f };

            int* myNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
            for (int k = 0; k < neighbourCount[i]; ++k)
            {
                int j = myNeighbours[k];
                if (j == i) continue;

                Vec2 diff = predictedPositions[i] - predictedPositions[j];
                float d2 = diff.dot(diff);
                if (d2 < 1e-12f || d2 >= h2) continue;

                float d = std::sqrt(d2);
                float s = (smoothingRadius - d) * (smoothingRadius - d) / d;
                Vec2  gradW = diff * s;


                eta += gradW * std::abs(vorticity[j]);
            }

            float etaMag = eta.magnitude();
            if (etaMag < 1e-6f) return;


            Vec2 N = eta * (1.0f / etaMag);

            float omega = vorticity[i];
            Vec2  f_vorticity = Vec2{ N.y, -N.x } * (omega * vorticityEpsilon);
            newVelocities[i] += f_vorticity * dt;
        });

    velocities = newVelocities;
}

// ---------------------------------------------------------------------------
void Particles::clampToBoundaries(Vec2* pos, float radiusPx,
                                   const int g_fb_w, const int g_fb_h)
{
  float half[2] = {radiusPx / (float)g_fb_w, radiusPx / (float)g_fb_h};
    float* arr[2] = {&pos->x, &pos->y};

    for (int i = 0; i < 2; ++i)
    {
        float lo = -1.0f + half[i];
        float hi =  1.0f - half[i];
        *arr[i] = std::max(lo, std::min(hi, *arr[i]));
    }
}

// ---------------------------------------------------------------------------
float Particles::calculateDensity(size_t i, float smoothingRadius)
{
    float density = poly6_norm * h2 * h2 * h2;  // self

    int* myNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
    for (int k = 0; k < neighbourCount[i]; ++k)
    {
        int j = myNeighbours[k];
        if (j == (int)i) continue;

        Vec2  diff = predictedPositions[i] - predictedPositions[j];
        float d2   = diff.dot(diff);
        if (d2 >= h2) continue;

        float sq = h2 - d2;
        density += poly6_norm * sq * sq * sq;
    }
    return density;
}

// ---------------------------------------------------------------------------
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

    std::fill(gridCount.begin(), gridCount.end(), 0);
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

Vec2 Particles::spikyKernelGradVec(Vec2 a, Vec2 b, float smoothingRadius)
{
    Vec2 diff = a - b;
    float distance = diff.magnitude();
    if (distance <= 1e-6f) return Vec2{0.0f, 0.0f};
    float scalar = spikyKernelGrad(smoothingRadius, distance);
    return diff * (scalar / distance);
}

float Particles::poly6Kernel(float smoothingRadius, float distance)
{
    if (distance > smoothingRadius) return 0.0f;
    float sq = h2 - distance * distance;
    return poly6_norm * sq * sq * sq;
}

float Particles::calculateDistance(Vec2 a, Vec2 b)
{
    return (a - b).magnitude();
}

float Particles::scorr(Vec2 pi, Vec2 pj, float h)
{
    Vec2 diff = pi - pj;
    float d2 = diff.dot(diff);
    if (d2 >= h2) return 0.0f;

    float sq = h2 - d2;
    float W = poly6_norm * sq * sq * sq;
    float ratio = W / W_dq;
    float ratio2 = ratio * ratio;
    float ratio4 = ratio2 * ratio2;

    const int n = 4;
    return -k * ratio4;       // purely repulsive, always negative
}

// ---------------------------------------------------------------------------
Vec3 Particles::getColor(Vec2& vel)
{
    float magnitude = vel.magnitude();
    float s = std::clamp(magnitude / MAX_SPEED, 0.0f, 1.0f);

    for (size_t i = 0; i + 1 < ColorStops.size(); ++i)
    {
        if (s >= ColorStops[i].pos && s <= ColorStops[i + 1].pos)
        {
            float span = ColorStops[i + 1].pos - ColorStops[i].pos;
            float t = (span > 0.0f) ? (s - ColorStops[i].pos) / span : 0.0f;

            Vec3 lower = {ColorStops[i].r, ColorStops[i].g, ColorStops[i].b};
            Vec3 upper = {ColorStops[i+1].r, ColorStops[i+1].g, ColorStops[i+1].b};
            return lerp(lower, upper, t);
        }
    }
    return {1.0f, 1.0f, 1.0f};
}

bool Particles::needsNeighbourRebuild() {
    if (positionsAtLastBuild.size() != (size_t)nParticles)
        return true;
    for (int i = 0; i < nParticles; ++i) {
        Vec2 diff = predictedPositions[i] - positionsAtLastBuild[i];
        if (diff.dot(diff) > skinRadius * skinRadius)
            return true;
    }
    return false;
}

void Particles::resizeParticles(int nNewParticles, float fSmoothingRadius, float spacing, float ox, float oy) {
  INIT_OFFSET_X = ox;
  INIT_OFFSET_Y = oy;
  INIT_SPACING = spacing;
 
  nParticles = nNewParticles;

  positions.clear();
  predictedPositions.clear();
  velocities.clear();
  densities.clear();
  colors.clear();

  positions.reserve(nNewParticles);
  predictedPositions.reserve(nNewParticles);
  velocities.reserve(nNewParticles);
  densities.reserve(nNewParticles);
  colors.reserve(nNewParticles);

  allLambdas.resize(nNewParticles);
  deltas.resize(nNewParticles);
  oldPositions.resize(nNewParticles);
  vorticity.resize(nNewParticles, 0.0f);
  neighbourData.resize(nNewParticles * MAX_NEIGHBOURS);
  neighbourCount.resize(nNewParticles, 0);
  indices.resize(nNewParticles);
  std::iota(indices.begin(), indices.end(), 0);

  initialiseParticles(nNewParticles, INIT_SPACING);
  buildGrid(fSmoothingRadius);
  buildNeighbours(fSmoothingRadius);
  positionsAtLastBuild = predictedPositions;
  restDensity = estimateRestDensity(fSmoothingRadius);
}
