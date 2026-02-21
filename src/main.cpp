#include "config.h"
#include "triangle_mesh.h"
#include "linear_algebra.h"
#include "colors.h"

unsigned int make_shader(const std::string& vertex_filepath, const std::string& fragment_filepath);
unsigned int make_module(const std::string& filepath, unsigned int module_type);
std::vector<float> rgba_normalizer(const int r, const int g, const int b, const int a);
vec4 lerp(const vec4&a, const vec4&b, float s);
vec4 getColor(const vec3 velocity);

static int g_fb_w = 640;
static int g_fb_h = 480;

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

    window = glfwCreateWindow(640, 480, "Fluid Sim", NULL, NULL);
    glfwMakeContextCurrent(window);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);


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

    float radius_px = 15.0f;
    vec3 quad_position = {0.0f, 0.5f, 0.0f};
  
    vec3 quad_velocity = {1.6f, 0.8f, 0.0f};
    vec3 gravity = {0.0f, -3.5f, 0.0f};       
    float energy_loss = 0.85f;

    vec4 object_color = {1.0f, 1.0f, 1.0f, 1.0f};

    double lastTime = glfwGetTime();

    unsigned int color_location = glGetUniformLocation(shader, "color");
    unsigned int model_location = glGetUniformLocation(shader, "model");

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;

        quad_velocity.entries[0] += gravity.entries[0] * dt;
        quad_velocity.entries[1] += gravity.entries[1] * dt;
        quad_velocity.entries[2] += gravity.entries[2] * dt;

        quad_position.entries[0] += quad_velocity.entries[0] * dt;
        quad_position.entries[1] += quad_velocity.entries[1] * dt;
        quad_position.entries[2] += quad_velocity.entries[2] * dt;

        object_color = getColor(quad_velocity);

        float sx = (2.0f * radius_px) / (float)g_fb_w;  // diameter in NDC
        float sy = (2.0f * radius_px) / (float)g_fb_h;

        vec3 quad_scaling = { sx, sy, 1.0f };

        float radius_ndc_y = (radius_px) / (float)g_fb_h; 
        float radius_ndc_x = (radius_px) / (float)g_fb_w;
        float halfsize_x = radius_ndc_x;
        float halfsize_y = radius_ndc_y; 
        float floorY = -1.0f + radius_ndc_y;
        float ceilY = 1.0f - radius_ndc_y;
        float leftX = -1.0f + radius_ndc_x;
        float rightX = 1.0f - radius_ndc_x;

        if (quad_position.entries[1] <= floorY) {
            quad_position.entries[1] = floorY;
            quad_velocity.entries[1] *= -1.0f * energy_loss;
        }

        if (quad_position.entries[1] >= ceilY) {
            quad_position.entries[1] = ceilY;
            quad_velocity.entries[1] *= -1.0f * energy_loss;
        }

        if (quad_position.entries[0] <= leftX) {
            quad_position.entries[0] = leftX;
            quad_velocity.entries[0] *= -1.0f * energy_loss;
        }

        if (quad_position.entries[0] >= rightX) {
            quad_position.entries[0] = rightX;
            quad_velocity.entries[0] *= -1.0f * energy_loss;
        }

        glfwPollEvents();
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shader);
        mat4 model = mat4_multiply(
            create_matrix_transform(quad_position),
            create_matrix_scaling(quad_scaling)
        );
        glUniformMatrix4fv(model_location, 1, GL_FALSE, model.entries);

        
        glUniform4f(
            color_location, 
            object_color.entries[0], 
            object_color.entries[1], 
            object_color.entries[2], 
            object_color.entries[3]
        );
        triangle->draw();

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

vec4 getColor(const vec3 velocity){
    float max_speed = 6.0f; 

    float magnitude = std::sqrt(
        velocity.entries[0] * velocity.entries[0] +
        velocity.entries[1] * velocity.entries[1] +
        velocity.entries[2] * velocity.entries[2]
    );

    float s = std::clamp(magnitude / max_speed, 0.0f, 1.0f);

    for (size_t i = 0; i + 1 < ColorStops.size(); ++i){
        if (s >= ColorStops[i].pos && s <= ColorStops[i+1].pos){
            float span = (ColorStops[i+1].pos - ColorStops[i].pos);
            float t = (span > 0.0f) ? (s - ColorStops[i].pos) / span : 0.0f;

            vec4 lower = {
                ColorStops[i].r / 255.0f,
                ColorStops[i].g / 255.0f,
                ColorStops[i].b / 255.0f,
                1.0f
            };
            vec4 upper = {
                ColorStops[i+1].r / 255.0f,
                ColorStops[i+1].g / 255.0f,
                ColorStops[i+1].b / 255.0f,
                1.0f
            };

            return lerp(lower, upper, t);
        }
    }

    return {1.0f, 1.0f, 1.0f, 1.0f};
}

vec4 lerp(const vec4& a, const vec4& b, float s){
    vec4 result;
    result.entries[0] = a.entries[0] + (b.entries[0] - a.entries[0]) * s;
    result.entries[1] = a.entries[1] + (b.entries[1] - a.entries[1]) * s;
    result.entries[2] = a.entries[2] + (b.entries[2] - a.entries[2]) * s;
    result.entries[3] = a.entries[3] + (b.entries[3] - a.entries[3]) * s;
    return result;
}