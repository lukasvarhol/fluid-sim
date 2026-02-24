 #include <vector>
 #include "linear_algebra.h"
 #include "helpers.h"

 struct Particles {
    std::vector<Vec3> positions;
    std::vector<Vec3> velocities;
    std::vector<Vec3> colors;
    std::vector<float> densities;
    unsigned int particle_count;
    unsigned int radius_px;

    Particles(const unsigned int particle_count, const unsigned int radius_px);
    void update(float dt, float g_fb_w, float g_fb_h);

private:
    static constexpr Vec3  GRAVITY{0.0f, -3.5f, 0.0f};
    static constexpr float MAX_SPEED = 1.5f;
    static constexpr float ENERGY_RETENTION_F = 0.8f;
    static constexpr float MASS = 1.0f;
    static constexpr float SMOOTHING_RADIUS = 0.4f;
    static constexpr float TARGET_DENSITY = 200.0f; 
    static constexpr float PRESSURE_MULTIPLIER = 0.5f;  


    Vec3 getColor(Vec3 vel);
    void keepInBoundaries(Vec3* pos, Vec3* vel, const float radius_px, const float g_fb_w, const float g_fb_h); 
    void applyGravity(Vec3& vel, float dt);
    float calculateDistance(Vec3& pos_a, Vec3& pos_b);
    float calculateInfluence(float distance);
    float smoothingDerivative(float distance);
    float calculateDensity(size_t position_index);
    void updateDensities();
    float calculateSharedPressure(float a, float b);
    Vec3 calculatePressureForce(int position_index);
    float densityToPressure(float density);
    void clampVelocity(Vec3& vel);
 };