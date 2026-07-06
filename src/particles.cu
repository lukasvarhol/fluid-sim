#include "particles.cuh"

#define NUM_BANKS 16
#define LOG_NUM_BANKS 4
#define CONFLICT_FREE_OFFSET(n)                                                \
  (((n) >> LOG_NUM_BANKS) + ((n) >> (LOG_NUM_BANKS << 1)))


/*******************************************************************************
 * @brief Gravity and mouse interaction kernel.
 *
 * Each particle is assigned a thread and then gravity and mouse force is
 * applied. Vector addition is not parallelised because with only three elements
 * x, y, z, there is no benefit.
 *
 * @param positions pointer to device particles positions array.
 * @param velocities pointer to device particles velocities array.
 * @param predictedPositions pointer to device particles predictedPositions
 *        array.
 * @param gravity in arbitrary units.
 * @param mouseStrength magnitude of mouse force.
 * @param mouseRadius defines mouse interaction affected area.
 * @param rayOrigin epicentre of mouse interaction force.
 * @param rayDir points to mouse cursor projection.
 * @param dt frame time delta, in seconds.
 * @param n number of particles.
 ******************************************************************************/
__global__
void gravityPredictKernel(Vec3 *positions, Vec3 *velocities,
			  Vec3 *predictedPositions,
			  float gravity,
			  float mouseStrength, float mouseRadius,
			  Vec3 rayOrigin, Vec3 rayDir, float dt,
			  int n) {

  int i = threadIdx.x + blockDim.x * blockIdx.x;
  if (i < n) {
    velocities[i] += Vec3{0.0f, gravity, 0.0f} * dt;

    if (mouseStrength != 0.0f && mouseRadius != 0.0f) {
      Vec3  toParticle = positions[i] - rayOrigin;
      // project onto ray, then get perpendicular distance
      float along      = toParticle.Dot(rayDir);
      Vec3  closest    = rayOrigin + rayDir * along;
      Vec3  perp       = positions[i] - closest;
      float d2         = perp.Dot(perp);

      if (d2 < mouseRadius*mouseRadius && d2 > 1e-6f) {
	float d       = std::sqrt(d2);
	float falloff = 1.0f - (d / mouseRadius);
	Vec3  dir     = perp * (1.0f / d);   // push/pull away from ray axis
	velocities[i] += dir * (mouseStrength * falloff * dt);
      }
    }
    predictedPositions[i] = positions[i] + velocities[i] * dt;
  }
}

void gpuGravityPredict(CudaBuffers& cb, std::vector<Vec3> &positions_h,
		       std::vector<Vec3> &velocities_h,
		       std::vector<Vec3> &predictedPositions_h, float gravity,
		       float mouseStrength, float mouseRadius, Vec3 rayOrigin,
		       Vec3 rayDir, float dt, int n) {

  size_t size = n * sizeof(Vec3);

  cudaEvent_t start, stop;
  cudaEventCreate(&start);
  cudaEventCreate(&stop);

  cudaEventRecord(start);
  
  cudaMemcpy(cb.positions_d, positions_h.data(), size, cudaMemcpyHostToDevice);
  cudaMemcpy(cb.velocities_d, velocities_h.data(), size, cudaMemcpyHostToDevice);

  gravityPredictKernel<<<ceil(n / 384.0), 384>>>(
      cb.positions_d, cb.velocities_d, cb.predictedPositions_d, gravity,
      mouseStrength, mouseRadius, rayOrigin, rayDir, dt, n);


  cudaMemcpy(predictedPositions_h.data(), cb.predictedPositions_d, size,
             cudaMemcpyDeviceToHost);
  
  cudaEventRecord(stop);

  cudaEventSynchronize(stop);
  
  cudaEventDestroy(start);
  cudaEventDestroy(stop);
}

/*******************************************************************************
 * @brief Work-efficient (Blelloch) scan
 *
 * Author: TVycas
 * Availability: github.com/TVycas/CUDA-Parallel-Prefix-Sum/
 * 
 * Multi-layered tree-scan exclusive prefix-scan implementation to find the
 * region in which to write particles to in the cell grid.
 *
 * @note BLOCK_SIZE is currently a preprocessor directive.
 *
 * @param output pointer to output array.
 * @param input pointer to input array.
 * @param n number of elements.
 * @param SUM sum from previous layer.
 ******************************************************************************/
