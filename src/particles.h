#pragma once
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
    std::vector<Vec2>  positions;
    std::vector<Vec2>  predictedPositions;
    std::vector<Vec2>  velocities;
    std::vector<float> densities;
    std::vector<Vec3>  colors;
    std::vector<float> allLambdas;
    std::vector<Vec2>  deltas;
    std::vector<Vec2>  oldPositions;

    std::vector<int> neighbourData;   
    std::vector<int> neighbourStart;  
    std::vector<int> neighbourCount;  

    float h2, h5, h8, poly6_norm, spiky_norm;

    Particles(int nParticles, float smoothingRadius);
    void update(float dt, float smoothingRadius, const float radiusPx, const float g_fb_w, const float g_fb_h);
    void reset(float smoothingRadius);

private:
    int   nCells1D;
    float restDensity;

    const float RELAXATION_F      = 1000.0f;
    const float ENERGY_RETENTION_F = 0.7f;
    const float MAX_SPEED          = 3.0f;
    const Vec2  gravity            = Vec2{0.0f, -2.6f};

    std::vector<int> gridData;   
    std::vector<int> gridStart;  
    std::vector<int> gridCount;  

    void  clampToBoundaries(Vec2* pos, const float radiusPx, const float g_fb_w, const float g_fb_h);
    void  initialiseParticles(int nParticles, float spacing);
    float spikyKernelGrad(float smoothingRadius, float distance);
    Vec2  spikyKernelGradVec(Vec2 a, Vec2 b, float smoothingRadius);
    float poly6Kernel(float smoothingRadius, float distance);
    float calculateDistance(Vec2 posA, Vec2 posB);
    float calculateDensity(size_t particleIdx, float smoothingRadius);
    Cell  positionToCoord(Vec2 position, float smoothingRadius);
    void  buildGrid(float smoothingRadius);
    void  buildNeighbours(float smoothingRadius);
    float calculateLambda(size_t particleIdx, float smoothingRadius);
    float estimateRestDensity(float smoothingRadius);
    float scorr(Vec2 pi, Vec2 pj, float h);
    Vec3  getColor(Vec2& vel);
};
