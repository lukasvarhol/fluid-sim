#pragma once
#include "linear_algebra.h"


#define BLOCK_SIZE 256

struct Particles;

class CudaBuffers {
public:
  CudaBuffers(Particles &particles);
  CudaBuffers(const CudaBuffers &cb) = delete;
  CudaBuffers &operator=(const CudaBuffers &cb) = delete;
  ~CudaBuffers();
  void printError(const cudaError_t err) const;
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

  int *sumsL1_d = NULL;
  int *sumsL2_d = NULL;
  int *incrL1_d = NULL;
  int *incrL2_d = NULL;

  int blocksPerGridL1;
  int blocksPerGridL2;
  int blocksPerGridL3;

};

