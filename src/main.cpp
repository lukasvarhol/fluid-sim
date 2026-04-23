#include "config.h"
#include "triangle_mesh.h"
#include "linear_algebra.h"
#include "particles.h"
#include "grid.h"
#include "helpers.h"

unsigned int make_shader(const std::string &vertex_filepath, const std::string &fragment_filepath);
unsigned int make_module(const std::string &filepath, unsigned int module_type);
void reset(Particles &particles);

static bool g_paused = true;
static bool g_step_one = false;
static bool g_reset = false;
static bool g_push = false;
static bool g_pull = false;
static bool show_hud = false;
static float g_panX = 0.0f;
static float g_panY = 0.0f;
static bool g_panning = false;
static double g_lastMouseX = 0.0;
static double g_lastMouseY = 0.0;

static int g_fb_w = 640;
static int g_fb_h = 480;

static bool g_orbiting = false;
static float g_azimuth   = 0.0f;    // horizontal angle
static float g_elevation = 0.3f;    // vertical angle, radians
static float g_radius = 3.0f;

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    g_radius -= (float)yoffset * 0.2f;
    g_radius = std::max(0.1f, g_radius); // don't go through the scene
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (action != GLFW_PRESS)
        return;

    if (key == GLFW_KEY_SPACE)
    {
        g_paused = !g_paused;
    }
    if (key == GLFW_KEY_RIGHT)
    {
        g_step_one = true;
    }
    if (key == GLFW_KEY_R)
    {
        g_reset = true;
    }
    if (key == GLFW_KEY_ESCAPE)
        show_hud = !show_hud;
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    if (ImGui::GetIO().WantCaptureMouse) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS)
        {
            if (mods & GLFW_MOD_CONTROL)
            {
                g_panning = true;
                glfwGetCursorPos(window, &g_lastMouseX, &g_lastMouseY);
            }
            else if (mods & GLFW_MOD_SHIFT)
            {
                g_orbiting = true;
                glfwGetCursorPos(window, &g_lastMouseX, &g_lastMouseY);
            }
            else
            {
                g_pull = true;
            }
        }
        if (action == GLFW_RELEASE)
        {
            g_pull    = false;
            g_panning  = false;
            g_orbiting = false;
        }
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (action == GLFW_PRESS)  g_push = true;
        if (action == GLFW_RELEASE) g_push = false;
    }
}

void cursor_pos_callback(GLFWwindow *window, double xpos, double ypos)
{
    double dx = xpos - g_lastMouseX;
    double dy = ypos - g_lastMouseY;
    g_lastMouseX = xpos;
    g_lastMouseY = ypos;

    if (g_orbiting)
    {
        g_azimuth   += (float)dx * 0.005f;
        g_elevation += (float)dy * 0.005f;
        g_elevation  = std::clamp(g_elevation, -PI/2.0f + 0.01f, PI/2.0f - 0.01f);
    }
    else if (g_panning)
      {
	// right and up vectors of the camera
	float sinA = sin(g_azimuth), cosA = cos(g_azimuth);
	float sinE = sin(g_elevation), cosE = cos(g_elevation);

	// camera right vector (perpendicular to forward, in XZ plane)
	Vec3 right = {cosA, 0.0f, -sinA};
	// camera up vector
	Vec3 up = {-sinA * sinE, cosE, -cosA * sinE};

	float speed = g_radius * 0.001f;
	g_panX -= ((float)dx * right.x - (float)dy * up.x) * speed;
	g_panY -= ((float)dx * right.y - (float)dy * up.y) * speed;
      }
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    g_fb_w = width;
    g_fb_h = height;
    glViewport(0, 0, width, height);
}

