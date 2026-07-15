#include "cuda_buffers.cuh"
#include "linear_algebra.h"
#include "particle_config.h"
#include "particles.cuh"
#include <cstdlib>
#include <driver_types.h>

#define NUM_BANKS 16
#define LOG_NUM_BANKS 4
#define CONFLICT_FREE_OFFSET(n)					\
  (((n) >> LOG_NUM_BANKS) + ((n) >> (LOG_NUM_BANKS << 1)))

__constant__ float poly6_coeff_d;
__constant__ float spiky_coeff_d;

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
void gravityPredictKernel(Vec3 * __restrict__ positions, Vec3 * __restrict__ velocities,
			  Vec3 * __restrict__ predictedPositions,
			  float gravity,
			  float mouseStrength, float mouseRadius,
			  Vec3 rayOrigin, Vec3 rayDir, float dt,
			  int n) {

  int i = threadIdx.x + blockDim.x * blockIdx.x;
  if (i < n) {
    Vec3 v = velocities[i];
    Vec3 p = positions[i];

    v +=  Vec3{0.0f, gravity, 0.0f} * dt;
    
    if (mouseStrength != 0.0f && mouseRadius != 0.0f) {
      Vec3  toParticle = p - rayOrigin;
      // project onto ray, then get perpendicular distance
      float along      = toParticle.Dot(rayDir);
      Vec3  closest    = rayOrigin + rayDir * along;
      Vec3  perp       = p - closest;
      float d2         = perp.Dot(perp);

      if (d2 < mouseRadius*mouseRadius && d2 > 1e-6f) {
	float d       = sqrtf(d2);
	float falloff = 1.0f - (d / mouseRadius);
	Vec3  dir     = perp * (1.0f / d);   // push/pull away from ray axis
	v += dir * (mouseStrength * falloff * dt);
      }
    }
    predictedPositions[i] = p + v * dt;
  }
}

