#pragma once
#include "linear_algebra.h"
#include "objects3d/sdf_collision.h"
#include "cuda_buffers.cuh"
#include "particles.h"
#include <vector>

void gpuGravityPredict(CudaBuffers& cb, Vec3* positions, Vec3* velocities, Vec3* predictedPositions, float gravity,
		       float mouseStrength, float mouseRadius, Vec3 rayOrigin,
		       Vec3 rayDir, float dt, int n);

void gpuBuildGrid(CudaBuffers &cb, int *gridStart_h, int *gridCount_h,
                  int *gridData_h, float smoothingRadius, int numCells1D,
                  int activeParticles);

void gpuBuildNeighbours(CudaBuffers &cb, int *neighbourData_h,
                        int *neighbourCount_h, std::vector<Vec3> &positionsAtLastBuild_h, float smoothingRadius, float skinRadius2,
                        int numCells1D, int activeParticles);

void gpuCalculateLambda(CudaBuffers &cb, float relaxation,
                        float restDensity, float smoothingRadius,
                        int activeParticles);

void gpuCalculateDeltas(CudaBuffers &cb, float restDensity,
                        float wdq, float scorr, float smoothingRadius, int activeParticles);

void gpuClampToBoundaries(CudaBuffers &cb, Vec3* predictedPositions_h, float radiusPx,
                          int g_fb_w, int g_fb_h, int activeParticles);

void gpuProjectParticleSDF(CudaBuffers &cb, Vec3 *predictedPositions_h,
                           Vec3 *velocities_h, int activeParticles);

void gpuUpdateVelocities(CudaBuffers &cb, Vec3* predictedPositions_h, Vec3 *positions_h, Vec3 *velocities_h,
                         float dt, int activeParticles);