__global__ void scanKernel(int *output, int *input, int n, int* SUM) {
  __shared__ int temp[(BLOCK_SIZE << 1) + (BLOCK_SIZE)];
  int threadId = threadIdx.x;
  int offset = 1;
  int blockOffset = BLOCK_SIZE * (blockIdx.x << 1);

  int ai = threadId;
  int bi = threadId + BLOCK_SIZE;

  int bankOffsetA = CONFLICT_FREE_OFFSET(ai);
  int bankOffsetB = CONFLICT_FREE_OFFSET(bi);

  if (blockOffset + ai < n) {
    temp[ai + bankOffsetA] = input[blockOffset + ai];
  }
  if (blockOffset + bi < n) {
    temp[bi + bankOffsetB] = input[blockOffset + bi];
  }

  // scan up
  for (int d = BLOCK_SIZE; d > 0; d >>= 1) {
    __syncthreads();
    if (threadId < d) {
      int ai = offset * ((threadId << 1) + 1) - 1;
      int bi = offset * ((threadId << 1) + 2) - 1;
      ai += CONFLICT_FREE_OFFSET(ai);
      bi += CONFLICT_FREE_OFFSET(bi);

      temp[bi] += temp[ai];
    }
    offset <<= 1;
  }

  if (threadId == 0) {
    if (SUM != NULL) {
      SUM[blockIdx.x] = temp[(BLOCK_SIZE << 1) - 1 +
                             CONFLICT_FREE_OFFSET((BLOCK_SIZE << 1) - 1)];
    }
    temp[(BLOCK_SIZE << 1) - 1 + CONFLICT_FREE_OFFSET((BLOCK_SIZE << 1) - 1)] = 0;
  }

  // scan down
  for (int d = 1; d < (BLOCK_SIZE << 1); d <<= 1) {
    offset >>= 1;
    __syncthreads();
    if (threadId < d) {
      int ai = offset * ((threadId << 1) + 1) - 1;
      int bi = offset * ((threadId << 1) + 2) - 1;
      ai += CONFLICT_FREE_OFFSET(ai);
      bi += CONFLICT_FREE_OFFSET(bi);

      int t = temp[ai];
      temp[ai] = temp[bi];
      temp[bi] += t;
    }
  }
  __syncthreads();

  if (blockOffset + ai < n) {
    output[blockOffset + ai] = temp[ai + bankOffsetA];
  }

  if (blockOffset + bi < n) {
    output[blockOffset + bi] = temp[bi + bankOffsetB];
  }
}

/*******************************************************************************
 * @brief Places particles in their cells
 *
 * @param predictedPositions pointer to predicted positions array
 * @param gridCount pointer to array which stores the number of particles per
 * cell.
 * @param smoothingRadius area of influence parameter.
 * @param numCells1D number of cells in one dimension (assumes cube grid)
 * @param activeParticles number of particles to handle.
 ******************************************************************************/
__global__
void particleToCellKernel(Vec3 *predictedPositions, int* gridCount, float smoothingRadius,
		     int numCells1D, int activeParticles) {
  
  int i = threadIdx.x + blockDim.x * blockIdx.x;

  if (i < activeParticles) {
    Cell c =
      PositionToCoord(predictedPositions[i], smoothingRadius, numCells1D);
    atomicAdd(&gridCount[CellIndex(c.x, c.y, c.z, numCells1D)], 1);
  }
}

/*******************************************************************************
 * @brief Takes the output array and for each block i, adds value i from INCR
 * array to every element.
 *
 * Author: TVycas
 * Availability: github.com/TVycas/CUDA-Parallel-Prefix-Sum/
 *
 * @param output pointer to output array.
 * @param n number of blocks per grid.
 * @param INCR
 ******************************************************************************/
__global__ void uniformAddKernel(int *output, int n, int *INCR) {
  int idx = threadIdx.x + (BLOCK_SIZE << 1) * blockIdx.x;

  int valueToAdd = INCR[blockIdx.x];

  if (idx < n) {
    output[idx] += valueToAdd;
  }
  if (idx + BLOCK_SIZE < n) {
    output[idx + BLOCK_SIZE] += valueToAdd;
  }
}

/*******************************************************************************
 * @brief Writes particle index to correct position in gridData array
 *
 * @param gridData pointer to array that stores particle indexes in flattened
 * grid data structure.
 * @param insertPos pointer to array that holds indices for the start of each
 * cell in the flattened array gridData.
 * @param predictedPositions pointer to array of predicted particle positions.
 * @param smoothingRadius area of influence parameeter.
 * @param numCells1D number of cells in one dimension (assumes cube grid)
 * @param activeParticles number of particles to handle.
 ******************************************************************************/