void gpuGravityPredict(CudaBuffers& cb, Vec3* positions_h, Vec3* velocities_h, Vec3* predictedPositions_h, float gravity,
		       float mouseStrength, float mouseRadius, Vec3 rayOrigin,
		       Vec3 rayDir, float dt, int n) {

  size_t size = n * sizeof(Vec3);

  cudaEvent_t start, stop;
  cudaEventCreate(&start);
  cudaEventCreate(&stop);

  cudaEventRecord(start);
  
  cudaMemcpy(cb.positions_d, positions_h, size, cudaMemcpyHostToDevice);
  cudaMemcpy(cb.velocities_d, velocities_h, size, cudaMemcpyHostToDevice);

  gravityPredictKernel<<<ceil(n / 128.0), 128>>>(
						 cb.positions_d, cb.velocities_d, cb.predictedPositions_d, gravity,
						 mouseStrength, mouseRadius, rayOrigin, rayDir, dt, n);

  // cudaMemcpy(predictedPositions_h, cb.predictedPositions_d, size,
  //            cudaMemcpyDeviceToHost);
  
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
__global__ void fillGridKernel(int * __restrict__ gridData, int* __restrict__ insertPos, Vec3 * __restrict__ predictedPositions,
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
  
  cudaError_t err = cudaGetLastError();
  if (err != cudaSuccess) {
    printf("someKernel launch failed: %s\n", cudaGetErrorString(err));
  }

  err = cudaDeviceSynchronize();
  if (err != cudaSuccess) {
    printf("someKernel execution failed: %s\n", cudaGetErrorString(err));
  }

  /* ------ Phase 2: Parallel Prefix Sum ------ */

  if (cb.blocksPerGridL1 == 1) {
    scanKernel<<<cb.blocksPerGridL1, BLOCK_SIZE>>>(cb.gridStart_d, cb.gridCount_d,
						   numCells, NULL);
    cudaDeviceSynchronize();
  } else if (cb.blocksPerGridL2 == 1) {

    scanKernel<<<cb.blocksPerGridL1, BLOCK_SIZE>>>(
        cb.gridStart_d, cb.gridCount_d, numCells, cb.sumsL1_d);

    
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
      printf("someKernel launch failed: %s\n", cudaGetErrorString(err));
    }

    err = cudaDeviceSynchronize();
    if (err != cudaSuccess) {
      printf("someKernel execution failed: %s\n", cudaGetErrorString(err));
    }

    scanKernel<<<cb.blocksPerGridL2, BLOCK_SIZE>>>(cb.incrL1_d, cb.sumsL1_d,
                                                   cb.blocksPerGridL1, NULL);

    
    err = cudaGetLastError();
    if (err != cudaSuccess) {
      printf("someKernel launch failed: %s\n", cudaGetErrorString(err));
    }

    err = cudaDeviceSynchronize();
    if (err != cudaSuccess) {
      printf("someKernel execution failed: %s\n", cudaGetErrorString(err));
    }

    uniformAddKernel <<<cb.blocksPerGridL1, BLOCK_SIZE>>>(cb.gridStart_d, numCells, cb.incrL1_d);

    cudaDeviceSynchronize();
  } else if (cb.blocksPerGridL3 == 1) {


    scanKernel<<<cb.blocksPerGridL1, BLOCK_SIZE>>>(
        cb.gridStart_d, cb.gridCount_d, numCells, cb.sumsL1_d);

    
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
      printf("someKernel launch failed: %s\n", cudaGetErrorString(err));
    }

    err = cudaDeviceSynchronize();
    if (err != cudaSuccess) {
      printf("someKernel execution failed: %s\n", cudaGetErrorString(err));
    }

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
  
   err = cudaGetLastError();
  if (err != cudaSuccess) {
    printf("someKernel launch failed: %s\n", cudaGetErrorString(err));
  }

  err = cudaDeviceSynchronize();
  if (err != cudaSuccess) {
    printf("someKernel execution failed: %s\n", cudaGetErrorString(err));
  }

  /* ------ Cleanup ------ */

  cudaMemcpy(gridStart_h, cb.gridStart_d, (numCells + 1) * sizeof(int),
             cudaMemcpyDeviceToHost);
  gridStart_h[numCells] = activeParticles;
  cudaMemcpy(gridData_h, cb.gridData_d, activeParticles * sizeof(int), cudaMemcpyDeviceToHost);
  cudaMemcpy(gridCount_h, cb.gridCount_d, size, cudaMemcpyDeviceToHost);

  cudaEventRecord(stop);

  cudaEventSynchronize(stop);
  // float miliseconds = 0.0f;
  // cudaEventElapsedTime(&miliseconds, start, stop);
  // printf("Execution time: %f miliseconds \n", miliseconds);
  
  cudaEventDestroy(start);
  cudaEventDestroy(stop);
}

__global__ void buildNeighboursKernel(int *neighbourData, int *neighbourCount,
                                      Vec3 *predictedPositions, int *gridStart,
                                      int *gridData, float smoothingRadius, int numCells1D,
                                      int activeParticles) {
  int i = threadIdx.x + blockIdx.x * blockDim.x;

  if (i < activeParticles) {
    int count = 0;
    int *currentNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
    Cell cell =
      PositionToCoord(predictedPositions[i], smoothingRadius, numCells1D);

    for (int ox = -1; ox <= 1; ++ox)
      for (int oy = -1; oy <= 1; ++oy)
        for (int oz = -1; oz <= 1; ++oz) {
          int cx = cell.x + ox;
          int cy = cell.y + oy;
          int cz = cell.z + oz;
	  if (cx < 0 || cy < 0 || cz < 0 || cx >= numCells1D || cy >= numCells1D ||
              cz >= numCells1D)
            continue;

          int idx = CellIndex(cx, cy, cz, numCells1D);
          for (int k = gridStart[idx]; k < gridStart[idx + 1]; ++k) {
            int j = gridData[k];
            Vec3 diff = predictedPositions[j] - predictedPositions[i];
            float d2 = diff.Dot(diff);
            if (d2 <= smoothingRadius * smoothingRadius && count < MAX_NEIGHBOURS)
              currentNeighbours[count++] = j;
          }
        }
    neighbourCount[i] = count;
  }
}

