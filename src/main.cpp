#include "config.h"
#include "triangle_mesh.h"
#include "linear_algebra.h"
#include "particles.h"
#include "grid.h"
#include "app/app_state.h"

#include "helpers.h"

unsigned int MakeShader(const std::string &vertexFilepath, const std::string &fragmentFilepath);
unsigned int MakeModule(const std::string &filepath, unsigned int moduleType);
void Reset(Particles &particles);

void ScrollCallback(GLFWwindow *window, double xOffset, double yOffset) {
  AppState *appState =
      static_cast<AppState *>(glfwGetWindowUserPointer(window));
  
  appState->camera->radius -= (float)yOffset * 0.2f;
  appState->camera->radius = std::max(0.001f, appState->camera->radius); // don't go through the scene
}

void KeyCallback(GLFWwindow *window, int key, int scancode, int action,
                 int mods) {
  AppState *appState = static_cast<AppState*>(glfwGetWindowUserPointer(window));
					       
  if (action != GLFW_PRESS)
    return;

  if (key == GLFW_KEY_SPACE)
    {
      appState->simulationControl->isPaused = !appState->simulationControl->isPaused;
    }
  if (key == GLFW_KEY_RIGHT)
    {
      appState->simulationControl->isStepping = true;
    }
  if (key == GLFW_KEY_R)
    {
      appState->simulationControl->isReset = true;
    }
  if (key == GLFW_KEY_ESCAPE)
    appState->simulationControl->isShowHUD = !appState->simulationControl->isShowHUD;
}

void MouseButtonCallback(GLFWwindow *window, int button, int action, int mods) {
  AppState *appState =
      static_cast<AppState *>(glfwGetWindowUserPointer(window));
  
  ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
  if (ImGui::GetIO().WantCaptureMouse) return;

  if (button == GLFW_MOUSE_BUTTON_LEFT){
    if (action == GLFW_PRESS)	{
      if (mods & GLFW_MOD_CONTROL) {
	appState->inputState->isPanning = true;   
	glfwGetCursorPos(window, &appState->inputState->lastMouseX, &appState->inputState->lastMouseY);
      }
      else if (mods & GLFW_MOD_SHIFT) {
	appState->inputState->isOrbiting = true;
	glfwGetCursorPos(window,  &appState->inputState->lastMouseX, &appState->inputState->lastMouseY);
      }
      else {
	appState->inputState->isPulling = true;
      }
    }
    if (action == GLFW_RELEASE) {
      appState->inputState->isPulling  = false;
      appState->inputState->isPanning  = false;
      appState->inputState->isOrbiting = false;
    }
  }
  if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
      if (action == GLFW_PRESS) appState->inputState->isPushing = true;
      if (action == GLFW_RELEASE) appState->inputState->isPushing = false;
    }
}

