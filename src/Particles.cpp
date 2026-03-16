#include "particles.h"
#include "colors.h"

Particles::Particles(int n, float smoothingRadius)
{
    /*   NTC space: -1.0 < x < 1.0 ; -1.0 < y 1.0 
        (boundary conditions ensure that a particle can never be at value of +/- 1.0f)
    */
    nParticles = n;
    nCells1D = std::ceil(2.0f / smoothingRadius);
    flattenedGrid.resize(nCells1D * nCells1D);
    positions.reserve(nParticles);
    predictedPositions.reserve(nParticles);
    velocities.reserve(nParticles);
    densities.reserve(nParticles);
    colors.reserve(nParticles);
    allNeighbours.resize(nParticles);
    allLambdas.resize(nParticles);
    deltas.resize(nParticles);
    oldPositions.resize(nParticles);

    for (auto& nbrs : allNeighbours)
        nbrs.reserve(64);

    initialiseParticles(nParticles, 0.018f);
    updateGrid(nParticles, smoothingRadius);
    restDensity = estimateRestDensity(smoothingRadius);
}

void Particles::initialiseParticles(int nParticles, float spacing)
{
    int particlesPerRow = (int)std::sqrt(nParticles);
    int particlesPerCol = (nParticles - 1) / particlesPerRow + 1;

    for (int i = 0; i < nParticles; ++i)
    {
        float x = (i % particlesPerRow - particlesPerRow / 2.0f + 0.5f) * spacing;
        float y = (i / particlesPerRow - particlesPerCol / 2.0f + 0.5f) * spacing;
        positions.push_back(Vec2{x, y});
        predictedPositions.push_back(Vec2{ x, y });
        velocities.push_back(Vec2{0.0f, 0.0f});
        colors.push_back(Vec3{0.0f, 0.0f, 1.0f});
        densities.push_back(0.0f);
    }
    
}

void Particles::update(float dt, float smoothingRadius, const float radiusPx, const float g_fb_w, const float g_fb_h) {
    // steps 1-4: apply forces, predict positions
    oldPositions = positions;
    for (size_t i = 0; i < nParticles; ++i) {
        velocities[i] += gravity * dt;
        predictedPositions[i] = positions[i] + velocities[i] * dt;
    }

    // steps 5-7: find neighbours
    for (size_t i = 0; i < nParticles; ++i) {
        allNeighbours[i].clear();
        deltas[i] = Vec2{ 0.0f, 0.0f };
    }

    // steps 8-19: solver loop
    const int solverIterations = 3;
    for (int iter = 0; iter < solverIterations; ++iter) {
        for (auto& cell : flattenedGrid) cell.clear();
        updateGrid(nParticles, smoothingRadius);

        for (size_t i = 0; i < nParticles; ++i)
           getNeighbours(i, smoothingRadius, allNeighbours[i]);

        for (size_t i = 0; i < nParticles; ++i)
            allLambdas[i] = calculateLambda(i, allNeighbours[i], smoothingRadius);

        for (size_t i = 0; i < nParticles; ++i) {
            Vec2 sum = Vec2{ 0.0f, 0.0f };

            for (size_t j : allNeighbours[i]) {
                if (j == i) continue;

                float corr = scorr(predictedPositions[i], predictedPositions[j], smoothingRadius);
                float lambdaSum = allLambdas[i] + allLambdas[j] + corr;
                sum += spikyKernelGradVec(predictedPositions[i], predictedPositions[j], smoothingRadius) * lambdaSum;
            }

            Vec2 delta = sum / restDensity;

            deltas[i] = delta;
        }

        for (size_t i = 0; i < nParticles; ++i) {
            predictedPositions[i] += deltas[i];
            clampToBoundaries(&predictedPositions[i], radiusPx, g_fb_w, g_fb_h);
        }
    }

    for (size_t i = 0; i < nParticles; ++i) {
        velocities[i] = (predictedPositions[i] - positions[i]) / dt;
        colors[i] = getColor(velocities[i]);
        positions[i] = predictedPositions[i];
    }

    std::vector<Vec2> newVelocities = velocities;
    const float xsphC = 0.1f;

    for (size_t i = 0; i < nParticles; ++i) {
        Vec2 xsph{ 0.0f, 0.0f };
        float wSum = 0.0f;

        for (size_t j : allNeighbours[i]) {
            if (j == i) continue;

            float d = calculateDistance(predictedPositions[i], predictedPositions[j]);
            float w = poly6Kernel(smoothingRadius, d);

            xsph += (velocities[j] - velocities[i]) * w;
            wSum += w;
        }

        if (wSum > 1e-6f)
            xsph = xsph * (1.0f / wSum);

        Vec2 dv = xsph * xsphC;
        float maxDv = 0.2f;
        float mag = dv.magnitude();
        if (mag > maxDv)
            dv = dv * (maxDv / mag);

        newVelocities[i] = velocities[i] + dv;
    }
    velocities = newVelocities;

}

