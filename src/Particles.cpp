#include "particles.h"
#include "colors.h"

Particles::Particles(const unsigned int particle_count, const unsigned int radius_px)
    : particle_count(particle_count), radius_px(radius_px)
{
    positions.resize(particle_count);
    velocities.resize(particle_count);
    colors.resize(particle_count);
    densities.resize(particle_count);
    predicted_positions.resize(particle_count);

    float particle_spacing = 0.04f;

    int particles_per_row = (int)std::sqrt(particle_count);
    int particles_per_col = (particle_count - 1) / particles_per_row + 1;
    float spacing = (radius_px/200.0f) *2 + particle_spacing;

    for (unsigned int i = 0; i < particle_count; ++i) {
        float x = (i % particles_per_row - particles_per_row / 2.0f + 0.5f) * spacing;
        float y = (i / particles_per_row - particles_per_col / 2.0f + 0.5f) * spacing;
        positions[i] = { x, y, 0.0f };
        predicted_positions[i] = { x, y, 0.0f };
        velocities[i] = { 0.0f, 0.0f, 0.0f };
        colors[i]     = { 0.0f, 0.0f, 1.0f };
        densities[i]  = 0.0f;
    }
}

void Particles::update(float dt, float g_fb_w, float g_fb_h)
{
    // Optional: gravity first
    for (size_t i = 0; i < particle_count; ++i) applyGravity(velocities[i], dt);

    // 1) predict positions (assignment, not +=)
    for (size_t i = 0; i < particle_count; ++i) {
        predicted_positions[i] = positions[i] + velocities[i] * dt;
    }

    // 2) compute densities using predicted positions
    for (size_t i = 0; i < particle_count; ++i) {
        densities[i] = calculateDensity(predicted_positions[i]); // make sure this uses predicted vs positions inside
    }

    // 3) compute forces + integrate
    for (size_t i = 0; i < particle_count; ++i) {
        Vec3 pressureForce = calculatePressureForce((int)i); // update this to use predicted_positions too (see below)

        Vec3 accel = pressureForce / MASS;
        velocities[i] += accel * dt;

        clampVelocity(velocities[i]);
        positions[i] += velocities[i] * dt;

        colors[i] = getColor(velocities[i]);
        keepInBoundaries(&positions[i], &velocities[i], radius_px, g_fb_w, g_fb_h);
    }
}

Vec3 Particles::getColor(Vec3& vel){
    float magnitude = vel.magnitude();
    float s = std::clamp(magnitude / MAX_SPEED, 0.0f, 1.0f);

    for (size_t i = 0; i + 1 < ColorStops.size(); ++i){
        if (s >= ColorStops[i].pos && s <= ColorStops[i+1].pos){
            float span = (ColorStops[i+1].pos - ColorStops[i].pos);
            float t = (span > 0.0f) ? (s - ColorStops[i].pos) / span : 0.0f;

            Vec3 lower = {
                ColorStops[i].r,
                ColorStops[i].g,
                ColorStops[i].b
            };
            Vec3 upper = {
                ColorStops[i+1].r,
                ColorStops[i+1].g,
                ColorStops[i+1].b
            };

            return lerp(lower, upper, t);
        }
    }
    return {1.0f, 1.0f, 1.0f};
}

// Careful! only checks collisions in 2D
void Particles::keepInBoundaries(Vec3* pos, Vec3* vel, const float radius_px, const float g_fb_w, const float g_fb_h) {
    float half[2] = {
        radius_px / g_fb_w,
        radius_px / g_fb_h
    };
    float* pos_arr[2] = { &pos->x, &pos->y };
    float* vel_arr[2] = { &vel->x, &vel->y };

    for (int i = 0; i < 2; i++) {
        float lo = -1.0f + half[i];
        float hi =  1.0f - half[i];

        if (*pos_arr[i] <= lo) {
            *pos_arr[i] = lo;
            *vel_arr[i] *= -ENERGY_RETENTION_F;
        } else if (*pos_arr[i] >= hi) {
            *pos_arr[i] = hi;
            *vel_arr[i] *= -ENERGY_RETENTION_F;
        }
    }
}

void Particles::applyGravity(Vec3& vel, float dt){
    vel += GRAVITY * dt;
}

float Particles::calculateDistance(Vec3& pos_a, Vec3& pos_b){
    return (pos_a - pos_b).magnitude();
}

float Particles::smoothingKernel(float distance){
    if (distance >= SMOOTHING_RADIUS) return 0.0f;
    float volume = (25 * PI * std::powf(SMOOTHING_RADIUS,5)) / 10.0f;
    return (3.0f * (SMOOTHING_RADIUS - distance) * (SMOOTHING_RADIUS - distance) * (SMOOTHING_RADIUS - distance)) / volume;
}

float Particles::smoothingKernelDerivative(float distance){
    if (distance >= SMOOTHING_RADIUS) return 0.0f;
    float coef = -18.0f / (5.0f * PI * std::powf(SMOOTHING_RADIUS,5));
    return coef * (SMOOTHING_RADIUS - distance) * (SMOOTHING_RADIUS - distance);
}

float Particles::calculateDensity(Vec3& position){
    float density = 0.0f; 

    for(size_t i = 0; i < positions.size(); ++i){
        float distance = calculateDistance(position, positions[i]);
        if (distance >= SMOOTHING_RADIUS) continue; 
        float influence = smoothingKernel(distance);
        density += MASS * influence;
    }
    return density;
}

float Particles::calculateSharedPressure(float a, float b){
    float pressure_a = densityToPressure(a);
    float pressure_b = densityToPressure(b);

    return (pressure_a + pressure_b) / 2.0f;
}

Vec3 Particles::calculatePressureForce(int particle_index){
    Vec3 pressure_force{0.0f, 0.0f, 0.0f};

    for (int other_particle_index = 0; other_particle_index < particle_count; ++other_particle_index){
        if (particle_index == other_particle_index) continue;

        Vec3 offset = positions[other_particle_index] - positions[particle_index];
        float dst = offset.magnitude();
        Vec3 dir = dst == 0 ? Vec3{0.1f,0.1f,0.0f} : offset / dst;

        float slope = smoothingKernelDerivative(dst);
        float density = densities[other_particle_index];
        float shared_pressure = calculateSharedPressure(density, densities[particle_index]);
        pressure_force += dir * shared_pressure * slope * MASS/ density;
    }

    return pressure_force;
}

float Particles::densityToPressure(float density){
    float densityErr = density - TARGET_DENSITY;
    float pressure = densityErr * PRESSURE_MULTIPLIER;
    return pressure;
}

void Particles::clampVelocity(Vec3& vel){
    float speed = vel.magnitude();
    if (speed > MAX_SPEED)
        vel = vel * (MAX_SPEED/speed);
}