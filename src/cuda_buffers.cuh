#pragma once
#include "linear_algebra.h"
#include "objects3d/editor_state.h"
#include <cuda_runtime.h>
#include <driver_types.h>

#define BLOCK_SIZE 256

#define HANDLE_ERROR(call){check((call), #call, __FILE__, __LINE__); }
inline void check(cudaError_t code, const char* function, const char *file, int line,
                  bool abort = false) {
  if (code != cudaSuccess) {
    printf("CUDA runtime error at: %s : %d\n", file, line);
    printf("%s : %s\n", cudaGetErrorString(code), function);
    if (abort)
      exit(code);
  }
}

#define CUDA_CHECK_LAST() checkLast(__FILE__, __LINE__)
inline void checkLast(const char *file, int line) {
  cudaError_t const err{cudaGetLastError()};
  if (err != cudaSuccess) {
    printf("CUDA runtime error at: %s : %d\n", file, line);
    printf("%s\n", cudaGetErrorString(err));
  }
}

struct Particles;

class CudaBuffers {
public:
  CudaBuffers(Particles &particles);
  CudaBuffers(const CudaBuffers &cb) = delete;
  CudaBuffers &operator=(const CudaBuffers &cb) = delete;
  ~CudaBuffers();
  void handleCellGridUpdate(int numCells1D);

  Vec3 *positions_d;
  Vec3 *velocities_d;
  Vec3 *predictedPositions_d;
  int *gridCount_d;
  int *gridStart_d;
  int *gridData_d;
  int *insertPos_d = NULL;
  int *neighbourCount_d;
  int *neighbourData_d;
  Vec3 *positionsAtLastBuild_d;
  float *allLambdas_d;
  Vec3 *deltas_d;
  Vec3* vorticities_d;
  SDFCollider *colliders_d;
  TriCollider* triColliders_d;
  Vec3 *closestPoints_d;

  int *buildNeighbours_d;

  int *sumsL1_d = NULL;
  int *sumsL2_d = NULL;
  int *incrL1_d = NULL;
  int *incrL2_d = NULL;

  int blocksPerGridL1;
  int blocksPerGridL2;
  int blocksPerGridL3;
};

