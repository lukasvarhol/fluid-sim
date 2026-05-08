#pragma once
#include <vector>

class ParticleMesh {
public:
  ParticleMesh();
  ~ParticleMesh();
  void SetupInstanceBuffers(int num_particles);
  void UpdateInstanceData(const std::vector<float> &positions,
                          const std::vector<float> &velocities);
  void DrawInstanced(int num_particles);
    

private:
  unsigned int VAO, VBO, EBO;
  unsigned int instancePosVBO, instanceVelVBO;
  int vertexCount;
};
