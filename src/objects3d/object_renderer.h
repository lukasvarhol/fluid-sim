#pragma once
#include <vector>
#include "objects3d/editor_state.h"
#include "linear_algebra.h"

struct ObjectRenderer {
    unsigned int VAO    = 0;
    unsigned int VBO    = 0;
    unsigned int shader = 0;
    std::vector<float> buffer; // rebuilt each frame on CPU
};

void SetupObjectRenderer(ObjectRenderer& r);

// Append 6-faced OBB geometry into buffer.
// axes[0..2] are the local X/Y/Z unit vectors in world space.
void AppendBox(std::vector<float>& buf,
               Vec3 center, Vec3 halfExtents, const Vec3 axes[3],
               Vec3 color, float alpha);

// Append a low-poly sphere into buffer
void AppendSphere(std::vector<float>& buf,
                  Vec3 center, float radius,
                  Vec3 color, float alpha);

// Generate all primitives for one object and append to buf
void AppendObject(std::vector<float>& buf, const RGObject& obj,
                  Vec3 color, float alpha);

// Render all active objects, optional ghost preview, optional cell overlays.
// previewObjs: null = no ghost pass; grid: null = no cell overlays.
void RenderObjects(ObjectRenderer& r,
                   const std::vector<RGObject>& objects,
                   const std::vector<RGObject>* previewObjs,
                   const GridState* grid,
                   bool showSelectedCell,
                   bool showOccupiedOutlines,
                   const Mat4& view,
                   const Mat4& projection);

void DestroyObjectRenderer(ObjectRenderer& r);
