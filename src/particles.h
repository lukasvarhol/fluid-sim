#include <vector>
#include <iostream>
#include <unordered_map>
#include <array>
#include "linear_algebra.h"
#include "helpers.h"
#include "cell.h"

struct Particles{
	int num_particles;
	std::vector<Vec2> positions;
	std::vector<Vec2> velocities;
	std::vector<float> densities;
	std::vector<Vec3> colors;
	unsigned int nCols, nRows;
	std::vector<std::vector<size_t>> cells; // store particle indexes in each cell
	std::vector<CellEntry> spatial_lut; // map hash to index
	Particles(int nParticles, int g_fb_w, int g_fb_h, float smoothingRadius);

private:
	void initialiseParticles(int nParticles, float spacing);
	float spikyKernel(float smoothing_radius, float distance); // used for gradient
	float poly6Kernel(float smoothing_radius, float distance); // used for density
	float calculateDistance(Vec2 posA, Vec2 posB);
	float calculateDensity(size_t particleIdx, std::vector<int> neighbours, float smoothingRadius);
	Cell positionToCoord(Vec2 position, float smoothingRadius);
	unsigned int hashCell(Cell cell);
	std::vector<size_t> getNeighbours(size_t particleIdx, float smoothingRadius);
	void updateSpatialLut(int nParticles, float smoothingRadius);
};