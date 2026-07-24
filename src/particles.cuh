#pragma once
#include "linear_algebra.h"
#include "objects3d/sdf_collision.h"
#include "cuda_buffers.cuh"
#include "particles.h"
#include <vector>

void gpuGravityPredict(CudaBuffers& cb, float gravity,
		       float mouseStrength, float mouseRadius, Vec3 rayOrigin,
		       Vec3 rayDir, float dt, int n);

void gpuBuildGrid(CudaBuffers &cb, float smoothingRadius, int numCells1D,
                  int activeParticles);

void gpuBuildNeighbours(CudaBuffers &cb, float smoothingRadius, float skinRadius2,
                        int numCells1D, int activeParticles, bool forceRebuild=false);

void gpuCalculateLambda(CudaBuffers &cb, float relaxation,
                        float restDensity, float smoothingRadius,
                        int activeParticles);

void gpuCalculateDeltas(CudaBuffers &cb, float restDensity,
                        float wdq, float scorr, float smoothingRadius, int activeParticles);

void gpuClampToBoundaries(CudaBuffers &cb, float radiusPx,
                          int g_fb_w, int g_fb_h, int activeParticles);

void gpuProjectParticleSDF(CudaBuffers &cb, int activeParticles);

void gpuProjectParticleTri(CudaBuffers &cb, Vec3 *closestPoints_h, int activeParticles);

void gpuUpdateVelocities(CudaBuffers &cb, Vec3 *positions_h,
                         float dt, float mSpeed, int activeParticles);

void gpuViscosity(CudaBuffers &cb, Vec3 *velocities_h, float h2, float xsphC,
                  int activeParticles);

void gpuEstimateRestDensity(CudaBuffers& cb, float* density_h, float smoothingRadius, int numParticles);

void gpuVorticity(CudaBuffers& cb, float smoothingRadius, float vorticityEpsilon, float dt, int activeParticles);
