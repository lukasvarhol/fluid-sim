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
  
  cudaError_t err;
  err = cudaMalloc((void **)&positions_d, size);
  printError(err);

  err = cudaMalloc((void **)&velocities_d, size);
  printError(err);

  err = cudaMalloc((void **)&predictedPositions_d, size);
  printError(err);

  err = cudaMalloc((void **)&gridCount_d, spaceGridSize);
  printError(err); // TODO: refactor out into free function "CUDA_ERROR"

  err = cudaMalloc((void **)&gridStart_d, (numCells + 1) * sizeof(int));
 printError(err); // TODO: refactor out into free function "CUDA_ERROR"

 err = cudaMalloc((void **)&gridData_d, activeSize);
 printError(err);

 err = cudaMalloc((void **)&neighbourData_d, activeSize * MAX_NEIGHBOURS);
 printError(err);

  err = cudaMalloc((void **)&neighbourCount_d, activeSize);
  printError(err);

  err = cudaMalloc((void **)&positionsAtLastBuild_d,
                   particles.activeParticles * sizeof(Vec3));
  printError(err);

  err = cudaMalloc((void **)&buildNeighbours_d, sizeof(int));
  printError(err);

  err = cudaMalloc((void **)&allLambdas_d,
                   sizeof(float) * particles.activeParticles);
  printError(err);

 handleCellGridUpdate(particles.numCells1D);

 if (blocksPerGridL2 == 1) {
    err = cudaMalloc((void **)&sumsL1_d, blocksPerGridL1 * sizeof(int));
    printError(err);

    err = cudaMalloc((void **)&incrL1_d, numCells * sizeof(int));
    printError(err);

 } else if (blocksPerGridL3 == 1) {
    err = cudaMalloc((void **)&sumsL1_d, blocksPerGridL1 * sizeof(int));
    printError(err);

    err = cudaMalloc((void **)&incrL1_d, numCells * sizeof(int));
    printError(err);

    err = cudaMalloc((void **)&sumsL2_d, blocksPerGridL2 * sizeof(int));
    printError(err);

    err = cudaMalloc((void **)&incrL2_d, numCells * sizeof(int));
    printError(err);
 }

  cudaMemcpy(positions_d, particles.positions.data(), size, cudaMemcpyHostToDevice);
  cudaMemcpy(velocities_d, particles.velocities.data(), size, cudaMemcpyHostToDevice);
  cudaMemcpy(predictedPositions_d, particles.predictedPositions.data(), size,
             cudaMemcpyHostToDevice);
  cudaMemcpy(gridCount_d, particles.gridCount.data(), spaceGridSize,
             cudaMemcpyHostToDevice);
  cudaMemcpy(neighbourData_d, particles.neighbourData.data(), activeSize,
             cudaMemcpyHostToDevice);
  cudaMemcpy(neighbourCount_d, particles.neighbourCount.data(), activeSize,
             cudaMemcpyHostToDevice);
  
}

CudaBuffers::~CudaBuffers() {
  cudaError_t err;
  err = cudaFree(positions_d);
  printError(err);
  err = cudaFree(velocities_d);
  printError(err);
  err = cudaFree(predictedPositions_d);
  printError(err);
  err = cudaFree(gridCount_d);
  printError(err);
  err = cudaFree(gridStart_d);
  printError(err);
  err = cudaFree(gridData_d);
  printError(err);
  err = cudaFree(insertPos_d);
  printError(err);
  err = cudaFree(neighbourData_d);
  printError(err);
  err = cudaFree(neighbourCount_d);
  printError(err);
  err = cudaFree(positionsAtLastBuild_d);
  printError(err);
  err = cudaFree(buildNeighbours_d);
  printError(err);
  err = cudaFree(allLambdas_d);
  printError(err);

  if (blocksPerGridL2 == 1 || blocksPerGridL3 == 1) {
    err = cudaFree(sumsL1_d);
    printError(err);
    err = cudaFree(incrL1_d);
    printError(err);
  }
  if (blocksPerGridL3 == 1) {
    err = cudaFree(sumsL2_d);
    printError(err);
    err = cudaFree(incrL2_d);
    printError(err);
  }
}


void CudaBuffers::printError(const cudaError_t err) const {
  if (err != cudaSuccess) {
    printf("%s in %s at line %d \n", cudaGetErrorString(err), __FILE__,
	   __LINE__);
    exit(EXIT_FAILURE);
  }
}

void CudaBuffers::handleCellGridUpdate(int numCells1D) {
  cudaError_t err;
  if (insertPos_d != nullptr) {
    err = cudaFree(insertPos_d);
    CudaBuffers::printError(err);
  }

  err = cudaMalloc((void **)&insertPos_d,
                   numCells1D * numCells1D * numCells1D * sizeof(int));
  CudaBuffers::printError(err);
}
