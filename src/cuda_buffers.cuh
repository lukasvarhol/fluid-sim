#pragma once
#include "particles.h"



class CudaBuffers {
public:
  CudaBuffers(std::vector<Vec3> &positions,
                      std::vector<Vec3> &velocities,
                      std::vector<Vec3> &predictedPositions, int numParticles) {
    size_t size = numParticles * sizeof(Vec3);
    cudaError_t err;
    err = cudaMalloc((void **)&positions_d, size);
    printError(err);

    err = cudaMalloc((void **)&velocities_d, size);
    printError(err);

    err = cudaMalloc((void **)&predictedPositions_d, size);
    printError(err);

    cudaMemcpy(positions_d, positions.data(), size, cudaMemcpyHostToDevice);
    cudaMemcpy(velocities_d, velocities.data(), size, cudaMemcpyHostToDevice);
    cudaMemcpy(predictedPositions_d, predictedPositions.data(), size, cudaMemcpyHostToDevice);
  }

  CudaBuffers(const CudaBuffers &cb) = delete;

  CudaBuffers &operator=(const CudaBuffers &cb) = delete;

  ~CudaBuffers() {
    cudaError_t err;
    err = cudaFree(positions_d);
    printError(err);

    err = cudaFree(velocities_d);
    printError(err);
    err = cudaFree(predictedPositions_d);
    printError(err);
  }

  Vec3* positions_d;
  Vec3* velocities_d;
  Vec3 *predictedPositions_d;

  void printError(const cudaError_t err) const {
    if (err != cudaSuccess) {
      printf("%s in %s at line %d \n", cudaGetErrorString(err), __FILE__,
             __LINE__);
      exit(EXIT_FAILURE);
    }
  }
};
