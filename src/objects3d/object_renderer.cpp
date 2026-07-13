#include "object_renderer.h"
#include <glad/glad.h>
#include <cmath>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <tuple>

#define TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJ_NO_INCLUDE_MAPBOX_EARCUT
#include "../../dependencies/tiny_obj_loader.h"

static std::string MeshKeyForType(RGObjectType type) {
  switch (type) {
  case RGObjectType::S_CHANNEL:   return "s_channel";
  case RGObjectType::L_CHANNEL:
    return "l_channel";
  case RGObjectType::RAMP:   return "ramp";
  default: return "";
  }
}

struct FaceData {
  Vec3 positions[3];
  Vec3 faceNormal;
};

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

static unsigned int LinkOverlayShader()
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

  std::string vs = readFile("src/shaders/overlay_vertex.glsl");
  std::string fs = readFile("src/shaders/overlay_fragment.glsl");

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
  r.overlayShader = LinkOverlayShader();
  printf("overlayShader=%u\n", r.overlayShader);

  glGenVertexArrays(1, &r.VAO);
  glGenBuffers(1, &r.VBO);

  glBindVertexArray(r.VAO);
  glBindBuffer(GL_ARRAY_BUFFER, r.VBO);

  LoadOBJ("meshes/LChannel.obj", r.loadedMeshes["l_channel"]);
  LoadOBJ("meshes/SChannel.obj", r.loadedMeshes["s_channel"]);
  LoadOBJ("meshes/Ramp.obj", r.loadedMeshes["ramp"]);

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

// ---------------------------------------------------------------------------
// Per-object-type color map
// ---------------------------------------------------------------------------

static Vec3 ObjectColor(RGObjectType type, bool selected)
{
  Vec3 c{0.25, 0.4, 0.41};
  if (selected) { c.x = std::min(1.0f, c.x * 1.6f); c.y = std::min(1.0f, c.y * 1.6f); c.z = std::min(1.0f, c.z * 1.6f); }
  return c;
}


// ---------------------------------------------------------------------------
// Upload and draw
// ---------------------------------------------------------------------------

static void FlushBuffer(ObjectRenderer& r, Vec3 color, float alpha)
{
  unsigned int tempVAO, tempVBO;
  glGenVertexArrays(1, &tempVAO);
  glGenBuffers(1, &tempVBO);
  glBindVertexArray(tempVAO);
  glBindBuffer(GL_ARRAY_BUFFER, tempVBO);
  glBufferData(GL_ARRAY_BUFFER, r.buffer.size() * sizeof(float), r.buffer.data(), GL_DYNAMIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 40, (void*)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 40, (void*)(6 * sizeof(float)));
  glEnableVertexAttribArray(2);
  Mat4 identity = Mat4::Identity();
  glUniformMatrix4fv(glGetUniformLocation(r.shader, "uModel"), 1, GL_FALSE, identity.entries);
  glUniform4f(glGetUniformLocation(r.shader, "uColor"), color.x, color.y, color.z, alpha);
  glDrawArrays(GL_TRIANGLES, 0, r.buffer.size() / 10);
  glBindVertexArray(0);
  glDeleteVertexArrays(1, &tempVAO);
  glDeleteBuffers(1, &tempVBO);
  r.buffer.clear();
}

