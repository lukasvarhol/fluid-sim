#include "object_renderer.h"
#include <glad/glad.h>
#include <cmath>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#define TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJ_NO_INCLUDE_MAPBOX_EARCUT
#include "../../dependencies/tiny_obj_loader.h"

static std::string MeshKeyForType(RGObjectType type) {
  switch (type) {
  case RGObjectType::S_CHANNEL:   return "s_channel";
  case RGObjectType::L_CHANNEL:
    return "l_channel";
  case RGObjectType::T_RAMP:   return "t_ramp";
  case RGObjectType::B_RAMP:   return "b_ramp";
  default: return "";
  }
}

// ---------------------------------------------------------------------------
// Shader compilation helpers (mirrors main.cpp MakeModule / MakeShader)
// ---------------------------------------------------------------------------

static unsigned int CompileShaderModule(const char* src, unsigned int type)
{
  unsigned int id = glCreateShader(type);
  glShaderSource(id, 1, &src, nullptr);
  glCompileShader(id);
  int ok;
  glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[512];
    glGetShaderInfoLog(id, 512, nullptr, log);
    std::cerr << "ObjectRenderer shader error: " << log << "\n";
  }
  return id;
}

static unsigned int LinkSolidShader()
{
  // Read shader files the same way main.cpp does
  auto readFile = [](const char* path) -> std::string {
    std::ifstream f(path);
    if (!f.is_open()) {
      std::cerr << "Cannot open: " << path << "\n";
      return "";
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
  };

  std::string vs = readFile("src/shaders/solid_vertex.glsl");
  std::string fs = readFile("src/shaders/solid_fragment.glsl");

  unsigned int v = CompileShaderModule(vs.c_str(), GL_VERTEX_SHADER);
  unsigned int f = CompileShaderModule(fs.c_str(), GL_FRAGMENT_SHADER);

  unsigned int prog = glCreateProgram();
  glAttachShader(prog, v);
  glAttachShader(prog, f);
  glLinkProgram(prog);

  int ok;
  glGetProgramiv(prog, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[512];
    glGetProgramInfoLog(prog, 512, nullptr, log);
    std::cerr << "ObjectRenderer link error: " << log << "\n";
  }

  glDeleteShader(v);
  glDeleteShader(f);
  return prog;
}

// ---------------------------------------------------------------------------
// Setup / teardown
// ---------------------------------------------------------------------------

void SetupObjectRenderer(ObjectRenderer& r)
{
  r.shader = LinkSolidShader();

  glGenVertexArrays(1, &r.VAO);
  glGenBuffers(1, &r.VBO);

  glBindVertexArray(r.VAO);
  glBindBuffer(GL_ARRAY_BUFFER, r.VBO);

  LoadOBJ("meshes/LChannel.obj", r.loadedMeshes["l_channel"]);
  LoadOBJ("meshes/SChannel.obj", r.loadedMeshes["s_channel"]);
  LoadOBJ("meshes/BRamp.obj", r.loadedMeshes["b_ramp"]);
  LoadOBJ("meshes/TRamp.obj", r.loadedMeshes["t_ramp"]);

  // Vertex layout: pos(3) + normal(3) + colorAlpha(4) = 10 floats = 40 bytes
  constexpr int stride = 10 * sizeof(float);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
  glEnableVertexAttribArray(2);

  glBindVertexArray(0);
}

void DestroyObjectRenderer(ObjectRenderer& r)
{
  if (r.VBO) glDeleteBuffers(1, &r.VBO);
  if (r.VAO) glDeleteVertexArrays(1, &r.VAO);
  if (r.shader) glDeleteProgram(r.shader);
  r.VBO = r.VAO = r.shader = 0;
}

// ---------------------------------------------------------------------------
// Geometry helpers
// ---------------------------------------------------------------------------

static void PushVertex(std::vector<float>& buf,
                       Vec3 pos, Vec3 normal, Vec3 color, float alpha)
{
  buf.push_back(pos.x);    buf.push_back(pos.y);    buf.push_back(pos.z);
  buf.push_back(normal.x); buf.push_back(normal.y); buf.push_back(normal.z);
  buf.push_back(color.x);  buf.push_back(color.y);  buf.push_back(color.z);
  buf.push_back(alpha);
}

static void PushQuad(std::vector<float>& buf,
                     Vec3 v0, Vec3 v1, Vec3 v2, Vec3 v3,
                     Vec3 normal, Vec3 color, float alpha)
{
  // Two triangles: (v0,v1,v2) and (v0,v2,v3)
  PushVertex(buf, v0, normal, color, alpha);
  PushVertex(buf, v1, normal, color, alpha);
  PushVertex(buf, v2, normal, color, alpha);
  PushVertex(buf, v0, normal, color, alpha);
  PushVertex(buf, v2, normal, color, alpha);
  PushVertex(buf, v3, normal, color, alpha);
}

void AppendBox(std::vector<float>& buf,
               Vec3 center, Vec3 he, const Vec3 axes[3],
               Vec3 color, float alpha)
{
  Vec3 ax = axes[0] * he.x;   // scaled local X
  Vec3 ay = axes[1] * he.y;   // scaled local Y
  Vec3 az = axes[2] * he.z;   // scaled local Z

  // +Z face
  PushQuad(buf,
	   center + ax + ay + az,
	   center - ax + ay + az,
	   center - ax - ay + az,
	   center + ax - ay + az,
	   axes[2], color, alpha);
  // -Z face
  PushQuad(buf,
	   center + ax + ay - az,
	   center + ax - ay - az,
	   center - ax - ay - az,
	   center - ax + ay - az,
	   Vec3{-axes[2].x, -axes[2].y, -axes[2].z}, color, alpha);
  // +X face
  PushQuad(buf,
	   center + ax + ay + az,
	   center + ax - ay + az,
	   center + ax - ay - az,
	   center + ax + ay - az,
	   axes[0], color, alpha);
  // -X face
  PushQuad(buf,
	   center - ax + ay - az,
	   center - ax - ay - az,
	   center - ax - ay + az,
	   center - ax + ay + az,
	   Vec3{-axes[0].x, -axes[0].y, -axes[0].z}, color, alpha);
  // +Y face
  PushQuad(buf,
	   center + ax + ay - az,
	   center - ax + ay - az,
	   center - ax + ay + az,
	   center + ax + ay + az,
	   axes[1], color, alpha);
  // -Y face
  PushQuad(buf,
	   center + ax - ay + az,
	   center - ax - ay + az,
	   center - ax - ay - az,
	   center + ax - ay - az,
	   Vec3{-axes[1].x, -axes[1].y, -axes[1].z}, color, alpha);
}

void AppendSphere(std::vector<float>& buf,
                  Vec3 center, float radius, Vec3 color, float alpha)
{
  const int nLat = 10;
  const int nLon = 10;
  const float PI_F = 3.14159265f;

  for (int i = 0; i < nLat; ++i) {
    float theta0 = PI_F * i       / nLat;
    float theta1 = PI_F * (i + 1) / nLat;
    for (int j = 0; j < nLon; ++j) {
      float phi0 = 2.0f * PI_F * j       / nLon;
      float phi1 = 2.0f * PI_F * (j + 1) / nLon;

      auto sphVert = [&](float th, float ph) -> std::pair<Vec3,Vec3> {
	float x = sinf(th) * cosf(ph);
	float y = cosf(th);
	float z = sinf(th) * sinf(ph);
	Vec3 n{x, y, z};
	Vec3 p{center.x + radius * x, center.y + radius * y, center.z + radius * z};
	return {p, n};
      };

      auto [p00, n00] = sphVert(theta0, phi0);
      auto [p01, n01] = sphVert(theta0, phi1);
      auto [p10, n10] = sphVert(theta1, phi0);
      auto [p11, n11] = sphVert(theta1, phi1);

      // Triangle 1
      PushVertex(buf, p00, n00, color, alpha);
      PushVertex(buf, p10, n10, color, alpha);
      PushVertex(buf, p11, n11, color, alpha);
      // Triangle 2
      PushVertex(buf, p00, n00, color, alpha);
      PushVertex(buf, p11, n11, color, alpha);
      PushVertex(buf, p01, n01, color, alpha);
    }
  }
}

// ---------------------------------------------------------------------------
// Per-object-type color map
// ---------------------------------------------------------------------------

static Vec3 ObjectColor(RGObjectType type, bool selected)
{
  Vec3 c{0.15, 0.4, 0.35};
  if (selected) { c.x = std::min(1.0f, c.x * 1.6f); c.y = std::min(1.0f, c.y * 1.6f); c.z = std::min(1.0f, c.z * 1.6f); }
  return c;
}

// ---------------------------------------------------------------------------
// Per-type child-box decomposition
// Child boxes are defined in PARENT LOCAL SPACE.
// axes[] are pre-computed from parent rotation (TransformDir).
// ---------------------------------------------------------------------------

struct ChildBox { Vec3 localCenter; Vec3 halfExtents; };

static std::vector<ChildBox> GetChildBoxes(const RGObject& obj) {
  switch (obj.type) {
  case RGObjectType::L_CHANNEL:
  case RGObjectType::S_CHANNEL:
  case RGObjectType::B_RAMP:
  case RGObjectType::T_RAMP:
    return {}; 
  }
  return {};
}
// ---------------------------------------------------------------------------
// AppendObject — generate all visual geometry for one RGObject
// ---------------------------------------------------------------------------

void AppendObject(std::vector<float>& buf, const RGObject& obj,
                  Vec3 color, float alpha)
{

  // Build rotation matrix from object's Euler angles
  Mat4 rotMat = CreateMatrixRotationXYZ(obj.rotation);

  Vec3 axes[3] = {
    TransformDir(rotMat, {1, 0, 0}),
    TransformDir(rotMat, {0, 1, 0}),
    TransformDir(rotMat, {0, 0, 1})
  };

  Vec3 worldPos = obj.position;

  auto children = GetChildBoxes(obj);
  for (const auto& cb : children) {
    Vec3 worldCenter = worldPos + TransformDir(rotMat, cb.localCenter);
    AppendBox(buf, worldCenter, cb.halfExtents, axes, color, alpha);
  }
}

// ---------------------------------------------------------------------------
// Upload and draw
// ---------------------------------------------------------------------------

static void FlushBuffer(ObjectRenderer& r)
{
  if (r.buffer.empty()) return;
  glBindVertexArray(r.VAO);
  glBindBuffer(GL_ARRAY_BUFFER, r.VBO);
  glBufferData(GL_ARRAY_BUFFER,
	       (GLsizeiptr)(r.buffer.size() * sizeof(float)),
	       r.buffer.data(), GL_DYNAMIC_DRAW);
  int vertCount = (int)(r.buffer.size() / 10);
  glDrawArrays(GL_TRIANGLES, 0, vertCount);
  glBindVertexArray(0);
  r.buffer.clear();
}

void RenderObjects(ObjectRenderer& r,
                   const std::vector<RGObject>& objects,
                   const std::vector<RGObject>* previewObjs,
                   const GridState* grid,
                   bool showSelectedCell,
                   bool showOccupiedOutlines,
                   const Mat4& view,
                   const Mat4& projection, Vec3 cameraPos)
{
  if (!r.shader)
    return;

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);

  GLboolean depthMaskSaved;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskSaved);

  glUseProgram(r.shader);
  glUniformMatrix4fv(glGetUniformLocation(r.shader, "uView"),       1, GL_FALSE, view.entries);
  glUniformMatrix4fv(glGetUniformLocation(r.shader, "uProjection"), 1, GL_FALSE, projection.entries);

  // --- Pass 1: opaque active objects ---
  glDepthMask(GL_TRUE);
  for (const RGObject& obj : objects) {
    if (!obj.active) continue;
    Vec3 col = (obj.colorOverride.x >= 0.0f)
      ? obj.colorOverride
      : ObjectColor(obj.type, false);
    
    // check if we have a loaded mesh for this type
    std::string meshKey = MeshKeyForType(obj.type);
    if (!meshKey.empty() && r.loadedMeshes.count(meshKey)) {
      Mat4 rot = CreateMatrixRotationXYZ(obj.rotation);
      Mat4 trans = CreateMatrixTransform(obj.position);
      Mat4 model = Mat4Multiply(trans, rot);
      glUniform4f(glGetUniformLocation(r.shader, "uColor"), col.x, col.y, col.z, 0.6f);
      DrawMesh(r.loadedMeshes[meshKey], model, view, projection, r.shader, cameraPos);
    } else {
      AppendObject(r.buffer, obj, col, 1.0f);
    }
  }
  FlushBuffer(r);

  // --- Pass 3: occupied cell outlines ---
  if (showOccupiedOutlines && grid) {
    Vec3 identAxes[3] = {{1,0,0},{0,1,0},{0,0,1}};
    constexpr float h = CELL_SIZE * 0.5f - 0.002f; // 0.198
    glDepthMask(GL_FALSE);
    for (int z = 0; z < GRID_Z; ++z)
      for (int y = 0; y < GRID_Y; ++y)
        for (int x = 0; x < GRID_X; ++x) {
	  if (grid->GetCell(x, y, z).feature == Feature::EMPTY) continue;
	  Vec3 c = grid->CellCenterWorld(x, y, z);
	  AppendBox(r.buffer, c, {h, h, h}, identAxes, {0.8f, 0.8f, 0.8f}, 0.30f);
        }
    FlushBuffer(r);
    glDepthMask(GL_TRUE);
  }

  // --- Pass 4: selected-cell highlight (semi-transparent yellow) ---
  if (showSelectedCell && grid) {
    Vec3 identAxes[3] = {{1,0,0},{0,1,0},{0,0,1}};
    constexpr float h = CELL_SIZE * 0.5f - 0.001f; // 0.199 — avoids Z-fight with cell lines
    Vec3 sel = grid->CellCenterWorld(grid->selX, grid->selY, grid->selZ);
    glDepthMask(GL_FALSE);
    AppendBox(r.buffer, sel, {h, h, h}, identAxes, {1.0f, 0.9f, 0.1f}, 0.25f);
    FlushBuffer(r);
    glDepthMask(GL_TRUE);
  }

  glDepthMask(depthMaskSaved);
}

