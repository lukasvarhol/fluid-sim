#include "config.h"
#include <string>
#include <limits>
#include "tiny_obj_loader.h"

bool isBenchmarking = false;
bool runParallel = true;
int currentFrame = 0;

bool useTriangleCollisions = false;
std::vector<TriCollider> gTriColliders;
std::vector<Vec3> gClosestPoints;

unsigned int MakeShader(const std::string &vertexFilepath, const std::string &fragmentFilepath);
unsigned int MakeModule(const std::string &filepath, unsigned int moduleType);

void FramebufferSizeCallback(GLFWwindow *window, int width, int height) {
  AppState* appState = static_cast<AppState*>(glfwGetWindowUserPointer(window));
  appState->viewport->screenWidth = width;
  appState->viewport->screenHeight = height;
  glViewport(0, 0, width, height);
}

static void glfwErrorCallback(int error, const char *description)
{
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// use only for benchmarks
std::vector<Vec3> LoadOBJTriangles(const std::string &path) {
  tinyobj::attrib_t attrib;
  float scale = 0.001f;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string err;
  std::vector<Vec3> out;
  bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, path.c_str());
  if (!err.empty()) std::cerr << "TinyObjLoader: " << err << "\n";
  if (!ret)
    return {};

  for (size_t s = 0; s < shapes.size(); s++) {
    size_t index_offset = 0;
    for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
      size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);

      for (size_t v = 0; v < fv; v++) {
	tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
	tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
	tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
	tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
        // Y/Z swap and scale
        out.push_back(Vec3{vx * scale, vz * scale, vy * scale});
      }
      index_offset += fv;
    }
  }
  return  out;
}