void RenderObjects(ObjectRenderer& r,
                   RGObject* objects,
                   RGObject* previewObjs,
                   const GridState* grid,
                   bool showSelectedCell,
                   bool showOccupiedOutlines,
                   const Mat4& view,
                   const Mat4& projection, Vec3 cameraPos, const std::vector<SDFCollider>& colliders)
{
  if (!r.shader)
    return;

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  GLboolean depthMaskSaved;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskSaved);

  glUseProgram(r.shader);
  glUniformMatrix4fv(glGetUniformLocation(r.shader, "uView"),       1, GL_FALSE, view.entries);
  glUniformMatrix4fv(glGetUniformLocation(r.shader, "uProjection"), 1, GL_FALSE, projection.entries);

  // --- Pass 1: opaque active objects ---
  glDepthMask(GL_TRUE);
  for (int j{}; j < MAX_OBJECTS; ++j) {
    if (!objects[j].active) continue;
    Vec3 col = (objects[j].colorOverride.x >= 0.0f)
      ? objects[j].colorOverride
      : ObjectColor(objects[j].type, false);
    
    // check if we have a loaded mesh for this type
    std::string meshKey = MeshKeyForType(objects[j].type);
    if (!meshKey.empty() && r.loadedMeshes.count(meshKey)) {
      Mat4 rot = CreateMatrixRotationXYZ(objects[j].rotation);
      Mat4 trans = CreateMatrixTransform(objects[j].position);
      Mat4 model = Mat4Multiply(trans, rot);
      glUniform4f(glGetUniformLocation(r.shader, "uColor"), col.x, col.y, col.z, 0.6f);
      DrawMesh(r.loadedMeshes[meshKey], model, view, projection, r.shader,
               cameraPos);
    }
  }


  if (previewObjs) {
    for (int j{}; j < MAX_OBJECTS; ++j) {
      if (!previewObjs[j].active) continue;
      Vec3 col = ObjectColor(previewObjs[j].type, false);
      std::string meshKey = MeshKeyForType(previewObjs[j].type);
      if (!meshKey.empty() && r.loadedMeshes.count(meshKey)) {
	Mat4 rot = CreateMatrixRotationXYZ(previewObjs[j].rotation);
	Mat4 trans = CreateMatrixTransform(previewObjs[j].position);
	Mat4 model = Mat4Multiply(trans, rot);
	glUniform4f(glGetUniformLocation(r.shader, "uColor"), col.x, col.y, col.z, 0.3f);
	DrawMesh(r.loadedMeshes[meshKey], model, view, projection, r.shader, cameraPos);
      }
    }
  }

  if (showOccupiedOutlines && grid) {
    Vec3 identAxes[3] = {{1,0,0},{0,1,0},{0,0,1}};
    constexpr float h = CELL_SIZE * 0.5f - 0.002f;
    glDepthMask(GL_FALSE);
    for (int z = 0; z < GRID_Z; ++z)
      for (int y = 0; y < GRID_Y; ++y)
        for (int x = 0; x < GRID_X; ++x) {
          if (grid->GetCell(x, y, z).feature == Feature::EMPTY) continue;
          Vec3 c = grid->CellCenterWorld(x, y, z);
          AppendBox(r.buffer, c, {h, h, h}, identAxes, {0.8f, 0.8f, 0.8f}, 0.30f);
        }
    Mat4 identity = Mat4::Identity();
    glUniformMatrix4fv(glGetUniformLocation(r.shader, "uModel"), 1, GL_FALSE, identity.entries);
    glBindVertexArray(r.VAO);
    // For occupied outlines:
    //glUniform4f(glGetUniformLocation(r.shader, "uColor"), 0.8f, 0.8f, 0.8f, 0.30f);

    // For selected cell:
    //glUniform4f(glGetUniformLocation(r.shader, "uColor"), 1.0f, 0.9f, 0.1f, 0.25f);
    FlushBuffer(r, {0.0f, 0.0f, 0.0f}, 0.0f);
    glDepthMask(GL_TRUE);
  }

  // --- Pass 4: selected-cell highlight (semi-transparent yellow) ---
  if (showSelectedCell && grid) {
    const CellFeature& cf = grid->GetCell(grid->selX, grid->selY, grid->selZ);
    bool cellEmpty = cf.feature == Feature::EMPTY;
    bool inPreview = previewObjs;
    
    Vec3 identAxes[3] = {{1,0,0},{0,1,0},{0,0,1}};
    constexpr float h = CELL_SIZE * 0.5f - 0.001f;
    Vec3 sel = grid->CellCenterWorld(grid->selX, grid->selY, grid->selZ);
    glDisable(GL_DEPTH_TEST);
    AppendBox(r.buffer, sel, {h, h, h}, identAxes, {1.0f, 0.9f, 0.1f}, 0.25f);
    FlushBuffer(r, {0.8f, 0.8f, 0.8f}, 0.2f);
    glEnable(GL_DEPTH_TEST);
  }

  if (!colliders.empty()) {
    for (const auto &collider : colliders) {
      auto points = SampleSDFInside(collider, 0.02f);
      if (!points.empty())
	RenderDebugPoints(r, points, view, projection);
    }
  }

  glDepthMask(depthMaskSaved);
}

