#include <vector>
#include "linear_algebra.h"

constexpr float PI = 3.14159265358979323846f;

struct Particle {
    std::vector<float> pos;
    std::vector<float> vel;
    std::vector<float> color;
    unsigned int radius_px;

    Particle(std::vector<float> pos, std::vector<float> vel, unsigned int radius_px);
    void update(float dt, float g_fb_w, float g_fb_h);
};

extern std::vector<Particle> particles;
extern std::vector<float> densities;
extern const float smoothing_radius;

std::vector<float> getColor(const std::vector<float> vel);
void keepInBoundaries(Particle* particle, const float g_fb_w, const float g_fb_h);
void applyGravity(Particle* particle, float dt);
float calculateDistance(Particle* a, Particle* b);
float calculateInfluence(float distance);
float smoothingDerivative(float distance);
float calculateDensity(Particle& particle);
void updateDensities();
std::vector<float> calculatePressureForce(int particleIndex);
float densityToPressure(float density);
float calculateSharedPressure(float a, float b);
void clampVelocity(Particle& p);

