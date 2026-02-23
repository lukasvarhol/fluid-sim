#include "config.h"
#include "triangle_mesh.h"
#include "linear_algebra.h"
#include "particle.h"

unsigned int make_shader(const std::string& vertex_filepath, const std::string& fragment_filepath);
unsigned int make_module(const std::string& filepath, unsigned int module_type);
std::vector<float> rgba_normalizer(const int r, const int g, const int b, const int a);
void reset(unsigned int radius_px);
float randomFloat(float lowerbound);

static bool g_paused = false;
static bool g_step_one = false;
static bool g_reset = false;

static int g_fb_w = 720;
static int g_fb_h = 540;
const unsigned int NUM_PARTICLES = 2000;
std::vector<Particle> particles;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action != GLFW_PRESS) return;

    if (key == GLFW_KEY_SPACE) {
        g_paused = !g_paused;
    }
    if (key == GLFW_KEY_RIGHT) {
        g_step_one = true; 
    }
    if (key == GLFW_KEY_R) {
        g_reset = true;
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    g_fb_w = width;
    g_fb_h = height;
    glViewport(0, 0, width, height);
}

int main() {

    GLFWwindow* window;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    window = glfwCreateWindow(g_fb_w, g_fb_h, "Fluid Sim", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }

    std::vector<float> background_color = rgba_normalizer(0, 0, 0, 1);
    glClearColor(background_color[0], background_color[1], background_color[2], background_color[3]);

    TriangleMesh* triangle = new TriangleMesh();

    unsigned int shader = make_shader(
        "src/shaders/vertex.txt", 
        "src/shaders/fragment.txt"
    );
    glfwSetWindowUserPointer(window, (void*)(uintptr_t)shader);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glfwGetFramebufferSize(window, &g_fb_w, &g_fb_h);
    glViewport(0, 0, g_fb_w, g_fb_h);

    glUseProgram(shader);

    float xscale, yscale;
    glfwGetWindowContentScale(window, &xscale, &yscale);

    float radius_logical = 5.0f;
    float radius_px = radius_logical * xscale;

    reset(radius_px);

    double lastTime = glfwGetTime();

    unsigned int color_location = glGetUniformLocation(shader, "color");
    unsigned int model_location = glGetUniformLocation(shader, "model");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents(); 

        double now = glfwGetTime();
        float dt_measured = static_cast<float>(now - lastTime);

        lastTime = now;

        dt_measured = std::min(dt_measured, 1.0f / 30.0f);

        float dt_to_sim = 0.0f;

        if (g_reset) {
            reset(radius_px);
            g_reset = false;
            g_paused = false; 
            g_step_one = false; 
        }

        if (!g_paused) {
            dt_to_sim = dt_measured;
        } else if (g_step_one) {
            dt_to_sim = 1.0f / 60.0f;   
            g_step_one = false;
        } 

        if (dt_to_sim > 0.0f) {
            for (auto& p : particles)
                p.update(dt_to_sim, (float)g_fb_w, (float)g_fb_h);
        }

        float sx = (2.0f * radius_px) / (float)g_fb_w;  
        float sy = (2.0f * radius_px) / (float)g_fb_h;

        std::vector<float> quad_scaling = { sx, sy, 1.0f };

        glfwPollEvents();
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shader);
        for (const auto& p : particles) {

            mat4 model = mat4_multiply(
                create_matrix_transform(p.pos),
                create_matrix_scaling(quad_scaling)
            );

            glUniformMatrix4fv(model_location, 1, GL_FALSE, model.entries);

            glUniform4f(
                color_location,
                p.color[0],
                p.color[1],
                p.color[2],
                p.color[3]
            );

            triangle->draw();
        }

        glfwSwapBuffers(window);
    }
    

    glDeleteProgram(shader);
    delete triangle;
    glfwTerminate();
    return 0;
}

unsigned int make_shader(const std::string& vertex_filepath, const std::string& fragment_filepath) {

    std::vector<unsigned int> modules;
    modules.push_back(make_module(vertex_filepath, GL_VERTEX_SHADER));
    modules.push_back(make_module(fragment_filepath, GL_FRAGMENT_SHADER));

    unsigned int shader = glCreateProgram();
    for (unsigned int shaderModule : modules) {
        glAttachShader(shader, shaderModule);
    }
    glLinkProgram(shader);

    int success;
    glGetProgramiv(shader, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "Error linking shader program: " << infoLog << std::endl;
    }

    for (unsigned int shaderModule : modules) {
        glDeleteShader(shaderModule);
    }

    return shader;
}

unsigned int make_module(const std::string& filepath, unsigned int module_type) {
    std::ifstream file(filepath);          // <-- OPEN IT HERE
    if (!file.is_open()) {
        std::cerr << "Failed to open shader file: " << filepath << "\n";
        return 0;
    }

    std::stringstream ss;
    ss << file.rdbuf();
    std::string shaderSource = ss.str();
    file.close();

    unsigned int shaderModule = glCreateShader(module_type);
    const char* shaderSrc = shaderSource.c_str();
    glShaderSource(shaderModule, 1, &shaderSrc, NULL);
    glCompileShader(shaderModule);

    int success;
    glGetShaderiv(shaderModule, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shaderModule, 512, NULL, infoLog);
        std::cerr << "Error compiling shader: " << infoLog << std::endl;
    }

    return shaderModule;
}

std::vector<float> rgba_normalizer(const int r, const int g, const int b, const int a){
        return {
        r / 255.0f,
        g / 255.0f,
        b / 255.0f,
        (float) a
    };
}

float randomFloat(float lowerbound) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(lowerbound, 1.0f);

    return dist(gen);
}

void reset(unsigned int radius_px){
    particles.clear();
    for (int i = 0; i < NUM_PARTICLES; ++i){
        particles.push_back(Particle({randomFloat(-1.0f),randomFloat(0.0f), 0.0f},{randomFloat(-1.0f),randomFloat(0.0f), 0.0f},radius_px));
    }
}


