#include "config.h"

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

int main() {
  Camera camera;
  InputState inputState;
  SimulationControl simulationControl;
  Viewport viewport;

  AppState appState;
  appState.camera = &camera;
  appState.inputState = &inputState;
  appState.simulationControl = &simulationControl;
  appState.viewport = &viewport;

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

  std::array<float, 3> backgroundColor = rgbaNormalizer(0, 0, 0);
  glClearColor(backgroundColor[0], backgroundColor[1], backgroundColor[2], 1.0f);

  Particles particles(numParticles, smoothingRadius);

  ParticleMesh particleMesh;
  unsigned int particleShader = MakeShader(
					   "src/shaders/vertex.glsl",
					   "src/shaders/fragment.glsl");
  Grid grid(50, 0.2f, -1.0f); 
  unsigned int gridShader = MakeShader(
				       "src/shaders/grid_vertex.glsl",
				       "src/shaders/grid_fragment.glsl");

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

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    DrawHUD(particles, simulationControl);    

    radiusPx = radiusLogical * xScale;

    double now = glfwGetTime();
    float dtMeasured = static_cast<float>(now - lastTime);
    lastTime = now;
    dtMeasured = std::min(dtMeasured, 1.0f / 60.0f);

    CameraState cameraState = ComputeViewMatrix(camera);

    float dtToSim = HandleSimulationControl(simulationControl, dtMeasured, particles);

    MouseRay mouseRay = MouseRaycast(inputState, cameraState, window);
    
    if (dtToSim > 0)
      {
      particles.Update(dtToSim, smoothingRadius, radiusPx, viewport.screenWidth,
                       viewport.screenHeight, mouseRay.origin,
                       mouseRay.direction, mouseRay.strength);
      }

    Render(cameraState, viewport, particles, particleMesh,
	   grid, particleShader, gridShader, radiusLogical, xScale);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }
  glDeleteProgram(particleShader);
  glDeleteProgram(gridShader);
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

