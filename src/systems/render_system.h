#pragma once

#include "camera_system.h"
#include "../particles.h"
#include "../particle_mesh.h"
#include "../app/viewport.h"
#include "../grid.h"

void Render(const CameraState &cameraState, const Viewport &viewport,
            Particles &particles, ParticleMesh &particleMesh, Grid &grid,
            unsigned int particleShader, unsigned int gridShader,
            float radiusLogical, float xScale);
