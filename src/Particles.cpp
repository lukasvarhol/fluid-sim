#include "particles.h"
#include "colors.h"

Particles::Particles(int nParticles, int g_fb_w, int g_fb_h, float smoothingRadius)
{
    nCols = (int)(g_fb_w / (smoothingRadius * 2.0f));
    nRows = (int)(g_fb_h / (smoothingRadius * 2.0f));
    flattenedGrid.reserve(nCols * nRows);
    positions.reserve(nParticles);
    velocities.reserve(nParticles);
    densities.reserve(nParticles);
    colors.reserve(nParticles);

    initialiseParticles(nParticles, 0.02f);
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
        positions[i] = {x, y};
        velocities[i] = {0.0f, 0.0f};
        colors[i] = {0.0f, 0.0f, 1.0f};
        densities[i] = 0.0f;
    }
}

// use for gradient
float Particles::spikyKernel(float smoothingRadius, float distance)
{
    if (distance > smoothingRadius)
        return 0;
    float influence = 15 / (PI * std::powf(smoothingRadius, 6));
    return influence * (smoothingRadius - distance) * (smoothingRadius - distance) * (smoothingRadius - distance);
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

float Particles::calculateDensity(size_t particleIdx, std::vector<int> neighbours, float smoothingRadius)
{
    float density = 0.0f;
    for (size_t i = 0; i < neighbours.size(); ++i)
    {
        if (neighbours[i] == particleIdx)
            continue;
        density += poly6Kernel(smoothingRadius, calculateDistance(positions[neighbours[i]], positions[particleIdx]));
        // mass neglected as it is just 1.0f
    }
    return density;
}

Cell Particles::positionToCoord(Vec2 position, float smoothingRadius)
{
    Cell cell;
    float spacing = 2.0f * smoothingRadius;
    cell.x = (int)std::floor(position.x / spacing);
    cell.y = (int)std::floor(position.y / spacing);
    return cell;
}

std::vector<size_t> Particles::getNeighbours(size_t particleIdx, float smoothingRadius)
{
    std::vector<size_t> neighbours;
    Cell cell = positionToCoord(positions[particleIdx], smoothingRadius);
    for (int offsetX = -1; offsetX <= 1; ++offsetX)
    {
        for (int offsetY = -1; offsetY <= 1; ++offsetY)
        {
            Cell currentCell = cell + Cell{offsetX, offsetY};
            size_t gridIdx = currentCell.x * nRows + currentCell.y;
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
    for (size_t i = 0; i < positions.size(); ++i)
    {
        Cell coord = positionToCoord(positions[i], smoothingRadius);
        size_t gridIdx = (size_t)(coord.x * nRows) + coord.y;
        flattenedGrid[gridIdx].push_back(i);
    }
}