void LoadOBJ(const std::string &path, MeshData &out, float scale) {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string err;
  bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, path.c_str());
  if (!err.empty()) std::cerr << "TinyObjLoader: " << err << "\n";
  if (!ret) return;

  for (size_t s = 0; s < shapes.size(); s++) {
    // Loop over faces(polygon)
    size_t index_offset = 0;
    for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
      size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);

      // Loop over vertices in the face.
      for (size_t v = 0; v < fv; v++) {
	// access to vertex
	tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
	tinyobj::real_t vx = attrib.vertices[3*size_t(idx.vertex_index)+0];
	tinyobj::real_t vy = attrib.vertices[3*size_t(idx.vertex_index)+1];
        tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

        out.vertices.push_back(vx * scale);
	out.vertices.push_back(vz * scale);
        out.vertices.push_back(vy * scale);
        
	float nx = 0, ny = 1, nz = 0; // default up normal
	if (idx.normal_index >= 0) {
	  nx = attrib.normals[3 * idx.normal_index + 0];
	  ny = attrib.normals[3 * idx.normal_index + 1];
	  nz = attrib.normals[3 * idx.normal_index + 2];
	}
        out.vertices.push_back(nx);
	out.vertices.push_back(nz);
	out.vertices.push_back(ny);
	
	out.indices.push_back(out.indices.size());
      }
      
      index_offset += fv;
    }
  }
  out.indexCount = (int)out.indices.size();

  glGenVertexArrays(1, &out.VAO);
  glBindVertexArray(out.VAO);

  glGenBuffers(1, &out.VBO);
  glBindBuffer(GL_ARRAY_BUFFER, out.VBO);
  glBufferData(GL_ARRAY_BUFFER, out.vertices.size() * sizeof(float),
	       out.vertices.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 24, (void *)0);
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 24, (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  glGenBuffers(1, &out.EBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
	       out.indices.size() * sizeof(unsigned int), out.indices.data(),
	       GL_STATIC_DRAW);
  glBindVertexArray(0);
}

void DrawMesh(const MeshData &mesh, const Mat4 &model, const Mat4 &view,
              const Mat4 &projection, unsigned int shader, Vec3 cameraPos) {
  glUseProgram(shader);
  glUniformMatrix4fv(glGetUniformLocation(shader, "uModel"), 1, GL_FALSE, model.entries);
  glUniformMatrix4fv(glGetUniformLocation(shader, "uView"), 1, GL_FALSE, view.entries);
  glUniformMatrix4fv(glGetUniformLocation(shader, "uProjection"), 1, GL_FALSE,
		     projection.entries);
  glUniform3f(glGetUniformLocation(shader, "uCameraPos"), 
	      cameraPos.x, cameraPos.y, cameraPos.z);
  glBindVertexArray(mesh.VAO);
  glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}


