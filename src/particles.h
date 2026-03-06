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
    void update(float dt);

private:
    unsigned int nCells1D;
    float restDensity = 100.0f;
    Vec2 gravity = Vec2{0.0f, -2.5f};
    std::vector<std::vector<size_t>> flattenedGrid; // store particle indexes

    void initialiseParticles(int nParticles, float spacing);
    float spikyKernelGrad(float smoothing_radius, float distance); // used for gradient
    float poly6Kernel(float smoothing_radius, float distance); // used for density
    float calculateDistance(Vec2 posA, Vec2 posB);
    float calculateDensity(size_t particleIdx, std::vector<int> neighbours, float smoothingRadius);
    Cell positionToCoord(Vec2 position, float smoothingRadius);
    unsigned int hashCell(Cell cell);
    std::vector<size_t> getNeighbours(size_t particleIdx, float smoothingRadius);
    void updateGrid(int nParticles, float smoothingRadius);
};