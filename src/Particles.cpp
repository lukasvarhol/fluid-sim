#include "particles.h"
#include "colors.h"

Particles::Particles(int n, float smoothingRadius)
{
    // TODO: get rid of viewport dimensions
    /*   NTC space: -1.0 < x < 1.0 ; -1.0 < y 1.0 
        (boundary conditions ensure that a particle can never be at value of +/- 1.0f)
    */
    nParticles = n;
    nCells1D = std::ceil( 2.0f / smoothingRadius);
    flattenedGrid.resize(nCells1D * nCells1D);
    positions.reserve(nParticles);
    predictedPositions.reserve(nParticles);
    velocities.reserve(nParticles);
    densities.reserve(nParticles);
    colors.reserve(nParticles);

    initialiseParticles(nParticles, 0.03f);
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
        predictedPositions.push_back(Vec2{0.0f, 0.0f});
        velocities.push_back(Vec2{0.0f, 0.0f});
        colors.push_back(Vec3{1.0f, 1.0f, 1.0f});
        densities.push_back(0.0f);
    }
}

void Particles::update(float dt){
    //wip
    for (size_t idx = 0; idx < nParticles; ++idx){
        Vec2 forces = gravity;
        velocities[idx] += forces * dt;
        predictedPositions[idx] = positions[idx] + velocities[idx] * dt;
        positions[idx] += velocities[idx] * dt;
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
    cell.x = (int)((position.x + 1.0f) / spacing); 
    cell.y = (int)((position.y + 1.0f) / spacing); 
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
            size_t gridIdx = (size_t)(currentCell.x * nCells1D) + currentCell.y;
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
        size_t gridIdx = (size_t)(coord.x * nCells1D) + coord.y;
        if (gridIdx >= flattenedGrid.size()) {
            std::cout << "OUT OF BOUNDS\n";
            continue;
        }
        flattenedGrid[gridIdx].push_back(i);
    }
}