int main(int argc, char *argv[]) {
  if (argc >= 2) {
    if (argc != 16 ) printf("Incorrect usage: ./fluid-sim --benchmark -c xyz123 -b cpu -p 10000 -f 2000 -sdf 10 -collision sdf -r 3\n");
    else {
      isBenchmarking = true;
      std::string commit = "";
      std::string backend = "";
      std::string collisionType = "";
      int profilerParticles = -1;
      int profilerFrames = -1;
      int profilerColliders = -1;
      int profilerRuns = -1;
      for (int i = 2; i < argc; i+=2) {
        if (std::strcmp(argv[i], "-c") == 0)
          commit = argv[i + 1];
        if (std::strcmp(argv[i], "-b") == 0)
          backend = argv[i + 1];
        if (std::strcmp(argv[i], "-p") == 0)
          profilerParticles = std::stoi(argv[i + 1]);
        if (std::strcmp(argv[i], "-f") == 0)
          profilerFrames = std::stoi(argv[i + 1]);
        if (std::strcmp(argv[i], "-sdf") == 0)
          profilerColliders = std::stoi(argv[i + 1]);
	if (std::strcmp(argv[i], "-collision") == 0)
          collisionType = argv[i + 1];
	if (std::strcmp(argv[i], "-r") == 0)
          profilerRuns = std::stoi(argv[i + 1]);
      }
      if (commit.empty() || backend.empty() || profilerParticles == -1 ||
          profilerFrames == -1 || profilerColliders == -1) {
        printf("Missing required arguments\n");
	return -1;
      }
      if (backend == "cpu-sequential")
        runParallel = false;

      if (collisionType == "tri")
	useTriangleCollisions = true;
      std::cout << runParallel << std::endl;
      std::string filepath = "benchmark/logs/" + commit + "-" + backend + "-" + collisionType +  "-" +
	std::to_string(profilerParticles) + "-" + std::to_string(profilerFrames) + "-" + std::to_string(profilerColliders) + "-" + std::to_string(profilerRuns) + ".csv";
      Profiler::Init(filepath, profilerParticles, profilerColliders, profilerFrames, backend,
                     commit);
      
      std::cout << "filepath: " << filepath << std::endl;
      Particles particles(profilerParticles, smoothingRadius);

      std::vector<SDFCollider> colliders;
      GridState grid;

      std::vector<Vec3> triangles = LoadOBJTriangles("meshes/SChannel.obj");
      
      int count = 0;
      for (int y = 4; y >= 0 && count < profilerColliders; --y) {
	for (int z = 1; z <= 3 && count < profilerColliders; ++z) {
	  for (int x = 1; x <= 3 && count < profilerColliders; ++x) {
	    if (!useTriangleCollisions) {
	      SDFCollider c;
	      c.type = RGObjectType::S_CHANNEL;
	      c.worldPosition = grid.CellCenterWorld(x, y, z);
	      c.rotationAxes = { Vec3{1,0,0}, Vec3{0,1,0}, Vec3{0,0,1} };
	      c.restitution = energyRetention;
	      colliders.push_back(c);
	      ++count;
	    } else  {
              TriCollider tc;
              Vec3 cellCenter = grid.CellCenterWorld(x, y, z);
              for (Vec3 v : triangles) {
                tc.triangles.push_back(v + cellCenter);
              }
              tc.restitution = energyRetention;
              gTriColliders.push_back(tc);
	      ++count;
            }
          }
        }
      }
      gClosestPoints.resize(profilerParticles); 
      	    
      for (int i = 0; i < profilerFrames; ++i) {
        particles.Update(1.0f / 60.0f, smoothingRadius, 2.0f, 640, 480,
                         Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 0.0f, 0.0f}, 0.0f, colliders);
	currentFrame++;
      }

      Profiler::Write();
      return 0;
    }
  }
  Camera camera;
  InputState inputState;
  SimulationControl simulationControl;
  Viewport viewport;
  EditorState editorState;

  AppState appState;
  appState.camera            = &camera;
  appState.inputState        = &inputState;
  appState.simulationControl = &simulationControl;
  appState.viewport          = &viewport;
  appState.editorState       = &editorState;

  if(isBenchmarking) appState.simulationControl->isPaused = false;

  glfwSetErrorCallback(glfwErrorCallback);

  if (!glfwInit())
    {
      std::cerr << "Failed to initialize GLFW" << std::endl;
      return -1;
    }

  GLFWwindow *window;

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwSwapInterval(0);

  window = glfwCreateWindow(viewport.screenWidth, viewport.screenHeight, "Fluid Sim", NULL, NULL);
  if (!window)
    {
      std::cerr << "Failed to create GLFW window\n";
      glfwTerminate();
      return -1;
    }
  glfwMakeContextCurrent(window);

  glfwSetWindowUserPointer(window, &appState);

  // ImGui_ImplGlfw_InitForOpenGL(window, false); // don't auto-install

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
  // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForOpenGL(window, false); // Second param install_callback=true will install GLFW callbacks and chain to existing ones.
  ImGui_ImplOpenGL3_Init();

  glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
  glfwSetKeyCallback(window, KeyCallback);
  glfwSetMouseButtonCallback(window, MouseButtonCallback);
  glfwSetScrollCallback(window, ScrollCallback);
  glfwSetCursorPosCallback(window, CursorPosCallback);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
      std::cerr << "Failed to initialize GLAD" << std::endl;
      glfwTerminate();
      return -1;
    }
  glEnable(GL_DEPTH_TEST);

  std::array<float, 3> backgroundColor = rgbaNormalizer(40, 40, 40);
  glClearColor(backgroundColor[0], backgroundColor[1], backgroundColor[2], 1.0f);

  Particles particles(numParticles, smoothingRadius);

  ObjectRenderer objectRenderer;
  SetupObjectRenderer(objectRenderer);

  ParticleMesh particleMesh;
  unsigned int particleShader = MakeShader("src/shaders/vertex.glsl",
					   "src/shaders/fragment.glsl");

  std::vector<SceneObject> sceneObjects;

  LineRenderer gridRenderer(BuildGridLines(50, 0.2f, -1.0f));
  LineRenderer boundingBoxRenderer(BuildBoundingBox());
  LineRenderer cellGridRenderer(BuildCellGridLines());

  unsigned int wireframeShader = MakeShader("src/shaders/wireframe_vertex.glsl",
                                            "src/shaders/wireframe_fragment.glsl");

  SceneObject grid;
  grid.shader    = wireframeShader;
  grid.renderer  = &gridRenderer;
  sceneObjects.push_back(grid);

  SceneObject boundingBox;
  boundingBox.shader   = wireframeShader;
  boundingBox.renderer = &boundingBoxRenderer;
  sceneObjects.push_back(boundingBox);

  SceneObject cellGridObj;
  cellGridObj.shader   = wireframeShader;
  cellGridObj.renderer = &cellGridRenderer;
  sceneObjects.push_back(cellGridObj);
  SceneObject& cellGridRef = sceneObjects.back(); // mutable ref for visibility toggle
			      
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glfwGetFramebufferSize(window, &viewport.screenWidth, &viewport.screenHeight);
  glViewport(0, 0, viewport.screenWidth, viewport.screenHeight);

  float xScale, yScale;
  glfwGetWindowContentScale(window, &xScale, &yScale);

  float radiusPx;
  particles.Reset(smoothingRadius);

  particleMesh.SetupInstanceBuffers(60000); //TODO: refactor out

  double lastTime = glfwGetTime();

  float dtMeasured = 0.0f;

  std::vector<SDFCollider> colliders;

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    double now = glfwGetTime();
    dtMeasured = static_cast<float>(now - lastTime);
    lastTime = now;
    dtMeasured = std::min(dtMeasured, 1.0f / 60.0f);

    DrawHUD(particles, simulationControl, editorState, dtMeasured);

    radiusPx = radiusLogical * xScale;

    CameraState cameraState = ComputeViewMatrix(camera);


    bool wasReset = simulationControl.isReset;
    float dtToSim = HandleSimulationControl(simulationControl, dtMeasured, particles);
    if (wasReset) {
      if (editorState.resetObjectsOnR)
        LoadDefaultScene(editorState);
    }
    // std::vector<SDFCollider> testColliders;
    // BuildSDFColliders(editorState.objects, testColliders);
    // for (const auto& c : testColliders) {
    //   auto pts = SampleSDFInside(c, 0.02f);
    //   printf("collider type %d: %zu inside points\n", (int)c.type, pts.size());
    // }

    MouseRay mouseRay = MouseRaycast(inputState, cameraState, window);

    std::vector<Vec3> sdfPoints;
    if (dtToSim > 0) {
      BuildSDFColliders(editorState.objects, colliders);
      particles.Update(dtToSim, smoothingRadius, radiusPx, viewport.screenWidth,
                       viewport.screenHeight, mouseRay.origin,
                       mouseRay.direction, mouseRay.strength, colliders);

    }

    cellGridRef.visible = editorState.showGrid;

    Render(cameraState, viewport, particles, particleMesh,
	   particleShader, sceneObjects, radiusLogical, xScale);

    // Render solid RG objects, optional ghost preview, and cell overlays
    {
      float aspect = (float)viewport.screenWidth / (float)viewport.screenHeight;
      Mat4 proj = Perspective(45.0f * PI / 180.0f, aspect, 0.1f, 100.0f);
      RenderObjects(objectRenderer, editorState.objects,
                    editorState.previewActive ? &editorState.previewObjects
                                              : nullptr,
                    &editorState.grid, editorState.showSelectedCell,
                    editorState.showOccupiedOutlines, cameraState.view, proj,
                    cameraState.position,std::vector<SDFCollider>{});
      //std::vector<SDFCollider>{}
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }
  glDeleteProgram(particleShader);
  DestroyObjectRenderer(objectRenderer);
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwTerminate();
  return 0;
}

