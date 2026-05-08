#pragma once

#include "camera_system.h"
#include "../particles.h"
#include "../particle_mesh.h"
#include "../app/viewport.h"
#include "../line_renderer.h"
#include "../app/scene_manager.h"

void Render(const CameraState &cameraState, const Viewport &viewport,
            Particles &particles, ParticleMesh &particleMesh,
            unsigned int particleShader,
            const std::vector<SceneObject>& sceneObjects,
            float radiusLogical, float xScale);
