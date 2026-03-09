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

    initialiseParticles(nParticles, smoothingRadius * 0.5f);
    updateGrid(nParticles, smoothingRadius);
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
        colors.push_back(Vec3{1.0f, 1.0f, 1.0f});
        densities.push_back(0.0f);
    }
}

void Particles::update(float dt, float smoothingRadius, const float radiusPx, const float g_fb_w, const float g_fb_h) {
    // steps 1-4: apply forces, predict positions
    std::vector<Vec2> oldPositions = positions;
    for (size_t i = 0; i < nParticles; ++i) {
        velocities[i] += gravity * dt;
        predictedPositions[i] = positions[i] + velocities[i] * dt;
    }

    // steps 5-7: find neighbours
    for (auto& cell : flattenedGrid) cell.clear();
    updateGrid(nParticles, smoothingRadius);
    std::vector<std::vector<size_t>> allNeighbours(nParticles);
    for (size_t i = 0; i < nParticles; ++i)
        allNeighbours[i] = getNeighbours(i, smoothingRadius);

    // steps 8-19: solver loop
    const int solverIterations = 3;
    for (int iter = 0; iter < solverIterations; ++iter) {
        // step 10: calculate all lambdas
        std::vector<float> allLambdas(nParticles);
        for (size_t i = 0; i < nParticles; ++i)
            allLambdas[i] = calculateLambda(i, allNeighbours[i], smoothingRadius);

        // steps 13-18: calculate deltas, handle collisions, update predicted positions
        for (size_t i = 0; i < nParticles; ++i) {
            Vec2 sum = Vec2{ 0.0f, 0.0f };
            for (size_t j : allNeighbours[i]) {
                if (j == i) continue;
                float lambdaSum = allLambdas[i] + allLambdas[j];
                sum += spikyKernelGradVec(predictedPositions[i], predictedPositions[j], smoothingRadius) * lambdaSum;
            }
            Vec2 delta = sum / restDensity;
            predictedPositions[i] += delta;
            keepInBoundaries(&predictedPositions[i], &velocities[i], radiusPx, g_fb_w, g_fb_h);
        }
    }

    // steps 21-23: update velocity and commit positions
    for (size_t i = 0; i < nParticles; ++i) {
        // velocities[i] = (predictedPositions[i] - oldPositions[i]) / dt;
        positions[i] = predictedPositions[i];
    }
}

void Particles::keepInBoundaries(Vec2* pos, Vec2* vel, const float radiusPx, const float g_fb_w, const float g_fb_h) {
    float half[2] = {
        radiusPx / g_fb_w,
        radiusPx / g_fb_h
    };
    float* pos_arr[2] = { &pos->x, &pos->y };
    float* vel_arr[2] = { &vel->x, &vel->y };

    for (int i = 0; i < 2; i++) {
        float lo = -1.0f + half[i];
        float hi = 1.0f - half[i];

        if (*pos_arr[i] <= lo) {
            *pos_arr[i] = lo;
            if (*vel_arr[i] < 0) *vel_arr[i] *= -ENERGY_RETENTION_F;
        }
        else if (*pos_arr[i] >= hi) {
            *pos_arr[i] = hi;
            if (*vel_arr[i] > 0) *vel_arr[i] *= -ENERGY_RETENTION_F;
        }
    }
}

// use for gradient
float Particles::spikyKernelGrad(float smoothingRadius, float distance)
{
    if (distance > smoothingRadius)
        return 0;
    float influence = -45.0f / (PI * std::powf(smoothingRadius, 6));
    return influence * (smoothingRadius - distance) * (smoothingRadius - distance);
}

Vec2 Particles::spikyKernelGradVec(Vec2 a, Vec2 b, float smoothingRadius) {
    Vec2 diff = a - b;
    float distance = diff.magnitude();
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
    float influence = 315 / (64 * PI * std::powf(smoothingRadius, 9));
    float sqrDiff = (smoothingRadius * smoothingRadius) - (distance * distance);
    return influence * sqrDiff * sqrDiff * sqrDiff;
}

float Particles::calculateDistance(Vec2 posA, Vec2 posB)
{
    return (posA - posB).magnitude();
}

float Particles::calculateDensity(size_t particleIdx, std::vector<size_t> neighbours, float smoothingRadius)
{
    float density = 0.0f;
    for (size_t i = 0; i < neighbours.size(); ++i)
    {
        if (neighbours[i] == particleIdx) continue;
        density += poly6Kernel(smoothingRadius, calculateDistance(predictedPositions[neighbours[i]], predictedPositions[particleIdx]));
        // mass neglected as it is just 1.0f
    }
    return density;
}

Cell Particles::positionToCoord(Vec2 position, float smoothingRadius)
{
    Cell cell;
    float spacing = 2.0f * smoothingRadius;
    cell.x = (int)((position.x + 1.0f) / smoothingRadius);
    cell.y = (int)((position.y + 1.0f) / smoothingRadius);
    return cell;
}

std::vector<size_t> Particles::getNeighbours(size_t particleIdx, float smoothingRadius)
{
    std::vector<size_t> neighbours;

    Cell cell = positionToCoord(predictedPositions[particleIdx], smoothingRadius);

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
                neighbours.push_back(idx);
            }
        }
    }

    return neighbours;
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

float Particles::calculateLambda(size_t particleIdx, std::vector<size_t> neighbours, float smoothingRadius) {
    float Ci = (calculateDensity(particleIdx, neighbours, smoothingRadius) / restDensity) - 1.0f;

    float denominator = 0.0f;
    Vec2 gradI = Vec2{ 0.0f, 0.0f }; // gradient w.r.t. particle i (k=i case)

    for (size_t j : neighbours) {
        if (j == particleIdx) continue;
        Vec2 g = spikyKernelGradVec(predictedPositions[particleIdx], predictedPositions[j], smoothingRadius);
        g = g * (1.0f / restDensity);
        gradI += g;           // accumulate for k=i case
        denominator += g.dot(g); // k=j case: each neighbour's own gradient
    }
    denominator += gradI.dot(gradI); // add k=i term

    return -Ci / (denominator + RELAXATION_F);
}

Vec2 Particles::calculateDeltaPos(size_t particleIdx, float smoothingRadius) {
    Vec2 sum = Vec2{ 0.0f, 0.0f };
    std::vector<size_t> neighbours = getNeighbours(particleIdx, smoothingRadius);
    for (size_t i = 0; i < neighbours.size(); ++i) {
        std::vector<size_t> neighboursNeighbours = getNeighbours(neighbours[i], smoothingRadius);
        float lambdaSum = calculateLambda(particleIdx, neighbours, smoothingRadius) + calculateLambda(neighbours[i], neighboursNeighbours, smoothingRadius);
        sum += spikyKernelGradVec(predictedPositions[particleIdx], predictedPositions[neighbours[i]], smoothingRadius) * lambdaSum;
    }
    return sum / restDensity;
}