void Particles::clampToBoundaries(Vec2* pos, const float radiusPx, const float g_fb_w, const float g_fb_h) {
    float half[2] = {
        radiusPx / g_fb_w,
        radiusPx / g_fb_h
    };
    float* pos_arr[2] = { &pos->x, &pos->y };

    for (int i = 0; i < 2; i++) {
        float lo = -1.0f + half[i];
        float hi = 1.0f - half[i];
        *pos_arr[i] = std::max(lo, std::min(hi, *pos_arr[i]));
    }
}

// use for gradient
float Particles::spikyKernelGrad(float smoothingRadius, float distance)
{
    if (distance > smoothingRadius)
        return 0;
    float influence = -30.0f / (PI * std::powf(smoothingRadius, 5));
    return influence * (smoothingRadius - distance) * (smoothingRadius - distance);
}

Vec2 Particles::spikyKernelGradVec(Vec2 a, Vec2 b, float smoothingRadius) {
    Vec2 diff = a - b;
    float distance = diff.magnitude();
    //std::cout << "dist: " << distance << "\n";
    if (distance <= 1e-6f) return Vec2{ 0.0f, 0.0f };
    float scalar = spikyKernelGrad(smoothingRadius, distance);
    Vec2 dir = diff * (1.0f / distance); 
    return dir * scalar;
}

// use for densities
float Particles::poly6Kernel(float smoothingRadius, float distance)
{
    if (distance > smoothingRadius)
        return 0;
    float influence = 4 / (PI * std::powf(smoothingRadius, 8));
    float sqrDiff = (smoothingRadius * smoothingRadius) - (distance * distance);
    return influence * sqrDiff * sqrDiff * sqrDiff;
}

float Particles::calculateDistance(Vec2 posA, Vec2 posB)
{
    return (posA - posB).magnitude();
}

float Particles::calculateDensity(size_t particleIdx, const std::vector<size_t>& neighbours, float smoothingRadius)
{
    float density = poly6Kernel(smoothingRadius, 0.0f);

    for (size_t j : neighbours)
    {
        if (j == particleIdx) continue;
        density += poly6Kernel(
            smoothingRadius,
            calculateDistance(predictedPositions[j], predictedPositions[particleIdx])
        );
    }
    return density;
}

Cell Particles::positionToCoord(Vec2 position, float smoothingRadius)
{
    Cell cell;
    int cx = (int)((position.x + 1.0f) / smoothingRadius);
    int cy = (int)((position.y + 1.0f) / smoothingRadius);

    cx = std::max(0, std::min(nCells1D - 1, cx));
    cy = std::max(0, std::min(nCells1D - 1, cy));

    cell.x = cx;
    cell.y = cy;
    return cell;
}

void Particles::getNeighbours(size_t particleIdx, float smoothingRadius, std::vector<size_t>& neighbours)
{
    neighbours.clear();

    Cell cell = positionToCoord(predictedPositions[particleIdx], smoothingRadius);
    float h2 = smoothingRadius * smoothingRadius;

    for (int offsetX = -1; offsetX <= 1; ++offsetX)
    {
        for (int offsetY = -1; offsetY <= 1; ++offsetY)
        {
            int cx = static_cast<int>(cell.x) + offsetX;
            int cy = static_cast<int>(cell.y) + offsetY;

            if (cx < 0 || cy < 0 ||
                cx >= static_cast<int>(nCells1D) || cy >= static_cast<int>(nCells1D))
                continue;

            size_t gridIdx = static_cast<size_t>(cx) * nCells1D + static_cast<size_t>(cy);

            for (size_t idx : flattenedGrid[gridIdx])
            {
                Vec2 diff = predictedPositions[idx] - predictedPositions[particleIdx];
                float d2 = diff.dot(diff);
                if (d2 <= h2)
                    neighbours.push_back(idx);
            }
        }
    }
}

