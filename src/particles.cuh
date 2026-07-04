#pragma once
#include "linear_algebra.h"
#include "cuda_buffers.cuh"
#include "particles.h"
#include <vector>

void gpuGravityPredict(CudaBuffers& cb, std::vector<Vec3> &positions_h,
                    std::vector<Vec3> &velocities_h,
                    std::vector<Vec3> &predictedPositions_h, float gravity,
                    float mouseStrength, float mouseRadius, Vec3 rayOrigin,
                    Vec3 rayDir, float dt, int n);

void gpuBuildGrid(CudaBuffers& cb, float smoothingRadius, int numCells1D, int activeParticles);