void CursorPosCallback(GLFWwindow *window, double xpos, double ypos) {
  AppState *appState =
      static_cast<AppState *>(glfwGetWindowUserPointer(window));
  
  double dx = xpos - appState->inputState->lastMouseX;
  double dy = ypos -  appState->inputState->lastMouseY;
  appState->inputState->lastMouseX = xpos;
  appState->inputState->lastMouseY = ypos;

  if (appState->inputState->isOrbiting)
    {
      appState->camera->azimuth   -= (float)dx * 0.005f;
      appState->camera->elevation += (float)dy * 0.005f;
      appState->camera->elevation  = std::clamp(appState->camera->elevation, -PI/2.0f + 0.01f, PI/2.0f - 0.01f);
    }
  else if (appState->inputState->isPanning)
    {
      // right and up vectors of the camera
      float sinA = sin(appState->camera->azimuth), cosA = cos(appState->camera->azimuth);
      float sinE = sin(appState->camera->elevation), cosE = cos(appState->camera->elevation);

      // camera right vector (perpendicular to forward, in XZ plane)
      Vec3 right = {cosA, 0.0f, -sinA};
      // camera up vector
      Vec3 up = {-sinA * sinE, cosE, -cosA * sinE};

      float speed = appState->camera->radius * 0.001f;
      appState->camera->panX -= ((float)dx * right.x - (float)dy * up.x) * speed;
      appState->camera->panY -= ((float)dx * right.y - (float)dy * up.y) * speed;
    }
}

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

  TriangleMesh *triangle = new TriangleMesh();

  unsigned int shader = MakeShader(
				   "src/shaders/vertex.glsl",
				   "src/shaders/fragment.glsl");

  unsigned int gridShader = MakeShader(
					"src/shaders/grid_vertex.glsl",
					"src/shaders/grid_fragment.glsl");
  Grid *grid = new Grid(50, 0.2f, -1.0f); 

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glfwGetFramebufferSize(window, &viewport.screenWidth, &viewport.screenHeight);
  glViewport(0, 0, viewport.screenWidth, viewport.screenHeight);

  glUseProgram(shader);

  float xScale, yScale;
  glfwGetWindowContentScale(window, &xScale, &yScale);

  float radiusPx;
  Reset(particles);

  triangle->setupInstanceBuffers(60000); //TODO: refactor out

  double lastTime = glfwGetTime();

  unsigned int scaleLocation = glGetUniformLocation(shader, "scale");

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    if (simulationControl.isShowHUD) {
      ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Once);
      ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);
      ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
	ImGuiWindowFlags_NoResize |
	ImGuiWindowFlags_NoScrollbar |
	ImGuiWindowFlags_NoBackground |
	ImGuiWindowFlags_AlwaysAutoResize;
      ImGui::Begin("##hud", nullptr, flags);

      ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

      if (ImGui::CollapsingHeader("Appearance")) {
        if (simulationControl.isPaused) {
	  bool changed = false;
	  int nPending = particles.numParticles;
	  changed |= ImGui::SliderInt("Particles", &nPending, 10, 60000);
	  changed |= ImGui::SliderFloat("Spacing", &initSpacing,  0.01f, 0.2f);
	  changed |= ImGui::SliderFloat("Offset X", &initOffsetX, -0.5f, 0.5f);
	  changed |= ImGui::SliderFloat("Offset Y", &initOffsetY, -0.5f, 0.5f);
	  changed |= ImGui::SliderFloat("Offset Z", &initOffsetZ, -0.5f, 0.5f);

	  if (changed)
	    particles.ResizeParticles(nPending, smoothingRadius, 
				      initSpacing, initOffsetX, initOffsetY, initOffsetZ);
	} else {
	  int nDisplay = particles.numParticles;
	  ImGui::BeginDisabled();
	  ImGui::SliderInt("Particles", &nDisplay, 1000, 60000);
	  ImGui::SliderFloat("Pos X", &initOffsetX, -1.0f, 1.0f);
	  ImGui::SliderFloat("Pos Y", &initOffsetY, -1.0f, 1.0f);
	  ImGui::SliderFloat("Spacing", &initSpacing, 0.001f, 0.1f);
	  ImGui::EndDisabled();
	}	  
	ImGui::SliderFloat("Particle Size", &radiusLogical, 1.0f, 50.0f);
      }

      if (ImGui::CollapsingHeader("Physics")) {
	ImGui::SliderFloat("Smoothing Radius", &smoothingRadius, 0.01f, 1.0f);
	ImGui::SliderFloat("Relaxation Factor", &relaxation, 1000.0f, 50000.0f);
	ImGui::SliderFloat("Gravity", &gravity, -10.0f, 10.0f);
	ImGui::SliderFloat("Scorr Coefficient", &scorrCoefficient, 0.000001f, 0.00005f);
	ImGui::SliderFloat("Viscosity", &xsphC, 0.01f, 1.0f);
	ImGui::SliderFloat("Vorticity", &vorticityEpsilon, 0.0f, 20000.0f);
	ImGui::SliderInt("Solvert Iterations", &numIterations, 1, 20);
      }
            
      if (ImGui::CollapsingHeader("Mouse")) {
	ImGui::SliderFloat("Push Strength", &pushStrength, -100.0f, 0.0f);
	ImGui::SliderFloat("Pull Strength", &pullStrength, 0.0, 100.0f);
	ImGui::SliderFloat("Push Radius", &pushRadius, 0.0f, 1.0f);
	ImGui::SliderFloat("Pull Radius", &pullRadius, 0.0f, 1.0f); 
      }
      ImGui::End();
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    radiusPx = radiusLogical * xScale;

    double now = glfwGetTime();
    float dtMeasured = static_cast<float>(now - lastTime);
    lastTime = now;
    dtMeasured = std::min(dtMeasured, 1.0f / 60.0f);
    float dtToSim = 0.0f;

    if (simulationControl.isReset) {
      Reset(particles);
      simulationControl.isReset = false;
      simulationControl.isPaused = false;
      simulationControl.isStepping = false;
    }

    if (!simulationControl.isPaused) {
      dtToSim = dtMeasured;
    } else if (simulationControl.isStepping) {
      dtToSim = 1.0f / 60.0f;
      simulationControl.isStepping = false;
    }

    Vec3 mouseRayOrigin = {0.0f, 0.0f, 0.0f};
    Vec3 mouseRayDir = {0.0f, 0.0f, 0.0f};
    float mouseStrength = 0.0f;

    float camX = camera.panX + camera.radius * cos(camera.elevation) * sin(camera.azimuth);
    float camY = camera.panY + camera.radius * sin(camera.elevation);
    float camZ = camera.radius * cos(camera.elevation) * cos(camera.azimuth);

    Mat4 view = LookAt(Vec3{camX, camY, camZ}, Vec3{camera.panX, camera.panY, 0.0f},
		       Vec3{0, 1.0, 0});

    if ((inputState.isPushing || inputState.isPulling) && !ImGui::GetIO().WantCaptureMouse) {
      double xPos, yPos;
      glfwGetCursorPos(window, &xPos, &yPos);
      int winW, winH;
      glfwGetWindowSize(window, &winW, &winH);

      float xNDC = (float)((xPos / winW) * 2.0 - 1.0);
      float yNDC = (float)(1.0 - (yPos / winH) * 2.0);

      float fov = 45.0f * PI / 180.0f;
      float aspect = (float)winW / winH;
      float tanHalfFOV = tan(fov / 2.0f);

      Vec3 rayView = {
	xNDC * aspect * tanHalfFOV,
	yNDC * tanHalfFOV,
	-1.0f
      };
      // transform ray to world space using inverse view
      Mat4 invView = InverseView(view);

      Vec3 rayWorld = {
	invView.entries[0] * rayView.x +
	invView.entries[4] * rayView.y +
	invView.entries[8] * rayView.z,
	invView.entries[1] * rayView.x +
	invView.entries[5] * rayView.y +
	invView.entries[9] * rayView.z,
	invView.entries[2] * rayView.x +
	invView.entries[6] * rayView.y +
	invView.entries[10] * rayView.z,
      };
      float len = rayWorld.Magnitude();
      rayWorld = rayWorld * (1.0f / len);

      Vec3 rayOrigin = {camX, camY, camZ};

      float t = -rayOrigin.z / rayWorld.z;
      float worldX = rayOrigin.x + t * rayWorld.x;
      float worldY = rayOrigin.y + t * rayWorld.y;

      mouseRayOrigin = {camX, camY, camZ};
      mouseRayDir = rayWorld;
      mouseStrength = inputState.isPulling ? pullStrength : pushStrength;
    }

    if (dtToSim > 0)
      {
	particles.Update(dtToSim, smoothingRadius, radiusPx, viewport.screenWidth,
			 viewport.screenHeight, mouseRayOrigin, mouseRayDir, mouseStrength);
      }

    // Build flat instance arrays
    int n = particles.numParticles;
    std::vector<float> posData(n * 3);
    std::vector<float> velData(n * 3);
    for (size_t i = 0; i < n; ++i)
      {
	posData[3 * i]     = particles.positions[i].x;
	posData[3 * i + 1] = particles.positions[i].y;
	posData[3 * i + 2] = particles.positions[i].z;
       
	velData[3 * i]     = particles.velocities[i].x;
	velData[3 * i + 1] = particles.velocities[i].y;
	velData[3 * i + 2] = particles.velocities[i].z;
      }
   
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(shader);
   
    float sx = (2.0f * radiusPx) / (float)viewport.screenWidth;
    float sy = (2.0f * radiusPx) / (float)viewport.screenHeight;
   
    Mat4 proj = Perspective(45.0f * PI / 180.0f, (float)viewport.screenWidth / viewport.screenHeight,
			    0.1f, 100.0f);
   
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, proj.entries);
    glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, view.entries);
   
    float fov = 45.0f * PI / 180.0f;
    float viewRadius = (radiusLogical / viewport.screenHeight) * 2.0f * tan(fov / 2.0f);
    glUniform1f(glGetUniformLocation(shader, "radius"), viewRadius);
   
    glUniform3f(glGetUniformLocation(shader, "lightDir"), 0.6f, 0.8f, 1.0f);

    triangle->updateInstanceData(posData, velData);
    triangle->drawInstanced((int)particles.positions.size());
    grid->draw(gridShader, proj.entries, view.entries);
   
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }
  glDeleteProgram(shader);
  delete triangle;
  glDeleteProgram(gridShader);
  delete grid;
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

void Reset(Particles &particles)
{
  particles.Reset(smoothingRadius);
}
