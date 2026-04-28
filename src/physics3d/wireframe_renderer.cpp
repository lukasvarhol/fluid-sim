#include "physics3d/wireframe_renderer.h"
#include "glad/glad.h"
#include <iostream>
#include <cmath>

namespace physics3d {

// ---------------------------------------------------------------------------
// Inline GLSL — world-space positions, per-vertex RGB color
// ---------------------------------------------------------------------------
static const char* kVert = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 projection;
uniform mat4 view;
out vec3 vColor;
void main() {
    gl_Position = projection * view * vec4(aPos, 1.0);
    vColor = aColor;
}
)";

static const char* kFrag = R"(
#version 330 core
in  vec3 vColor;
out vec4 fragColor;
void main() { fragColor = vec4(vColor, 1.0); }
)";

// ---------------------------------------------------------------------------
static unsigned int compileStage(const char* src, unsigned int type)
{
    unsigned int s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    int ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, 512, nullptr, log);
        std::cerr << "WireframeRenderer shader compile error: " << log << "\n";
    }
    return s;
}

// ---------------------------------------------------------------------------
WireframeRenderer::WireframeRenderer()
{
    buildShader();

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    constexpr int stride = 6 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));

    glBindVertexArray(0);
}

WireframeRenderer::~WireframeRenderer()
{
    if (m_vbo)    glDeleteBuffers(1, &m_vbo);
    if (m_vao)    glDeleteVertexArrays(1, &m_vao);
    if (m_shader) glDeleteProgram(m_shader);
}

void WireframeRenderer::buildShader()
{
    unsigned int vs = compileStage(kVert, GL_VERTEX_SHADER);
    unsigned int fs = compileStage(kFrag, GL_FRAGMENT_SHADER);
    m_shader = glCreateProgram();
    glAttachShader(m_shader, vs);
    glAttachShader(m_shader, fs);
    glLinkProgram(m_shader);
    int ok;
    glGetProgramiv(m_shader, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(m_shader, 512, nullptr, log);
        std::cerr << "WireframeRenderer link error: " << log << "\n";
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
}

// ---------------------------------------------------------------------------
void WireframeRenderer::addBox(const Vec3& c, const Vec3& hs,
                                float r, float g, float b)
{
    // 8 corners of the AABB
    const float corners[8][3] = {
        {c.x - hs.x, c.y - hs.y, c.z - hs.z},  // 0
        {c.x + hs.x, c.y - hs.y, c.z - hs.z},  // 1
        {c.x + hs.x, c.y + hs.y, c.z - hs.z},  // 2
        {c.x - hs.x, c.y + hs.y, c.z - hs.z},  // 3
        {c.x - hs.x, c.y - hs.y, c.z + hs.z},  // 4
        {c.x + hs.x, c.y - hs.y, c.z + hs.z},  // 5
        {c.x + hs.x, c.y + hs.y, c.z + hs.z},  // 6
        {c.x - hs.x, c.y + hs.y, c.z + hs.z},  // 7
    };

    // 12 edges: bottom ring, top ring, 4 vertical pillars
    constexpr int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},  // bottom face
        {4,5},{5,6},{6,7},{7,4},  // top face
        {0,4},{1,5},{2,6},{3,7},  // vertical edges
    };

    for (auto& e : edges) {
        for (int v = 0; v < 2; ++v) {
            m_verts.push_back(corners[e[v]][0]);
            m_verts.push_back(corners[e[v]][1]);
            m_verts.push_back(corners[e[v]][2]);
            m_verts.push_back(r);
            m_verts.push_back(g);
            m_verts.push_back(b);
        }
    }
}

// ---------------------------------------------------------------------------
void WireframeRenderer::addSphere(const Vec3& center, float radius,
                                   float r, float g, float b)
{
    constexpr int kSegs = 24;
    constexpr float kStep = 6.28318530718f / kSegs;

    auto pushSeg = [&](float ax, float ay, float az,
                       float bx, float by, float bz) {
        m_verts.push_back(ax); m_verts.push_back(ay); m_verts.push_back(az);
        m_verts.push_back(r);  m_verts.push_back(g);  m_verts.push_back(b);
        m_verts.push_back(bx); m_verts.push_back(by); m_verts.push_back(bz);
        m_verts.push_back(r);  m_verts.push_back(g);  m_verts.push_back(b);
    };

    for (int i = 0; i < kSegs; ++i) {
        float a0 = i * kStep, a1 = (i + 1) * kStep;
        float c0 = std::cos(a0) * radius, s0 = std::sin(a0) * radius;
        float c1 = std::cos(a1) * radius, s1 = std::sin(a1) * radius;
        // XY circle
        pushSeg(center.x + c0, center.y + s0, center.z,
                center.x + c1, center.y + s1, center.z);
        // XZ circle
        pushSeg(center.x + c0, center.y, center.z + s0,
                center.x + c1, center.y, center.z + s1);
        // YZ circle
        pushSeg(center.x, center.y + c0, center.z + s0,
                center.x, center.y + c1, center.z + s1);
    }
}

// ---------------------------------------------------------------------------
void WireframeRenderer::addPlane(const Vec3& point, const Vec3& normal,
                                  float halfExtent, float r, float g, float b)
{
    Vec3 n = normalize(normal);
    Vec3 ref = (std::fabsf(n.y) < 0.9f) ? Vec3{0.f, 1.f, 0.f}
                                         : Vec3{1.f, 0.f, 0.f};
    Vec3 t1 = normalize(cross(n, ref));
    Vec3 t2 = cross(n, t1);

    Vec3 corners[4] = {
        point + t1 * halfExtent + t2 * halfExtent,
        point - t1 * halfExtent + t2 * halfExtent,
        point - t1 * halfExtent - t2 * halfExtent,
        point + t1 * halfExtent - t2 * halfExtent,
    };

    auto pushSeg = [&](const Vec3& va, const Vec3& vb) {
        m_verts.push_back(va.x); m_verts.push_back(va.y); m_verts.push_back(va.z);
        m_verts.push_back(r);    m_verts.push_back(g);    m_verts.push_back(b);
        m_verts.push_back(vb.x); m_verts.push_back(vb.y); m_verts.push_back(vb.z);
        m_verts.push_back(r);    m_verts.push_back(g);    m_verts.push_back(b);
    };

    pushSeg(corners[0], corners[1]);
    pushSeg(corners[1], corners[2]);
    pushSeg(corners[2], corners[3]);
    pushSeg(corners[3], corners[0]);

    // Normal arrow
    Vec3 tip = point + n * (halfExtent * 0.4f);
    pushSeg(point, tip);
}

// ---------------------------------------------------------------------------
void WireframeRenderer::flush(const float* projection, const float* view)
{
    if (m_verts.empty())
        return;

    const int needed = (int)m_verts.size();

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    if (needed > m_vboFloats) {
        glBufferData(GL_ARRAY_BUFFER, needed * sizeof(float),
                     m_verts.data(), GL_DYNAMIC_DRAW);
        m_vboFloats = needed;
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, needed * sizeof(float), m_verts.data());
    }

    glUseProgram(m_shader);
    glUniformMatrix4fv(glGetUniformLocation(m_shader, "projection"),
                       1, GL_FALSE, projection);
    glUniformMatrix4fv(glGetUniformLocation(m_shader, "view"),
                       1, GL_FALSE, view);

    // 6 floats per vertex
    glDrawArrays(GL_LINES, 0, needed / 6);

    glBindVertexArray(0);
    m_verts.clear();
}

} // namespace physics3d