__global__ void needsNeighbourRebuildKernel(Vec3 *predictedPositions,
                                            Vec3 *positionsAtLastBuild,
                                            int *buildNeighbours,
                                            float skinRadius2,
                                            int activeParticles) {
  
  int i = threadIdx.x + blockDim.x * blockIdx.x;

  if (i < activeParticles) {
    Vec3 diff = predictedPositions[i] - positionsAtLastBuild[i];
    if (diff.Dot(diff) > skinRadius2)
      atomicOr(buildNeighbours, 1);
    return;
  }
}


void gpuBuildNeighbours(CudaBuffers &cb, int *neighbourData_h,
                        int *neighbourCount_h,
                        std::vector<Vec3> &positionsAtLastBuild_h,
                        float smoothingRadius, float skinRadius2,
                        int numCells1D, int activeParticles,
                        bool forceRebuild) {
  int one = 1;
  if (forceRebuild) {
    cudaMemcpy(cb.buildNeighbours_d, &one, sizeof(int), cudaMemcpyHostToDevice);
  } else {
    cudaMemset(cb.buildNeighbours_d, 0, sizeof(int));
    if ((int)positionsAtLastBuild_h.size() < activeParticles)
      cudaMemcpy(cb.buildNeighbours_d, &one, sizeof(int), cudaMemcpyHostToDevice);
    else {
      cudaMemcpy(cb.positionsAtLastBuild_d, positionsAtLastBuild_h.data(),
		 sizeof(Vec3) * activeParticles, cudaMemcpyHostToDevice);

      needsNeighbourRebuildKernel<<<ceil(activeParticles / 384.0), 384>>>(
          cb.predictedPositions_d, cb.positionsAtLastBuild_d,
          cb.buildNeighbours_d, skinRadius2, activeParticles);
    }
  }

  int buildNeighbours_h;
  cudaMemcpy(&buildNeighbours_h, cb.buildNeighbours_d, sizeof(int), cudaMemcpyDeviceToHost);
  if(buildNeighbours_h == 1) {
    buildNeighboursKernel<<<ceil(activeParticles / 384.0), 384>>>(
								  cb.neighbourData_d, cb.neighbourCount_d, cb.predictedPositions_d,
								  cb.gridStart_d, cb.gridData_d, smoothingRadius, numCells1D,
								  activeParticles);
  
    cudaMemcpy(neighbourData_h, cb.neighbourData_d, activeParticles * MAX_NEIGHBOURS * sizeof(int),
	       cudaMemcpyDeviceToHost);
    cudaMemcpy(neighbourCount_h, cb.neighbourCount_d,
               activeParticles * sizeof(int), cudaMemcpyDeviceToHost);
  }

}

__global__ void calculateLambdasKernel(float *allLambdas,
				       Vec3 *predictedPositions,
				       int *neighbourData,
				       int* neighbourCount,
				       float relaxation,
				       float restDensity,
				       float smoothingRadius,
				       int activeParticles) {
  int i = threadIdx.x + blockIdx.x * blockDim.x;

  if (i < activeParticles) {
    float h2 = smoothingRadius * smoothingRadius;

    float density = poly6_coeff_d * powf(h2,3.0f);
    Vec3 gradI = Vec3{0.0f, 0.0f, 0.0f};
    float denominator = 0.0f;

    const Vec3 &pI = predictedPositions[i];

    int* currentNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
    for (int e{}; e < neighbourCount[i]; ++e) {
      int n = currentNeighbours[e];
      if (n == int(i))
        continue;

      Vec3 diff = pI - predictedPositions[n];
      float d2 = diff.Dot(diff);
      if (d2 >= h2 || d2 < 1e-12f)
        continue;
      float sq = h2 - d2;
      density += poly6_coeff_d * powf(sq,3.0f);

      float d = sqrtf(d2);
      float s = spiky_coeff_d * (smoothingRadius - d) * (smoothingRadius - d);
      Vec3 grad = diff * (s / (d * restDensity));
      gradI += grad;
      denominator += grad.Dot(grad);
    }

    denominator += gradI.Dot(gradI);
    float Ci = (density / restDensity) - 1.0f;
    allLambdas[i] = -Ci / (denominator + relaxation);
  }
}

