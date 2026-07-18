#pragma once
#include <vector>
#include <glad/glad.h>
#include "linear_algebra.h"

#ifdef USE_CUDA
#include "cuda_runtime_api.h"
#include "cuda_gl_interop.h"
#include "cuda_buffers.cuh"
#else

#endif

class ParticleMesh {
public:
  ParticleMesh();
  ~ParticleMesh();
  void SetupInstanceBuffers(int num_particles);
  void UpdateInstanceData(const std::vector<float> &positions,
                          const std::vector<float> &velocities);
  void gpuUpdateInstanceData(Vec3 *positions_d, Vec3 *velocities_d,
                             int numParticles);
  void DrawInstanced(int num_particles);
    

private:
  unsigned int VAO, VBO, EBO;
  GLuint instancePosVBO, instanceVelVBO;
  cudaGraphicsResource *velocitiesCudaResource, *positionsCudaResource;
  int vertexCount;
};