void LoadOBJ(const std::string &path, MeshData &out, float scale, bool flipWinding) {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string err;
  bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, path.c_str());
  if (!err.empty()) std::cerr << "TinyObjLoader: " << err << "\n";
  if (!ret) return;

  auto quantize = [](Vec3 p) {
  return std::make_tuple(
			 (int)std::round(p.x * 10000.0f),
			 (int)std::round(p.y * 10000.0f),
			 (int)std::round(p.z * 10000.0f));
  };

  std::vector<FaceData> faces;
  std::map<std::tuple<int,int,int>, Vec3> normalAccum;

  // Pass 1: read triangles, compute face normals, accumulate at each position
  for (size_t s = 0; s < shapes.size(); s++) {
    size_t index_offset = 0;
    for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
      size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);

      FaceData fd;
      for (size_t v = 0; v < fv; v++) {
	tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
	tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
	tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
	tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
	// Y/Z swap and scale
	fd.positions[v] = Vec3{vx * scale, vz * scale, vy * scale};
      }

      Vec3 e1 = fd.positions[1] - fd.positions[0];
      Vec3 e2 = fd.positions[2] - fd.positions[0];
      fd.faceNormal = Normalize(Cross(e1, e2));
      if (flipWinding) fd.faceNormal = fd.faceNormal * -1.0f;

      for (size_t v = 0; v < fv; v++) {
	auto key = quantize(fd.positions[v]);
	normalAccum[key] += fd.faceNormal;
      }

      faces.push_back(fd);
      index_offset += fv;
    }
  }

  // Normalize accumulated normals
  for (auto& kv : normalAccum) {
    kv.second = Normalize(kv.second);
  }

  // Pass 2: emit vertices with smoothed normals
  for (const FaceData& fd : faces) {
    for (size_t v = 0; v < 3; v++) {
      auto key = quantize(fd.positions[v]);
      Vec3 N = normalAccum[key];

      out.vertices.push_back(fd.positions[v].x);
      out.vertices.push_back(fd.positions[v].y);
      out.vertices.push_back(fd.positions[v].z);
      out.vertices.push_back(N.x);
      out.vertices.push_back(N.y);
      out.vertices.push_back(N.z);

      out.indices.push_back(out.indices.size());
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

void RenderDebugPoints(ObjectRenderer& r, const std::vector<Vec3>& points,
                       const Mat4& view, const Mat4& projection) {
  if (points.empty()) return;
  
  // Build padded buffer with dummy normal and orange color
  std::vector<float> buf;
  for (const Vec3& p : points) {
    buf.push_back(p.x); buf.push_back(p.y); buf.push_back(p.z);
    buf.push_back(0.0f); buf.push_back(1.0f); buf.push_back(0.0f);
    buf.push_back(1.0f); buf.push_back(0.5f); buf.push_back(0.0f); buf.push_back(1.0f);
  }
  
  unsigned int tempVAO, tempVBO;
  glGenVertexArrays(1, &tempVAO);
  glGenBuffers(1, &tempVBO);
  glBindVertexArray(tempVAO);
  glBindBuffer(GL_ARRAY_BUFFER, tempVBO);
  glBufferData(GL_ARRAY_BUFFER, buf.size() * sizeof(float), buf.data(), GL_DYNAMIC_DRAW);
  
  // Same attribute layout as FlushBuffer
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 40, (void*)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 40, (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 40, (void*)(6 * sizeof(float)));
  glEnableVertexAttribArray(2);
  
  glUseProgram(r.shader);
  glUniformMatrix4fv(glGetUniformLocation(r.shader, "uView"), 1, GL_FALSE, view.entries);
  glUniformMatrix4fv(glGetUniformLocation(r.shader, "uProjection"), 1, GL_FALSE, projection.entries);
  Mat4 identity = Mat4::Identity();
  glUniformMatrix4fv(glGetUniformLocation(r.shader, "uModel"), 1, GL_FALSE, identity.entries);
  glUniform4f(glGetUniformLocation(r.shader, "uColor"), 1.0f, 0.5f, 0.0f, 1.0f);
  
  glPointSize(5.0f);
  glDrawArrays(GL_POINTS, 0, points.size());
  
  glBindVertexArray(0);
  glDeleteVertexArrays(1, &tempVAO);
  glDeleteBuffers(1, &tempVBO);
}

