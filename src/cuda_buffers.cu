#include "cuda_buffers.cuh"
#include "particles.h"

CudaBuffers::CudaBuffers(Particles& particles) {
  size_t size = particles.numParticles * sizeof(Vec3);
  int numCells =
    particles.numCells1D * particles.numCells1D * particles.numCells1D;
  size_t spaceGridSize = numCells * sizeof(int);

  size_t activeSize = particles.activeParticles * sizeof(int);

  blocksPerGridL1 = 1 + (numCells - 1) / (BLOCK_SIZE << 1);
  blocksPerGridL2 = 1 + blocksPerGridL1 / (BLOCK_SIZE << 1);
  blocksPerGridL3 = 1 + blocksPerGridL2 / (BLOCK_SIZE << 1);
  
  HANDLE_ERROR(cudaMalloc((void **)&positions_d, size));
  HANDLE_ERROR(cudaMalloc((void **)&velocities_d, size));
  HANDLE_ERROR(cudaMalloc((void **)&predictedPositions_d, size));
  HANDLE_ERROR(cudaMalloc((void **)&gridCount_d, spaceGridSize));
  HANDLE_ERROR(cudaMalloc((void **)&gridStart_d, (numCells + 1) * sizeof(int)));
  HANDLE_ERROR(cudaMalloc((void **)&gridData_d, activeSize));
  HANDLE_ERROR(cudaMalloc((void **)&neighbourData_d, activeSize * MAX_NEIGHBOURS));
  HANDLE_ERROR(cudaMalloc((void **)&neighbourCount_d, activeSize));
  HANDLE_ERROR(cudaMalloc((void **)&positionsAtLastBuild_d,
			  particles.activeParticles * sizeof(Vec3)));
  HANDLE_ERROR(cudaMalloc((void **)&buildNeighbours_d, sizeof(int)));
  HANDLE_ERROR(cudaMalloc((void **)&allLambdas_d,
			  sizeof(float) * particles.activeParticles));
  HANDLE_ERROR(cudaMalloc((void **)&deltas_d, sizeof(Vec3) * particles.activeParticles));
  HANDLE_ERROR(cudaMalloc((void **)&colliders_d, sizeof(SDFCollider) * MAX_OBJECTS));
  HANDLE_ERROR(cudaMalloc((void **)&vorticities_d,
                          sizeof(Vec3) * particles.activeParticles));
  HANDLE_ERROR(
      cudaMalloc((void **)&triColliders_d, sizeof(TriCollider) * MAX_OBJECTS));
  HANDLE_ERROR(
      cudaMalloc((void **)&closestPoints_d, sizeof(Vec3) * MAX_OBJECTS));

  handleCellGridUpdate(particles.numCells1D);

  if (blocksPerGridL2 == 1) {
    HANDLE_ERROR(cudaMalloc((void **)&sumsL1_d, blocksPerGridL1 * sizeof(int)));
    HANDLE_ERROR(cudaMalloc((void **)&incrL1_d, numCells * sizeof(int)));
  } else if (blocksPerGridL3 == 1) {
    HANDLE_ERROR(cudaMalloc((void **)&sumsL1_d, blocksPerGridL1 * sizeof(int)));
    HANDLE_ERROR(cudaMalloc((void **)&incrL1_d, numCells * sizeof(int)));
    HANDLE_ERROR(cudaMalloc((void **)&sumsL2_d, blocksPerGridL2 * sizeof(int)));
    HANDLE_ERROR(cudaMalloc((void **)&incrL2_d, numCells * sizeof(int)));
  }

  HANDLE_ERROR(cudaMemcpy(positions_d, particles.positions.data(), size, cudaMemcpyHostToDevice));
  // HANDLE_ERROR(cudaMemcpy(velocities_d, particles.velocities.data(), size, cudaMemcpyHostToDevice));
  // HANDLE(cudaMemcpy(predictedPositions_d, particles.predictedPositions.data(),
  // 		    size, cudaMemcpyHostToDevice));
  // HANDLE_ERROR(cudaMemcpy(gridCount_d, particles.gridCount.data(), spaceGridSize,
  // 			  cudaMemcpyHostToDevice));
  // HANDLE_ERROR(cudaMemcpy(neighbourData_d, particles.neighbourData.data(), activeSize,
  // 			  cudaMemcpyHostToDevice));
  // HANDLE_ERROR(cudaMemcpy(neighbourCount_d, particles.neighbourCount.data(), activeSize,
  // 			  cudaMemcpyHostToDevice));
}

CudaBuffers::~CudaBuffers() {
  HANDLE_ERROR(cudaFree(positions_d));
  HANDLE_ERROR(cudaFree(velocities_d));
  HANDLE_ERROR(cudaFree(predictedPositions_d));
  HANDLE_ERROR(cudaFree(gridCount_d));
  HANDLE_ERROR(cudaFree(gridStart_d));
  HANDLE_ERROR(cudaFree(gridData_d));
  HANDLE_ERROR(cudaFree(insertPos_d));
  HANDLE_ERROR(cudaFree(neighbourData_d));
  HANDLE_ERROR(cudaFree(neighbourCount_d));
  HANDLE_ERROR(cudaFree(positionsAtLastBuild_d));
  HANDLE_ERROR(cudaFree(buildNeighbours_d));
  HANDLE_ERROR(cudaFree(allLambdas_d));
  HANDLE_ERROR(cudaFree(deltas_d));
  HANDLE_ERROR(cudaFree(colliders_d));
  HANDLE_ERROR(cudaFree(vorticities_d));
  HANDLE_ERROR(cudaFree(triColliders_d));
  HANDLE_ERROR(cudaFree(closestPoints_d));

  if (blocksPerGridL2 == 1 || blocksPerGridL3 == 1) {
    HANDLE_ERROR(cudaFree(sumsL1_d));
    HANDLE_ERROR(cudaFree(incrL1_d));
  }
  if (blocksPerGridL3 == 1) {
    HANDLE_ERROR(cudaFree(sumsL2_d));
    HANDLE_ERROR(cudaFree(incrL2_d));
  }
}

void CudaBuffers::handleCellGridUpdate(int numCells1D) {
  if (insertPos_d != nullptr) {
    HANDLE_ERROR(cudaFree(insertPos_d));
  }
  HANDLE_ERROR(cudaMalloc((void **)&insertPos_d,
                          numCells1D * numCells1D * numCells1D * sizeof(int)));
}

