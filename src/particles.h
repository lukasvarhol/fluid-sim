#include <vector>
#include <iostream>
#include <unordered_map>
#include <array>
#include "linear_algebra.h"
#include "helpers.h"
#include "cell.h"
#include <algorithm>

struct Particles
{
    int nParticles;
    std::vector<Vec2> positions;
    std::vector<Vec2> predictedPositions;
    std::vector<Vec2> velocities;
    std::vector<float> densities;
    std::vector<Vec3> colors;
    Particles(int nParticles, float smoothingRadius);
    void update(float dt, float smoothingRadius, const float radiusPx, const float g_fb_w, const float g_fb_h);

private:
    int nCells1D;
    const float restDensity = 200.0f;
    const float ENERGY_RETENTION_F = 0.5f;
    const float RELAXATION_F = 1.0f;
    const Vec2 gravity = Vec2{0.0f, -0.1f};
    std::vector<std::vector<size_t>> flattenedGrid; // store particle indexes

    void keepInBoundaries(Vec2* pos, Vec2* vel, const float radiusPx, const float g_fb_w, const float g_fb_h);
    void initialiseParticles(int nParticles, float spacing);
    float spikyKernelGrad(float smoothing_radius, float distance); // used for gradient
    Vec2 spikyKernelGradVec(Vec2 a, Vec2 b, float smoothingRadius);
    float poly6Kernel(float smoothing_radius, float distance); // used for density
    float calculateDistance(Vec2 posA, Vec2 posB);
    float calculateDensity(size_t particleIdx, std::vector<size_t> neighbours, float smoothingRadius);
    Cell positionToCoord(Vec2 position, float smoothingRadius);
    std::vector<size_t> getNeighbours(size_t particleIdx, float smoothingRadius);
    void updateGrid(int nParticles, float smoothingRadius);
    float calculateLambda(size_t particleIdx, std::vector<size_t> neighbours, float smoothingRadius);
    Vec2 calculateDeltaPos(size_t particleIdx, float smoothingRadius);
};