void gpuCalculateLambda(CudaBuffers &cb, float relaxation,
                        float restDensity, float smoothingRadius,
                        int activeParticles) {

  
  cudaEvent_t start, stop;
  cudaEventCreate(&start);
  cudaEventCreate(&stop);

  cudaEventRecord(start);

  float poly6_coeff_h = 315.0f / (64.0f * PI * powf(smoothingRadius, 9));
  cudaMemcpyToSymbol(poly6_coeff_d, &poly6_coeff_h, sizeof(float));

  float spiky_coeff_h = -45.0f / (PI * powf(smoothingRadius, 6));
  cudaMemcpyToSymbol(spiky_coeff_d, &spiky_coeff_h, sizeof(float));
  
  calculateLambdasKernel<<<ceil(activeParticles / 128.0), 128>>>(
								 cb.allLambdas_d, cb.predictedPositions_d, cb.neighbourData_d,
								 cb.neighbourCount_d, relaxation, restDensity, smoothingRadius,
								 activeParticles);

  // cudaMemcpy(allLambdas_h, cb.allLambdas_d, sizeof(float) * activeParticles,
  //            cudaMemcpyDeviceToHost);

  cudaEventRecord(stop);

  cudaEventSynchronize(stop);
  // float miliseconds = 0.0f;
  // cudaEventElapsedTime(&miliseconds, start, stop);
  // printf("Execution time: %f milliseconds \n", miliseconds);
  
  cudaEventDestroy(start);
  cudaEventDestroy(stop);
}

__global__ void calculateDeltasKernel(Vec3 *deltas, Vec3 *predictedPositions,
                                      int *neighbourData, int *neighbourCount, float* allLambdas,
                                      float restDensity,
                                      float wdq, float scorr, float smoothingRadius,
                                      int activeParticles) {
  
  int i = threadIdx.x + blockIdx.x * blockDim.x;

  if (i < activeParticles) {
    float h2 = smoothingRadius * smoothingRadius;
    
    Vec3 sum = {0.0f, 0.0f, 0.0f};
    Vec3& pI = predictedPositions[i];

    int* currentNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
    for (int n{}; n < neighbourCount[i]; ++n) {
      int e = currentNeighbours[n];
      if (e == i)
        continue;

      Vec3 diff = pI - predictedPositions[e];
      float d2 = diff.Dot(diff);
      if (d2 < 1e-12f || d2 >= h2)
        continue;

      float d = sqrtf(d2);
      float s =
	spiky_coeff_d * (smoothingRadius - d) * (smoothingRadius - d) / d;
      float corr = Scorr(pI, predictedPositions[e], h2, poly6_coeff_d, wdq, scorr);
      float lambdaSum = allLambdas[i] + allLambdas[e] + corr;
      sum += diff * (s * lambdaSum);
    }
    deltas[i] = sum / restDensity;
    pI += deltas[i];
  }
}

void gpuCalculateDeltas(CudaBuffers &cb, float restDensity,
                        float wdq, float scorr, float smoothingRadius, int activeParticles) {
  calculateDeltasKernel<<<ceil(activeParticles / 128.0f), 128>>>(
      cb.deltas_d, cb.predictedPositions_d, cb.neighbourData_d,
      cb.neighbourCount_d, cb.allLambdas_d, restDensity, wdq, scorr,
      smoothingRadius, activeParticles);
  

  // cudaMemcpy(deltas_h, cb.deltas_d, sizeof(Vec3) * activeParticles,
  //            cudaMemcpyDeviceToHost);
}

