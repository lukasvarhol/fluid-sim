#include "particle_mesh.h"

ParticleMesh::ParticleMesh() {
    std::vector<float> positions = {
        -1.0f, -1.0f, 0.0f, 
         1.0f, -1.0f, 0.0f, 
        -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f, 0.0f
    };

    std::vector<unsigned int> elementIndices = {
        0, 1, 2, 2, 1, 3
    };
    vertexCount = 6;

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // position
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(float), positions.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, (void*)0);
    glEnableVertexAttribArray(0);

    //emement buffer
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 elementIndices.size() * sizeof(unsigned int),
                 elementIndices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
}

void ParticleMesh::SetupInstanceBuffers(int num_particles){
    glBindVertexArray(VAO);

    // instance positions
    glGenBuffers(1, &instancePosVBO);

    glBindBuffer(GL_ARRAY_BUFFER, instancePosVBO);
    glBufferData(GL_ARRAY_BUFFER, num_particles * sizeof(Vec3), nullptr,
                 GL_DYNAMIC_DRAW);

#ifdef USE_CUDA
    cudaError_t err = cudaGraphicsGLRegisterBuffer(
        &positionsCudaResource, instancePosVBO, cudaGraphicsRegisterFlagsNone);
    if (err != cudaSuccess) printf("register pos failed: %s\n", cudaGetErrorString(err));
#endif
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1,1);

    // instance velocities
    glGenBuffers(1, &instanceVelVBO);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVelVBO);
    glBufferData(GL_ARRAY_BUFFER, num_particles * sizeof(Vec3), nullptr,
                 GL_DYNAMIC_DRAW);
#ifdef USE_CUDA
    err = cudaGraphicsGLRegisterBuffer(&velocitiesCudaResource, instanceVelVBO,
                                       cudaGraphicsRegisterFlagsNone);
    if (err != cudaSuccess) printf("register vel failed: %s\n", cudaGetErrorString(err));
#endif
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);
}

void ParticleMesh::UpdateInstanceData(const std::vector<Vec3>& positions, const std::vector<Vec3>& velocities){
    glBindBuffer(GL_ARRAY_BUFFER, instancePosVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, positions.size() * sizeof(Vec3), positions.data());

    glBindBuffer(GL_ARRAY_BUFFER, instanceVelVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, velocities.size() * sizeof(Vec3), velocities.data());
}

#ifdef USE_CUDA
void ParticleMesh::gpuUpdateInstanceData(Vec3 *positions_d, Vec3 *velocities_d,
                                         int numParticles) {
  cudaError_t err = cudaGraphicsMapResources(1, &positionsCudaResource, 0);
  if (err != cudaSuccess) printf("map pos failed: %s\n", cudaGetErrorString(err));
  err = cudaGraphicsMapResources(1, &velocitiesCudaResource, 0);
  if (err != cudaSuccess)
    printf("map vel failed: %s\n", cudaGetErrorString(err));
  
  Vec3* mappedPos;
  Vec3* mappedVel;
  size_t num_bytes;
  cudaGraphicsResourceGetMappedPointer((void**)&mappedPos, &num_bytes, positionsCudaResource);
  cudaGraphicsResourceGetMappedPointer((void**)&mappedVel, &num_bytes, velocitiesCudaResource);

  cudaMemcpy(mappedPos, positions_d, numParticles * sizeof(Vec3), cudaMemcpyDeviceToDevice);
  cudaMemcpy(mappedVel, velocities_d, numParticles * sizeof(Vec3), cudaMemcpyDeviceToDevice);

  cudaGraphicsUnmapResources(1, &positionsCudaResource, 0);
  cudaGraphicsUnmapResources(1, &velocitiesCudaResource, 0);
}
#endif

void ParticleMesh::DrawInstanced(int num_particles) {
    glBindVertexArray(VAO);
    glDrawElementsInstanced(GL_TRIANGLES, vertexCount, GL_UNSIGNED_INT, 0, num_particles);
}

ParticleMesh::~ParticleMesh() {
#ifdef USE_CUDA
  cudaGraphicsUnregisterResource(positionsCudaResource);
  cudaGraphicsUnregisterResource(velocitiesCudaResource);
#endif
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteBuffers(1, &EBO);
  glDeleteBuffers(1, &instancePosVBO);
  glDeleteBuffers(1, &instanceVelVBO);
}