static void glfw_error_callback(int error, const char *description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main()
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    GLFWwindow *window;

    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwSwapInterval(0);

    window = glfwCreateWindow(g_fb_w, g_fb_h, "Fluid Sim", NULL, NULL);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

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

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }
    glEnable(GL_DEPTH_TEST);

    std::array<float, 3> background_color = rgba_normalizer(0, 0, 0);
    glClearColor(background_color[0], background_color[1], background_color[2], 1.0f);

    Particles particles(NUM_PARTICLES, smoothingRadius);

    TriangleMesh *triangle = new TriangleMesh();

    unsigned int shader = make_shader(
        "src/shaders/vertex.glsl",
        "src/shaders/fragment.glsl");
    glfwSetWindowUserPointer(window, (void *)(uintptr_t)shader);

    
    unsigned int grid_shader = make_shader("src/shaders/grid_vertex.glsl",
					   "src/shaders/grid_fragment.glsl");
    Grid *grid = new Grid(20, 0.1f, -1.0f); // 20 cells each side, 0.1 spacing

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glfwGetFramebufferSize(window, &g_fb_w, &g_fb_h);
    glViewport(0, 0, g_fb_w, g_fb_h);

    glUseProgram(shader);

    float xscale, yscale;
    glfwGetWindowContentScale(window, &xscale, &yscale);

    float radius_px;
    reset(particles);

    triangle->setupInstanceBuffers(30000); //TODO: refactor out

    double lastTime = glfwGetTime();

    unsigned int scale_location = glGetUniformLocation(shader, "scale");

    unsigned int min_val = 1000;
    unsigned int max_val = 10000;
    unsigned int my_val = 5000;

    while (!glfwWindowShouldClose(window))
    {

        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        if (show_hud)
        {
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
              if (g_paused) {
		bool changed = false;
		int nPending = particles.nParticles;
		changed |= ImGui::SliderInt("Particles", &nPending, 10, 30000);
		changed |= ImGui::SliderFloat("Spacing",   &INIT_SPACING,  0.01f, 0.2f);
		changed |= ImGui::SliderFloat("Offset X",  &INIT_OFFSET_X, -0.5f, 0.5f);
		changed |= ImGui::SliderFloat("Offset Y",  &INIT_OFFSET_Y, -0.5f, 0.5f);

		if (changed)
		  particles.resizeParticles(nPending, smoothingRadius,
                                  INIT_SPACING, INIT_OFFSET_X, INIT_OFFSET_Y);
	      } else {
                int nDisplay = particles.nParticles;
                ImGui::BeginDisabled();
                ImGui::SliderInt("Particles", &nDisplay, 1000, 30000);
		ImGui::SliderFloat("Pos X", &INIT_OFFSET_X, -1.0f, 1.0f);
                ImGui::SliderFloat("Pos Y", &INIT_OFFSET_Y, -1.0f, 1.0f);
	        ImGui::SliderFloat("Spacing", &INIT_SPACING, 0.001f, 0.1f);
                ImGui::EndDisabled();
              }
              
		  
              ImGui::SliderFloat("Particle Size", &radius_logical, 1.0f, 50.0f);
            }

            if (ImGui::CollapsingHeader("Physics")) {
	      ImGui::SliderFloat("Smoothing Radius", &smoothingRadius, 0.01f, 0.10f);
              ImGui::SliderFloat("Relaxation Factor", &RELAXATION_F, 1000.0f, 50000.0f);
              ImGui::SliderFloat("Gravity", &gravity, -10.0f, 10.0f);
              ImGui::SliderFloat("Scorr Coefficient", &k, 0.000001f, 0.00005f);
              ImGui::SliderFloat("Viscosity", &xsphC, 0.01f, 1.0f);
              ImGui::SliderFloat("Vorticity", &vorticityEpsilon, 0.0f, 20000.0f);
	      ImGui::SliderInt("Solvert Iterations", &NUM_ITERATIONS, 1, 20);
            }
            
            if (ImGui::CollapsingHeader("Mouse")) {
              ImGui::SliderFloat("Push Strength", &PUSH_STREN, -100.0f, 0.0f);
              ImGui::SliderFloat("Pull Strength", &PULL_STREN, 0.0, 100.0f);
              ImGui::SliderFloat("Push Radius", &PUSH_RAD, 0.0f, 1.0f);
              ImGui::SliderFloat("Pull Radius", &PULL_RAD, 0.0f, 1.0f); 
            }

            ImGui::End();
        }

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        radius_px = radius_logical * xscale;

        double now = glfwGetTime();
        float dt_measured = static_cast<float>(now - lastTime);
        lastTime = now;
        dt_measured = std::min(dt_measured, 1.0f / 60.0f);
        float dt_to_sim = 0.0f;

        if (g_reset)
        {
            reset(particles);
            g_reset = false;
            g_paused = false;
            g_step_one = false;
        }

        if (!g_paused)
        {
            dt_to_sim = dt_measured;
        }
        else if (g_step_one)
        {
            dt_to_sim = 1.0f / 60.0f;
            g_step_one = false;
        }

        Vec2 mousePos = {0.0f, 0.0f};
        float mouseStrength = 0.0f;

	float camX = g_panX + g_radius * cos(g_elevation) * sin(g_azimuth);
	float camY = g_panY + g_radius * sin(g_elevation);
	float camZ = g_radius * cos(g_elevation) * cos(g_azimuth);

        Mat4 view = lookAt(Vec3{camX, camY, camZ}, Vec3{g_panX, g_panY, 0.0f},
                           Vec3{0, 1, 0});

	if ((g_push || g_pull) && !ImGui::GetIO().WantCaptureMouse)
	{
	  double xpos, ypos;
	  glfwGetCursorPos(window, &xpos, &ypos);
	  int winW, winH;
	  glfwGetWindowSize(window, &winW, &winH);

	  float x_ndc = (float)((xpos / winW) * 2.0 - 1.0);
	  float y_ndc = (float)(1.0 - (ypos / winH) * 2.0);

	  float fov = 45.0f * PI / 180.0f;
	  float aspect = (float)winW / winH;
	  float tan_half_fov = tan(fov / 2.0f);

	  Vec3 ray_view = {
	    x_ndc * aspect * tan_half_fov,
	    y_ndc * tan_half_fov,
	    -1.0f
	  };
	    // transform ray to world space using inverse view
	  Mat4 invView = inverse_view(view);

	  Vec3 ray_world = {
	    invView.entries[0]*ray_view.x + invView.entries[4]*ray_view.y + invView.entries[8]*ray_view.z,
	    invView.entries[1]*ray_view.x + invView.entries[5]*ray_view.y + invView.entries[9]*ray_view.z,
	    invView.entries[2]*ray_view.x + invView.entries[6]*ray_view.y + invView.entries[10]*ray_view.z,
	  };

	  Vec3 ray_origin = {camX, camY, camZ};

	  float t = -ray_origin.z / ray_world.z;
	  float world_x = ray_origin.x + t * ray_world.x;
	  float world_y = ray_origin.y + t * ray_world.y;

	  mousePos = {world_x, world_y};
	  mouseStrength = g_pull ? PULL_STREN : PUSH_STREN;
	}

        if (dt_to_sim > 0)
        {
          particles.update(dt_to_sim, smoothingRadius, radius_px, g_fb_w,
                           g_fb_h, mousePos, mouseStrength);
        }

        // Build flat instance arrays
	int n = particles.nParticles;
        std::vector<float> pos_data(n * 3);
        std::vector<float> color_data(n * 3);
	std::vector<float> angle_data(n);
        for (size_t i = 0; i < n; ++i)
        {
            pos_data[3 * i] = particles.positions[i].x;
            pos_data[3 * i + 1] = particles.positions[i].y;
	    pos_data[3 * i + 2] = 0.0f; // use for z

            color_data[3 * i] = particles.colors[i].x;
            color_data[3 * i + 1] = particles.colors[i].y;
            color_data[3 * i + 2] = particles.colors[i].z;

        }

        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shader);

        float sx = (2.0f * radius_px) / (float)g_fb_w;
        float sy = (2.0f * radius_px) / (float)g_fb_h;

        Mat4 proj = perspective(45.0f * PI / 180.0f, (float)g_fb_w / g_fb_h,
                                0.1f, 100.0f);

	glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, proj.entries);
	glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, view.entries);

	float fov = 45.0f * PI / 180.0f;
        float view_radius = (radius_logical / g_fb_h) * 2.0f * g_radius * tan(fov / 2.0f);
	glUniform1f(glGetUniformLocation(shader, "radius"), view_radius);

	glUniform3f(glGetUniformLocation(shader, "lightDir"), 0.6f, 0.8f, 1.0f);

        triangle->updateInstanceData(pos_data, color_data);
        triangle->drawInstanced((int)particles.positions.size());
	grid->draw(grid_shader, proj.entries, view.entries);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
    glDeleteProgram(shader);
    delete triangle;
    glDeleteProgram(grid_shader);
    delete grid;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}

unsigned int make_shader(const std::string &vertex_filepath, const std::string &fragment_filepath)
{

    std::vector<unsigned int> modules;
    modules.push_back(make_module(vertex_filepath, GL_VERTEX_SHADER));
    modules.push_back(make_module(fragment_filepath, GL_FRAGMENT_SHADER));

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

unsigned int make_module(const std::string &filepath, unsigned int module_type)
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

    unsigned int shaderModule = glCreateShader(module_type);
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

void reset(Particles &particles)
{
    particles.reset(smoothingRadius);
}