__global__ void clampToBoundariesKernel(Vec3* predictedPositions, float radiusPx, int g_fb_w, int g_fb_h, int activeParticles) {
  int i = threadIdx.x + blockDim.x * blockIdx.x;

  if (i < activeParticles) {
    Vec3* p = &predictedPositions[i];
    float halfX = radiusPx / float(g_fb_w);
    float halfY = radiusPx / float(g_fb_h);

    p->x = fmax(-1.0f + halfX, fmin(1.0f - halfX, p->x));
    p->y = fmax(-1.0f + halfY, fmin(1.0f - halfY, p->y));

    const float halfZ = halfX;
    p->z = fmax(-1.0f + halfZ, fmin(1.0f - halfZ, p->z));
  }
}

void gpuClampToBoundaries(CudaBuffers& cb, Vec3* predictedPositions_h, float radiusPx, int g_fb_w, int g_fb_h, int activeParticles) {
  clampToBoundariesKernel<<<ceil(activeParticles / 128.0), 128>>>(
								  cb.predictedPositions_d, radiusPx, g_fb_w, g_fb_h, activeParticles);
}


__global__ void projectParticleSDFKernel(Vec3 *positions, Vec3 *velocities,
                                         SDFCollider *colliders, int activeParticles) {
  int i = threadIdx.x + blockDim.x * blockIdx.x;

  if (i < activeParticles) {
    for (size_t j{}; j < MAX_OBJECTS; ++j) {
      SDFCollider* collider = &colliders[j];
      if (collider->type == RGObjectType::BOX)
        continue;
      Vec3 d = positions[i] - collider->worldPosition;
      Vec3 localPosition = {d.Dot(collider->rotationAxes[0]),
			    d.Dot(collider->rotationAxes[1]),
			    d.Dot(collider->rotationAxes[2])};

      float pushDistance = sdfDispatch(collider->type, localPosition);
      if (pushDistance >= 0.0f) continue;

      Vec3 localGradient = sdfGradient(collider->type, localPosition);
      Vec3 worldGradient = collider->rotationAxes[0] * localGradient.x +
	collider->rotationAxes[1] * localGradient.y +
	collider->rotationAxes[2] * localGradient.z;
      positions[i] += worldGradient * (-pushDistance + 0.002f);
      float velocityDot = velocities[i].Dot(worldGradient);
      if (velocityDot < 0.0f)
	velocities[i] -= worldGradient * ((1.0f + collider->restitution) * velocityDot);
    }
  }
}

void gpuProjectParticleSDF(CudaBuffers &cb, Vec3 *predictedPositions_h,
                           Vec3 *velocities_h, int activeParticles) {
  projectParticleSDFKernel<<<ceil(activeParticles / 128.0), 128>>>(
      cb.predictedPositions_d, cb.velocities_d, cb.colliders_d, activeParticles);
  // cudaMemcpy(predictedPositions_h, cb.predictedPositions_d,
  //            activeParticles * sizeof(Vec3), cudaMemcpyDeviceToHost);
  // cudaMemcpy(velocities_h, cb.velocities_d, activeParticles * sizeof(Vec3),
  //            cudaMemcpyDeviceToHost);

}

// TODO: projectParticleTri

__global__ void updateVelocityKernel(Vec3 *predictedPositions, Vec3 *positions,
				     Vec3 *velocities, float dt, float mSpeed, int activeParticles) {
  int i = threadIdx.x + blockDim.x * blockIdx.x;

  if (i < activeParticles) {
    velocities[i] = (predictedPositions[i] - positions[i]) / dt;
    positions[i] = predictedPositions[i];

    float speed = velocities[i].Magnitude();
    if (speed > mSpeed)
      velocities[i] *= (mSpeed / speed);
  }
}