unsigned int MakeShader(const std::string &vertexFilepath, const std::string &fragmentFilepath)
{
  std::vector<unsigned int> modules;
  modules.push_back(MakeModule(vertexFilepath, GL_VERTEX_SHADER));
  modules.push_back(MakeModule(fragmentFilepath, GL_FRAGMENT_SHADER));

  unsigned int shader = glCreateProgram();
  for (unsigned int shaderModule : modules)
    {
      glAttachShader(shader, shaderModule);
    }
  glLinkProgram(shader);

  int success;
  glGetProgramiv(shader, GL_LINK_STATUS, &success);
  if (!success)
    {
      char infoLog[512];
      glGetProgramInfoLog(shader, 512, NULL, infoLog);
      std::cerr << "Error linking shader program: " << infoLog << std::endl;
    }

  for (unsigned int shaderModule : modules)
    {
      glDeleteShader(shaderModule);
    }

  return shader;
}

unsigned int MakeModule(const std::string &filepath, unsigned int moduleType)
{
  std::ifstream file(filepath);
  if (!file.is_open())
    {
      std::cerr << "Failed to open shader file: " << filepath << "\n";
      return 0;
    }

  std::stringstream ss;
  ss << file.rdbuf();
  std::string shaderSource = ss.str();
  file.close();

  unsigned int shaderModule = glCreateShader(moduleType);
  const char *shaderSrc = shaderSource.c_str();
  glShaderSource(shaderModule, 1, &shaderSrc, NULL);
  glCompileShader(shaderModule);

  int success;
  glGetShaderiv(shaderModule, GL_COMPILE_STATUS, &success);
  if (!success)
    {
      char infoLog[512];
      glGetShaderInfoLog(shaderModule, 512, NULL, infoLog);
      std::cerr << "Error compiling shader: " << infoLog << std::endl;
    }

  return shaderModule;
}

