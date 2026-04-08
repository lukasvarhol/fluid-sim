#include "config.h"
#include "triangle_mesh.h"
#include "linear_algebra.h"
#include "particles.h"
#include "helpers.h"

unsigned int make_shader(const std::string &vertex_filepath, const std::string &fragment_filepath);
unsigned int make_module(const std::string &filepath, unsigned int module_type);
void reset(Particles& particles);

static bool g_paused = false;
static bool g_step_one = false;
static bool g_reset = false;
static bool g_push = false;
static bool g_pull = false;
Vec3 interaction_force {0.0f, 0.0f, 0.0f};

static int g_fb_w = 640;
static int g_fb_h = 480;
const unsigned int NUM_PARTICLES = 5000;
const float radius_logical = 2.0f;
const float smoothingRadius = 0.07f;


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
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS)   g_pull = true;
        if (action == GLFW_RELEASE) g_pull = false;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS)   g_push = true;
        if (action == GLFW_RELEASE) g_push = false;
    }
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    g_fb_w = width;
    g_fb_h = height;
    glViewport(0, 0, width, height);
}

int main()
{

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

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }

    std::array<float, 3> background_color = rgba_normalizer(0, 0, 0);
    glClearColor(background_color[0], background_color[1], background_color[2], 1.0f);

    Particles particles(NUM_PARTICLES, smoothingRadius);

    TriangleMesh* triangle = new TriangleMesh();

    unsigned int shader = make_shader(
        "src/shaders/vertex.glsl",
        "src/shaders/fragment.glsl");
    glfwSetWindowUserPointer(window, (void *)(uintptr_t)shader);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glfwGetFramebufferSize(window, &g_fb_w, &g_fb_h);
    glViewport(0, 0, g_fb_w, g_fb_h);

    glUseProgram(shader);

    float xscale, yscale;
    glfwGetWindowContentScale(window, &xscale, &yscale);

    float radius_px = radius_logical * xscale;
    reset(particles);

    triangle->setupInstanceBuffers(NUM_PARTICLES);

    double lastTime = glfwGetTime();

    unsigned int scale_location = glGetUniformLocation(shader, "scale");

    while (!glfwWindowShouldClose(window))
    {
        
        glfwPollEvents();
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

        // bool interact = false;
        // Vec3 cursor_pos{0.0f,0.0f,0.0f};
        // float interact_radius = 0.4f; 
        // float interact_strength = 0.0f;

        // if (g_push || g_pull) {
        //     interact = true;

        //     double xpos, ypos;
        //     glfwGetCursorPos(window, &xpos, &ypos);

        //     float x_ndc = (float)((xpos / g_fb_w) * 2.0 - 1.0);
        //     float y_ndc = (float)(1.0 - (ypos / g_fb_h) * 2.0);

        //     cursor_pos = { x_ndc, y_ndc, 0.0f };

        //     interact_strength = g_pull ? 30.0f : -30.0f;
        // }


        const int SUBSTEPS = 1;
        float sub_dt = dt_to_sim / SUBSTEPS; 
        if (dt_to_sim > 0) {
            for (int i = 0; i < SUBSTEPS; ++i) {
                particles.update(sub_dt, smoothingRadius, radius_px, g_fb_w, g_fb_h);
            }
        }

        // Build flat instance arrays
        std::vector<float> pos_data(NUM_PARTICLES * 2);
        std::vector<float> color_data(NUM_PARTICLES * 3);
        for (size_t i = 0; i < particles.positions.size(); ++i)
        {
           pos_data[2*i]     = particles.positions[i].x;
           pos_data[2*i + 1] = particles.positions[i].y;

           color_data[3*i]     = particles.colors[i].x;
           color_data[3*i + 1] = particles.colors[i].y;
           color_data[3*i + 2] = particles.colors[i].z;
        }

        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shader);

        float sx = (2.0f * radius_px) / (float)g_fb_w;
        float sy = (2.0f * radius_px) / (float)g_fb_h;
        glUniform2f(scale_location, sx, sy);

        triangle->updateInstanceData(pos_data, color_data);
        triangle->drawInstanced(particles.positions.size());

        glfwSwapBuffers(window);
    }
    glDeleteProgram(shader);
    delete triangle;
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

void reset(Particles& particles)
{
    particles.reset(smoothingRadius);
}