void gpuUpdateVelocities(CudaBuffers &cb, Vec3* predictedPositions_h, Vec3 *positions_h, Vec3 *velocities_h,
                         float dt, float mSpeed, int activeParticles) {
  updateVelocityKernel<<<ceil(activeParticles / 128.0), 128>>>(
							       cb.predictedPositions_d, cb.positions_d, cb.velocities_d, dt, mSpeed,
							       activeParticles);
  cudaMemcpy(positions_h, cb.positions_d, activeParticles * sizeof(Vec3),
             cudaMemcpyDeviceToHost);
  cudaMemcpy(predictedPositions_h, cb.predictedPositions_d,
             activeParticles * sizeof(Vec3), cudaMemcpyDeviceToHost);

}



__global__ void viscocityKernel(Vec3* predictedPositions, Vec3* velocities, int* neighbourData, int* neighbourCount, float h2, float xsphC, int activeParticles) {
  int i = threadIdx.x + blockDim.x * blockIdx.x;

  if (i < activeParticles) {
    Vec3 xsph = {0.0f, 0.0f, 0.0f};
    float wSum = 0.0f;

    int *currentNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
    for (int e{}; e < neighbourCount[i]; ++e) {
      int j = currentNeighbours[e];
      if (j == i)
        continue;

      Vec3 diff = predictedPositions[i] - predictedPositions[j];
      float d2 = diff.Dot(diff);
      if (d2 >= h2)
        continue;

      float sq = h2 - d2;
      float w = poly6_coeff_d * sq * sq * sq;
      xsph += (velocities[j] - velocities[i]) * w;
      wSum += w;
    }
    if (wSum > 1e-6f)
      xsph = xsph * (1.0f / wSum);

    Vec3 dv = xsph * xsphC;
    float mag = dv.Magnitude();
    if (mag > maxDv)
      dv = dv * (maxDv / mag);

    velocities[i] += dv;
  }
}

void gpuViscosity(CudaBuffers &cb, Vec3 *velocities_h, float h2, float xsphC,
                  int activeParticles) {
  viscocityKernel<<<ceil(activeParticles / 128.0), 128>>>(
      cb.predictedPositions_d, cb.velocities_d, cb.neighbourData_d,
      cb.neighbourCount_d, h2, xsphC, activeParticles);
  cudaMemcpy(velocities_h, cb.velocities_d, activeParticles * sizeof(Vec3),
             cudaMemcpyDeviceToHost);
}

__global__ void estimateRestDensityKernel(float* density, Vec3* predictedPositions, int* neighbourData, int* neighbourCount, int c, float h2) {
  int i = threadIdx.x + blockDim.x * blockIdx.x;

  int currentNeighbourCount = neighbourCount[c];
  if (i < currentNeighbourCount) {
    int j = neighbourData[(c * MAX_NEIGHBOURS) + i];
    if (j == c)
      return;

    Vec3 diff = predictedPositions[c] - predictedPositions[j];
    float d2 = diff.Dot(diff);
    if (d2 >= h2)
      return;

    float sq = h2 - d2;
    atomicAdd(density, poly6_coeff_d * sq * sq * sq);
  }
}

void gpuEstimateRestDensity(CudaBuffers& cb, float* density_h, float smoothingRadius, int numParticles) {
  int centre = numParticles / 2;
  float *density_d;

  float h2 = smoothingRadius * smoothingRadius;

  float poly6_coeff_h = 315.0f / (64.0f * PI * powf(smoothingRadius, 9));
  cudaError_t err;
  err = cudaMemcpyToSymbol(poly6_coeff_d, &poly6_coeff_h, sizeof(float));
  if (err != cudaSuccess) {
    printf("%s in %s at line %d \n", cudaGetErrorString(err), __FILE__,
	   __LINE__);
    exit(EXIT_FAILURE);
  }

  *density_h = poly6_coeff_h * h2 * h2 * h2;
  printf("init density: %f\n", *density_h);
  err = cudaMalloc((void **)&density_d, sizeof(float));
  if (err != cudaSuccess) {
    printf("%s in %s at line %d \n", cudaGetErrorString(err), __FILE__,
	   __LINE__);
    exit(EXIT_FAILURE);
  }
  err =
    cudaMemcpy(density_d, density_h, sizeof(float), cudaMemcpyHostToDevice);
  if (err != cudaSuccess) {
    printf("%s in %s at line %d \n", cudaGetErrorString(err), __FILE__,
	   __LINE__);
    exit(EXIT_FAILURE);
  }


  estimateRestDensityKernel<<<ceil(MAX_NEIGHBOURS/32.0), 32>>>(density_d, cb.predictedPositions_d, cb.neighbourData_d, cb.neighbourCount_d, centre, h2);

  err = cudaMemcpy(density_h, density_d, sizeof(float), cudaMemcpyDeviceToHost);
  if (err != cudaSuccess) {
    printf("%s in %s at line %d \n", cudaGetErrorString(err), __FILE__,
	   __LINE__);
    exit(EXIT_FAILURE);
  }
  cudaFree(density_d);

  printf("rest density: %f\n", *density_h);

}


