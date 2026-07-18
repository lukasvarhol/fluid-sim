#include "render_system.h"
#include <glad/glad.h>

void Render(const CameraState &cameraState, const Viewport &viewport,
            Particles &particles, ParticleMesh &particleMesh,
            unsigned int particleShader,
            const std::vector<SceneObject>& sceneObjects,
            float radiusLogical, float xScale, AppState* as) {

  int n = particles.activeParticles;
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#ifdef USE_CUDA
#else

  float radiusPx = radiusLogical * xScale;


  std::vector<float> posData(n * 3);
  std::vector<float> velData(n * 3);
  for (int i = 0; i < n; ++i)
    {
      posData[3 * i]     = particles.positions[i].x;
      posData[3 * i + 1] = particles.positions[i].y;
      posData[3 * i + 2] = particles.positions[i].z;

      velData[3 * i]     = particles.velocities[i].x;
      velData[3 * i + 1] = particles.velocities[i].y;
      velData[3 * i + 2] = particles.velocities[i].z;
    }

#endif
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

#ifdef USE_CUDA
  
  particleMesh.gpuUpdateInstanceData(as->cudaBuffers->positions_d, as->cudaBuffers->velocities_d, n);
#else
  particleMesh.UpdateInstanceData(posData, velData);
#endif

  particleMesh.DrawInstanced(n);

  for (const auto& obj : sceneObjects) {
    if (!obj.visible) continue;
    obj.renderer->Draw(obj.shader, proj.entries, cameraState.view.entries);
  }
}
