#include <vector>
#include <iostream>
#include <unordered_map>
#include <array>
#include "linear_algebra.h"
#include "helpers.h"

struct Particles{
	int num_particles;
	std::vector<Vec2> positions;
	std::vector<Vec2> velocities;
	std::vector<Vec2> densities;
	std::vector<std::vector<int>> spatial_lut; // store particle indexes in each cell
	Particles();

private:
	float spikyKernel(float smoothing_radius, float distance); // used for gradient
	float poly6Kernel(float smoothing_radius, float distance); // used for density
	float calculateDistance(Vec2 posA, Vec2 posB);
	float calculateDensity(size_t particleIdx, std::vector<int> neighbours, float smoothingRadius);
	std::array<int, 2> positionToCoord(Vec2 position, float smoothingRadius);
	std::vector<int> getNeighbours(int particleIdx, float smoothingRadius);
};