__global__ void vorticityKernel(Vec3* velocities, Vec3* predictedPositions, Vec3* vorticities, int* neighbourData, int* neighbourCount, float smoothingRadius, float vorticityEpsilon, float dt, int activeParticles) {
  int i = threadIdx.x + blockIdx.x * blockDim.x;

  if (i < activeParticles) {
    Vec3 omega = {0.0f, 0.0f, 0.0f};
      const Vec3& vi = velocities[i];

      int* myNeighbours = &neighbourData[i * MAX_NEIGHBOURS];
      for (int k = 0; k < neighbourCount[i]; ++k) {
	int j = myNeighbours[k];
	if (j == i) continue;

	Vec3  diff = predictedPositions[i] - predictedPositions[j];
	float d2   = diff.Dot(diff);
	if (d2 < 1e-12f || d2 >= (smoothingRadius * smoothingRadius)) return;

	float d     = sqrtf(d2);
	float s     = (smoothingRadius - d) * (smoothingRadius - d) / d;
	Vec3  gradW = diff * s;
	Vec3 vij = velocities[j] - vi;
       
	omega.x += vij.y * gradW.z - vij.z * gradW.y;
	omega.y += vij.z * gradW.x - vij.x * gradW.z;
	omega.z += vij.x * gradW.y - vij.y * gradW.x;
      }
      vorticities[i] = omega;

      // Pass 2: apply vorticity confinement force
      Vec3 eta = {0.0f, 0.0f, 0.0f};

      for (int k = 0; k < neighbourCount[i]; ++k) {
	int j = myNeighbours[k];
	if (j == i) return;

	Vec3  diff = predictedPositions[i] - predictedPositions[j];
	float d2   = diff.Dot(diff);
	if (d2 < 1e-12f || d2 >= (smoothingRadius * smoothingRadius)) return;

	float d     = sqrtf(d2);
	float s     = (smoothingRadius - d) * (smoothingRadius - d) / d;
	Vec3  gradW = diff * s;

	float omegaMag = vorticities[j].Magnitude();
	eta += gradW * omegaMag;
      }

      float etaMag = eta.Magnitude();
      if (etaMag < 1e-6f) return;

      Vec3 N = eta * (1.0f / etaMag);       // location vector

      const Vec3& w = vorticities[i];
      Vec3 f_vorticity = {
	N.y * w.z - N.z * w.y,
	N.z * w.x - N.x * w.z,
	N.x * w.y - N.y * w.x
      };
      velocities[i] += f_vorticity * (vorticityEpsilon * dt);
  }
}

void gpuVorticity(CudaBuffers& cb, float smoothingRadius, float vorticityEpsilon, float dt, int activeParticles) {
  vorticityKernel<<<ceil(activeParticles / 128.0), 128>>>(
      cb.velocities_d, cb.predictedPositions_d, cb.vorticities_d,
      cb.neighbourData_d, cb.neighbourCount_d, smoothingRadius,
      vorticityEpsilon, dt, activeParticles);
}
