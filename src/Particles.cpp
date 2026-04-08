#include "particles.h"
#include "colors.h"

Particles::Particles(int n, float smoothingRadius)
    : h2(smoothingRadius * smoothingRadius)
    , h5(h2 * h2 * smoothingRadius)
    , h8(h5 * h2 * smoothingRadius)
    , poly6_norm(4.0f / (PI * h8))
    , spiky_norm(-30.0f / (PI * h5))
{
    nParticles = n;
    nCells1D   = (int)std::ceil(2.0f / smoothingRadius);

    const int nCells = nCells1D * nCells1D;
    gridData.reserve(nParticles * 4);
    gridStart.resize(nCells + 1, 0);
    gridCount.resize(nCells, 0);

    positions.reserve(nParticles);
    predictedPositions.reserve(nParticles);
    velocities.reserve(nParticles);
    densities.reserve(nParticles);
    colors.reserve(nParticles);
    allLambdas.resize(nParticles);
    deltas.resize(nParticles);
    oldPositions.resize(nParticles);

    neighbourData.reserve(nParticles * 64);
    neighbourStart.resize(nParticles + 1, 0);
    neighbourCount.resize(nParticles, 0);

    initialiseParticles(nParticles, 0.018f);
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
        float x = (i % particlesPerRow - particlesPerRow / 2.0f + 0.5f) * spacing;
        float y = (i / particlesPerRow - particlesPerCol / 2.0f + 0.5f) * spacing;
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
        Cell c     = positionToCoord(predictedPositions[i], smoothingRadius);
        int  cellIdx = c.x * nCells1D + c.y;
        ++gridCount[cellIdx];
    }

    // prefix sum → gridStart
    gridStart[0] = 0;
    for (int c = 0; c < nCells; ++c)
        gridStart[c + 1] = gridStart[c] + gridCount[c];

    // pass 2: fill gridData
    gridData.resize(gridStart[nCells]);
    std::vector<int> insertPos(gridStart.begin(), gridStart.begin() + nCells);

    for (int i = 0; i < nParticles; ++i)
    {
        Cell c       = positionToCoord(predictedPositions[i], smoothingRadius);
        int  cellIdx = c.x * nCells1D + c.y;
        gridData[insertPos[cellIdx]++] = i;
    }
}

// ---------------------------------------------------------------------------
// Flat neighbour list: count → prefix sum → fill
// ---------------------------------------------------------------------------
void Particles::buildNeighbours(float smoothingRadius)
{
    // pass 1: count neighbours per particle
    std::fill(neighbourCount.begin(), neighbourCount.end(), 0);

    for (int i = 0; i < nParticles; ++i)
    {
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
                int   j    = gridData[k];
                Vec2  diff = predictedPositions[j] - predictedPositions[i];
                float d2   = diff.dot(diff);
                if (d2 <= h2)
                    ++neighbourCount[i];
            }
        }
    }

    neighbourStart[0] = 0;
    for (int i = 0; i < nParticles; ++i)
        neighbourStart[i + 1] = neighbourStart[i] + neighbourCount[i];

    // pass 2: fill neighbourData
    neighbourData.resize(neighbourStart[nParticles]);
    std::vector<int> insertPos(neighbourStart.begin(), neighbourStart.begin() + nParticles);

    for (int i = 0; i < nParticles; ++i)
    {
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
                int   j    = gridData[k];
                Vec2  diff = predictedPositions[j] - predictedPositions[i];
                float d2   = diff.dot(diff);
                if (d2 <= h2)
                    neighbourData[insertPos[i]++] = j;
            }
        }
    }
}

// ---------------------------------------------------------------------------
float Particles::calculateLambda(size_t i, float smoothingRadius)
{
    // self-contribution to density (distance = 0)
    float density = poly6_norm * h2 * h2 * h2;

    Vec2  gradI      = {0.0f, 0.0f};
    float denominator = 0.0f;

    const Vec2& pi = predictedPositions[i];

    for (int k = neighbourStart[i]; k < neighbourStart[i + 1]; ++k)
    {
        int j = neighbourData[k];
        if (j == (int)i) continue;

        Vec2  diff = pi - predictedPositions[j];
        float d2   = diff.dot(diff);
        if (d2 >= h2 || d2 < 1e-12f) continue;

        // poly6 density contribution
        float sq = h2 - d2;
        density += poly6_norm * sq * sq * sq;

        // spiky gradient (sqrt only for particles inside radius)
        float d   = std::sqrt(d2);
        float s   = spiky_norm * (smoothingRadius - d) * (smoothingRadius - d);
        Vec2  grad = diff * (s / (d * restDensity));
        gradI      += grad;
        denominator += grad.dot(grad);
    }

    denominator += gradI.dot(gradI);

    float Ci = (density / restDensity) - 1.0f;
    return -Ci / (denominator + RELAXATION_F);
}

