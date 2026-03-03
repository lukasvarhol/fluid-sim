#include "particles.h"
#include "colors.h"

// use for gradient
float Particles::spikyKernel(float smoothingRadius, float distance) {
	if (distance > smoothingRadius) return 0;
	float influence = 15 / (PI * std::powf(smoothingRadius, 6));
	return influence * (smoothingRadius - distance) * (smoothingRadius - distance) * (smoothingRadius - distance);
}

// use for densities
float Particles::poly6Kernel(float smoothingRadius, float distance) {
	if (distance > smoothingRadius) return 0;
	float influence = 315 / (64 * PI * std::powf(smoothingRadius, 9));
	float sqrDiff = (smoothingRadius * smoothingRadius) - (distance * distance);
	return influence * sqrDiff * sqrDiff * sqrDiff;
}

float Particles::calculateDistance(Vec2 posA, Vec2 posB) {
	return (posA - posB).magnitude();
}

float Particles::calculateDensity(size_t particleIdx, std::vector<int> neighbours, float smoothingRadius) {
	float density;
	for (size_t i = 0; i < neighbours.size(); ++i) {
		if (neighbours[i] == particleIdx) continue;
		density += poly6Kernel(smoothingRadius,calculateDistance(positions[neighbours[i]], positions[particleIdx]));
		// mass neglected as it is just 1.0f
	}
	return density;
}

std::array<int, 2> Particles::positionToCoord(Vec2 position, float smoothingRadius) {
	std::array<int, 2> coord;
	float spacing = 2.0f * smoothingRadius;
	coord[0] = (int)std::floor(position.x / spacing);
	coord[1] = (int)std::floor(position.y / spacing);
	return coord;
}

std::vector<int> Particles::getNeighbours(int particleIdx, float smoothingRadius) {
	std::array<int,2> cell = positionToCoord(positions[particleIdx], smoothingRadius);
	// need some way of converting a cell coord to an index in spatial_lut
		// hash x and y values to get a unique cell key? Then how do I map that back to an index?
	// the index in the spatial_lut can be gotten by i = coord_x * numCols + coord_y
}