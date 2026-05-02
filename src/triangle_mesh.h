#pragma once
#include <vector>

class TriangleMesh {
public:
  TriangleMesh();
  ~TriangleMesh();
  void setupInstanceBuffers(int num_particles);
  void updateInstanceData(const std::vector<float> &positions,
                          const std::vector<float> &velocities);
  void drawInstanced(int num_particles);
    

private:
  unsigned int VAO, VBO, EBO;
  unsigned int instancePosVBO, instanceVelVBO;
  int vertexCount;
};
