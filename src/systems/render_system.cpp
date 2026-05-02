#include "render_system.h"
#include <glad/glad.h>

void Render(const CameraState &cameraState, const Viewport &viewport,
            Particles &particles, ParticleMesh &particleMesh, Grid &grid,
            unsigned int particleShader, unsigned int gridShader,
            float radiusLogical, float xScale) {


  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  float radiusPx = radiusLogical * xScale;

  int n = particles.numParticles;
  std::vector<float> posData(n * 3);
  std::vector<float> velData(n * 3);
  for (size_t i = 0; i < n; ++i)
    {
      posData[3 * i]     = particles.positions[i].x;
      posData[3 * i + 1] = particles.positions[i].y;
      posData[3 * i + 2] = particles.positions[i].z;
       
      velData[3 * i]     = particles.velocities[i].x;
      velData[3 * i + 1] = particles.velocities[i].y;
      velData[3 * i + 2] = particles.velocities[i].z;
    }

  glUseProgram(particleShader);
   
  Mat4 proj = Perspective(45.0f * PI / 180.0f, (float)viewport.screenWidth / viewport.screenHeight,
			  0.1f, 100.0f);
   
  glUniformMatrix4fv(glGetUniformLocation(particleShader, "projection"), 1, GL_FALSE, proj.entries);
  glUniformMatrix4fv(glGetUniformLocation(particleShader, "view"), 1, GL_FALSE,
                     cameraState.view.entries);

  float fov = 45.0f * PI / 180.0f;
  float viewRadius = (radiusLogical / viewport.screenHeight) * 2.0f * tan(fov / 2.0f);
  glUniform1f(glGetUniformLocation(particleShader, "radius"), viewRadius);

  glUniform3f(glGetUniformLocation(particleShader, "lightDir"), 0.6f, 0.8f, 1.0f);

  particleMesh.UpdateInstanceData(posData, velData);
  particleMesh.DrawInstanced((int)particles.positions.size());
  grid.draw(gridShader, proj.entries, cameraState.view.entries);
}
