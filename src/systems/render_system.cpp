#include "render_system.h"
#include <glad/glad.h>

void Render(const CameraState &cameraState, const Viewport &viewport,
            Particles &particles, ParticleMesh &particleMesh,
            unsigned int particleShader,
            const std::vector<SceneObject>& sceneObjects,
            float radiusLogical, float xScale, AppState* as) {

  int n = particles.activeParticles;
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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
  particleMesh.UpdateInstanceData(particles.positions, particles.velocities);
#endif

  particleMesh.DrawInstanced(n);

  for (const auto& obj : sceneObjects) {
    if (!obj.visible) continue;
    obj.renderer->Draw(obj.shader, proj.entries, cameraState.view.entries);
  }
}