// ---------------------------------------------------------------------------
void Particles::update(float dt, float smoothingRadius,
                       const float radiusPx, const float g_fb_w, const float g_fb_h)
{
    // 1. apply gravity, predict positions
    oldPositions = positions;
    for (int i = 0; i < nParticles; ++i)
    {
        velocities[i]         += gravity * dt;
        predictedPositions[i]  = positions[i] + velocities[i] * dt;
    }

    // 2. build grid and neighbour list once
    buildGrid(smoothingRadius);
    buildNeighbours(smoothingRadius);

    // 3. solver
    const int solverIterations = 3;
    for (int iter = 0; iter < solverIterations; ++iter)
    {
        // lambda
        for (int i = 0; i < nParticles; ++i)
            allLambdas[i] = calculateLambda(i, smoothingRadius);

        // delta
        for (int i = 0; i < nParticles; ++i)
        {
            Vec2       sum = {0.0f, 0.0f};
            const Vec2& pi = predictedPositions[i];

            for (int k = neighbourStart[i]; k < neighbourStart[i + 1]; ++k)
            {
                int j = neighbourData[k];
                if (j == i) continue;

                Vec2  diff = pi - predictedPositions[j];
                float d2   = diff.dot(diff);
                if (d2 < 1e-12f || d2 >= h2) continue;

                float d          = std::sqrt(d2);
                float s          = spiky_norm * (smoothingRadius - d) * (smoothingRadius - d) / d;
                float lambdaSum  = allLambdas[i] + allLambdas[j];
                sum += diff * (s * lambdaSum);
            }
            deltas[i] = sum / restDensity;
        }

        // apply deltas
        for (int i = 0; i < nParticles; ++i)
        {
            predictedPositions[i] += deltas[i];
            clampToBoundaries(&predictedPositions[i], radiusPx, g_fb_w, g_fb_h);
        }
    }

    // 4. update velocities and positions
    for (int i = 0; i < nParticles; ++i)
    {
        velocities[i] = (predictedPositions[i] - positions[i]) / dt;
        colors[i]     = getColor(velocities[i]);
        positions[i]  = predictedPositions[i];
    }

    // 5. XSPH velocity smoothing
    std::vector<Vec2> newVelocities = velocities;
    const float xsphC = 0.1f;
    const float maxDv = 0.2f;

    for (int i = 0; i < nParticles; ++i)
    {
        Vec2  xsph = {0.0f, 0.0f};
        float wSum = 0.0f;

        for (int k = neighbourStart[i]; k < neighbourStart[i + 1]; ++k)
        {
            int j = neighbourData[k];
            if (j == i) continue;

            Vec2  diff = predictedPositions[i] - predictedPositions[j];
            float d2   = diff.dot(diff);
            if (d2 >= h2) continue;

            float d = std::sqrt(d2);
            float sq = h2 - d2;
            float w  = poly6_norm * sq * sq * sq;  // inlined poly6, no powf

            xsph += (velocities[j] - velocities[i]) * w;
            wSum += w;
        }

        if (wSum > 1e-6f)
            xsph = xsph * (1.0f / wSum);

        Vec2  dv  = xsph * xsphC;
        float mag = dv.magnitude();
        if (mag > maxDv)
            dv = dv * (maxDv / mag);

        newVelocities[i] = velocities[i] + dv;
    }
    velocities = newVelocities;
}

// ---------------------------------------------------------------------------
void Particles::clampToBoundaries(Vec2* pos, const float radiusPx,
                                   const float g_fb_w, const float g_fb_h)
{
    float half[2]    = {radiusPx / g_fb_w, radiusPx / g_fb_h};
    float* arr[2]    = {&pos->x, &pos->y};

    for (int i = 0; i < 2; ++i)
    {
        float lo   = -1.0f + half[i];
        float hi   =  1.0f - half[i];
        *arr[i]    = std::max(lo, std::min(hi, *arr[i]));
    }
}

// ---------------------------------------------------------------------------
float Particles::calculateDensity(size_t i, float smoothingRadius)
{
    float density = poly6_norm * h2 * h2 * h2;  // self

    for (int k = neighbourStart[i]; k < neighbourStart[i + 1]; ++k)
    {
        int j = neighbourData[k];
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

    std::fill(gridCount.begin(),      gridCount.end(),      0);
    std::fill(neighbourCount.begin(), neighbourCount.end(), 0);

    initialiseParticles(nParticles, 0.02f);
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
    Vec2  diff     = a - b;
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
    // TODO: implement for NDC space
    return 0.0f;
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
            float t    = (span > 0.0f) ? (s - ColorStops[i].pos) / span : 0.0f;

            Vec3 lower = {ColorStops[i].r,     ColorStops[i].g,     ColorStops[i].b};
            Vec3 upper = {ColorStops[i+1].r,   ColorStops[i+1].g,   ColorStops[i+1].b};
            return lerp(lower, upper, t);
        }
    }
    return {1.0f, 1.0f, 1.0f};
}