__global__ void fillGridKernel(int *gridData, int* insertPos, Vec3 *predictedPositions,
                               float smoothingRadius, int numCells1D, int activeParticles) {

  int i = threadIdx.x + (blockIdx.x * blockDim.x);

  if (i < activeParticles) {
    Cell c =
        PositionToCoord(predictedPositions[i], smoothingRadius, numCells1D);
    int idx = CellIndex(c.x, c.y, c.z, numCells1D);
    int beforeIncrement = atomicAdd(&insertPos[idx], 1);
    gridData[beforeIncrement] = i;
  }
}

void gpuBuildGrid(CudaBuffers &cb, int *gridStart_h, int *gridCount_h,
                  int *gridData_h, float smoothingRadius, int numCells1D,
                  int activeParticles) {

  cudaEvent_t start, stop;
  cudaEventCreate(&start);
  cudaEventCreate(&stop);

  cudaEventRecord(start);
  
  int numCells = numCells1D * numCells1D * numCells1D;
  size_t size = numCells * sizeof(int);

  cudaMemset(cb.gridCount_d, 0, numCells * sizeof(int));

  /* ------ Phase 1: assign particle to cell ------ */
  particleToCellKernel<<<ceil(activeParticles / 384.0), 384>>>(
      cb.predictedPositions_d, cb.gridCount_d, smoothingRadius, numCells1D,
      activeParticles);

  /* ------ Phase 2: Parallel Prefix Sum ------ */

  if (cb.blocksPerGridL1 == 1) {
    scanKernel<<<cb.blocksPerGridL1, BLOCK_SIZE>>>(cb.gridStart_d, cb.gridCount_d,
                                                numCells, NULL);
    cudaDeviceSynchronize();
  } else if (cb.blocksPerGridL2 == 1) {

    scanKernel<<<cb.blocksPerGridL1, BLOCK_SIZE>>>(cb.gridStart_d, cb.gridCount_d,
                                                numCells, cb.sumsL1_d);

    scanKernel<<<cb.blocksPerGridL2, BLOCK_SIZE>>>(cb.incrL1_d, cb.sumsL1_d,
                                                cb.blocksPerGridL1, NULL);

    uniformAddKernel <<<cb.blocksPerGridL1, BLOCK_SIZE>>>(cb.gridStart_d, numCells, cb.incrL1_d);

    cudaDeviceSynchronize();
  } else if (cb.blocksPerGridL3 == 1) {


    scanKernel<<<cb.blocksPerGridL1, BLOCK_SIZE>>>(cb.gridStart_d, cb.gridCount_d,
                                                numCells, cb.sumsL1_d);

    scanKernel<<<cb.blocksPerGridL2, BLOCK_SIZE>>>(cb.incrL1_d, cb.sumsL1_d,
                                                cb.blocksPerGridL1, cb.sumsL2_d);

    scanKernel<<<cb.blocksPerGridL3, BLOCK_SIZE>>>(cb.incrL2_d, cb.sumsL2_d,
                                                cb.blocksPerGridL2, NULL);

    uniformAddKernel<<<cb.blocksPerGridL2, BLOCK_SIZE>>>(cb.incrL1_d, cb.blocksPerGridL1,
                                                cb.incrL2_d);
    uniformAddKernel<<<cb.blocksPerGridL1, BLOCK_SIZE>>>(cb.gridStart_d, numCells,
                                                cb.incrL1_d);

    cudaDeviceSynchronize();
  } else {
    printf("The array of %d elements is too large for only 3 Layers", numCells);
    exit(EXIT_FAILURE);
  }

  /* ------ Phase 3: Parallel Scatter ------ */
  cudaMemcpy(cb.insertPos_d, cb.gridStart_d, size,
             cudaMemcpyDeviceToDevice);

  fillGridKernel<<<ceil(activeParticles / 384.0), 384>>>(
      cb.gridData_d, cb.insertPos_d, cb.predictedPositions_d, smoothingRadius,
      numCells1D, activeParticles);

  /* ------ Cleanup ------ */

  cudaMemcpy(gridStart_h, cb.gridStart_d, (numCells + 1) * sizeof(int),
             cudaMemcpyDeviceToHost);
  gridStart_h[numCells] = activeParticles;
  cudaMemcpy(gridData_h, cb.gridData_d, activeParticles * sizeof(int), cudaMemcpyDeviceToHost);
  cudaMemcpy(gridCount_h, cb.gridCount_d, size, cudaMemcpyDeviceToHost);

  cudaEventRecord(stop);

  cudaEventSynchronize(stop);
  float miliseconds = 0.0f;
  cudaEventElapsedTime(&miliseconds, start, stop);
  printf("Execution time: %f miliseconds \n", miliseconds);
  
  cudaEventDestroy(start);
  cudaEventDestroy(stop);
}
