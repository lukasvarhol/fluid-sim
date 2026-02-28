#include <vector>
#include <iostream>
#include <unordered_map>
#include <array>
#include "linear_algebra.h"
#include "helpers.h"

struct Particles
{
    std::vector<Vec3> positions;
    std::vector<Vec3> predicted_positions;
    std::vector<Vec3> velocities;
    std::vector<Vec3> colors;
    std::vector<float> densities;
    std::vector<std::array<unsigned int,2>> spatial_lut;
    std::vector<unsigned int> start_indices;
    unsigned int particle_count;
    unsigned int radius_px;
    std::unordered_map<unsigned int, unsigned int> start_index_by_hash;

    Particles(const unsigned int particle_count, const unsigned int radius_px);
    void update(float dt, float g_fb_w, float g_fb_h, bool interact, Vec3 cursor, float radius, float strength);
    Vec3 interactionForce(Vec3 mouse_pos, float radius, float strength, int particle_idx);

private:
    static constexpr Vec3 GRAVITY{0.0f, -7.0f, 0.0f};
    static constexpr float MAX_SPEED = 3.5f;
    static constexpr float ENERGY_RETENTION_F = 0.6f;
    static constexpr float MASS = 1.0f;
    static constexpr float SMOOTHING_RADIUS = 0.06f;
    static constexpr float TARGET_DENSITY = 180.0f;
    static constexpr float PRESSURE_MULTIPLIER = 0.5f;
    static constexpr float VISCOSITY_COEFFICIENT = 0.005f;
    static constexpr std::array<std::array<int, 2>, 9> cell_offsets = {{
        {-1, -1}, {0, -1}, {1, -1},
        {-1,  0}, {0,  0}, {1,  0},
        {-1,  1}, {0,  1}, {1,  1}
    }};

    Vec3 getColor(Vec3 &vel);
    Vec3 calculateBoundaryForce(int i, float g_fb_w, float g_fb_h);
    void keepInBoundaries(Vec3 *pos, Vec3 *vel, const float radius_px, const float g_fb_w, const float g_fb_h);
    void applyGravity(Vec3 &vel, float dt);
    float calculateDistance(Vec3 &pos_a, Vec3 &pos_b);
    float smoothingKernel(float distance);
    float smoothingKernelDerivative(float distance);
    float smoothingKernelLaplacian(float distance);
    float calculateDensity(Vec3 &position, std::vector<int>& neighbours);
    float calculateSharedPressure(float a, float b);
    Vec3 calculatePressureForce(int position_index, std::vector<int>& neighbours);
    Vec3 calculateViscosity(int particle_index, std::vector<int>& neighbours);
    float densityToPressure(float density);
    void clampVelocity(Vec3 &vel);
    void updateSpatialLut(std::vector<Vec3>& pos);
    std::array<int, 2> positionToCellCord(Vec3 point);
    unsigned int hashCell(std::array<int, 2> cell);
    unsigned int getKeyFromHash(unsigned int hash);
    void foreachPointInRadius(Vec3 point, float radius, std::vector<int>& out);
};