void Particles::updateGrid(int nParticles, float smoothingRadius)
{
    for (size_t i = 0; i < predictedPositions.size(); ++i)
    {
        Cell coord = positionToCoord(predictedPositions[i], smoothingRadius);
        size_t gridIdx = static_cast<size_t>(coord.x) * nCells1D + static_cast<size_t>(coord.y);

        if (gridIdx >= flattenedGrid.size()) {
            continue;
        }

        flattenedGrid[gridIdx].push_back(i);
    }
}

float Particles::calculateLambda(size_t particleIdx, const std::vector<size_t>& neighbours, float smoothingRadius) {
    float density = calculateDensity(particleIdx, neighbours, smoothingRadius);
    float Ci = (density / restDensity) - 1.0f;

    float denominator = 0.0f;
    Vec2 gradI = Vec2{ 0.0f, 0.0f }; 

    for (size_t j : neighbours) {
        if (j == particleIdx) continue;
        Vec2 g = spikyKernelGradVec(predictedPositions[particleIdx], predictedPositions[j], smoothingRadius);
        g = g * (1.0f / restDensity);
        gradI += g;           // accumulate for k=i case
        denominator += g.dot(g); // k=j case: each neighbour's own gradient
    }
    denominator += gradI.dot(gradI); // add k=i term

    float lambda = -Ci / (denominator + RELAXATION_F);

    return lambda;
}

float Particles::estimateRestDensity(float smoothingRadius)
{
    size_t center = nParticles / 2;
    std::vector<size_t> neighbours;
    neighbours.reserve(64);
    getNeighbours(center, smoothingRadius, neighbours);
    return calculateDensity(center, neighbours, smoothingRadius);
}

float Particles::scorr(Vec2 pi, Vec2 pj, float h) {
    // TODO: fix this for NTC space
    float q = 0.2f * h;
    float d = (pi - pj).magnitude();

    float num = poly6Kernel(h, d);
    float den = poly6Kernel(h, q);
    if (den <= 1e-8f) return 0.0f;

    float ratio = num / den;
    float ratio2 = ratio * ratio;
    float ratio4 = ratio2 * ratio2;

    const float k = 5e-5f;
    return 0.0f;
}

void Particles::reset(float smoothingRadius)
{
    positions.clear();
    predictedPositions.clear();
    velocities.clear();
    densities.clear();
    colors.clear();

    for (auto& cell : flattenedGrid)
        cell.clear();

    for (auto& nbrs : allNeighbours)
        nbrs.clear();

    initialiseParticles(nParticles, 0.02f);
    updateGrid(nParticles, smoothingRadius);
    restDensity = estimateRestDensity(smoothingRadius);
}

Vec3 Particles::getColor(Vec2& vel) {
    float magnitude = vel.magnitude();
    float s = std::clamp(magnitude / MAX_SPEED, 0.0f, 1.0f);

    for (size_t i = 0; i + 1 < ColorStops.size(); ++i) {
        if (s >= ColorStops[i].pos && s <= ColorStops[i + 1].pos) {
            float span = (ColorStops[i + 1].pos - ColorStops[i].pos);
            float t = (span > 0.0f) ? (s - ColorStops[i].pos) / span : 0.0f;

            Vec3 lower = {
                ColorStops[i].r,
                ColorStops[i].g,
                ColorStops[i].b
            };
            Vec3 upper = {
                ColorStops[i + 1].r,
                ColorStops[i + 1].g,
                ColorStops[i + 1].b
            };

            return lerp(lower, upper, t);
        }
    }
    return { 1.0f, 1.0f, 1.0f };
}