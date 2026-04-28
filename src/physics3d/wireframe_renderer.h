#pragma once
#include "linear_algebra.h"
#include <vector>

namespace physics3d {

// Immediate-mode 3D wireframe overlay.
// Batches line segments in world space and draws them with projection + view.
//
// Usage each frame:
//   renderer.addBox(center, halfSize, r, g, b);
//   renderer.flush(proj.entries, view.entries);
class WireframeRenderer {
public:
    WireframeRenderer();
    ~WireframeRenderer();

    // Queue a wireframe AABB for drawing this frame.
    void addBox(const Vec3& center, const Vec3& halfSize,
                float r = 1.0f, float g = 0.5f, float b = 0.0f);

    // Queue three axis-aligned circles approximating a sphere.
    void addSphere(const Vec3& center, float radius,
                   float r = 0.0f, float g = 0.9f, float b = 1.0f);

    // Queue a rectangle lying in the plane plus a short normal arrow.
    void addPlane(const Vec3& point, const Vec3& normal, float halfExtent = 0.8f,
                  float r = 1.0f, float g = 0.9f, float b = 0.1f);

    // Draw everything queued since the last flush, then clear the queue.
    // Must be called after gladLoadGLLoader and before glfwSwapBuffers.
    void flush(const float* projection, const float* view);

private:
    void buildShader();

    unsigned int m_vao        = 0;
    unsigned int m_vbo        = 0;
    unsigned int m_shader     = 0;
    int          m_vboFloats  = 0;   // current VBO capacity in floats

    // Interleaved per-vertex data: x y z r g b  (6 floats)
    std::vector<float> m_verts;
};

} // namespace physics3d
