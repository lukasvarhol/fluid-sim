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
    spatial_lut.resize(particle_count);
    start_indices.resize(particle_count);
    start_indices.resize(particle_count * 3);

    float particle_spacing = 0.05f;

    int particles_per_row = (int)std::sqrt(particle_count);
    int particles_per_col = (particle_count - 1) / particles_per_row + 1;
    float spacing = ((radius_px>>6)<<1) + particle_spacing;

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
    for (size_t i = 0; i < particle_count; ++i) applyGravity(velocities[i], dt);

    for (size_t i = 0; i < particle_count; ++i) {
        predicted_positions[i] = positions[i] + velocities[i] * dt;
    }

    updateSpatialLut(predicted_positions); 

    std::vector<int> neighbours;
    neighbours.reserve(64);

    for (size_t i = 0; i < particle_count; ++i) {
        foreachPointInRadius(predicted_positions[i], neighbours);
        densities[i] = calculateDensity(predicted_positions[i], neighbours);
    }

    for (size_t i = 0; i < particle_count; ++i) {
        foreachPointInRadius(positions[i], neighbours);
        Vec3 pressureForce = calculatePressureForce((int)i, neighbours);
        Vec3 viscosityForce = calculateViscosity((int)i, neighbours);

        Vec3 accel = (pressureForce + viscosityForce) / MASS;
        velocities[i] += accel * dt;
        clampVelocity(velocities[i]);
        colors[i] = getColor(velocities[i]);
    }

    for (size_t i = 0; i < particle_count; ++i) {
        positions[i] += velocities[i] * dt;
        keepInBoundaries(&positions[i], &velocities[i], radius_px, g_fb_w, g_fb_h);
    }
}

Vec3 Particles::calculateBoundaryForce(int i, float g_fb_w, float g_fb_h){
    Vec3 f{0.0f, 0.0f, 0.0f};
    float margin = SMOOTHING_RADIUS;
    float strength = 1.5f;

    Vec3& pos = positions[i];

    if (pos.x < -1.0f + margin)
        f.x += strength * (1.0f - (pos.x + 1.0f) / margin);
    if (pos.x >  1.0f - margin)
        f.x -= strength * (1.0f - (1.0f - pos.x) / margin);
    if (pos.y < -1.0f + margin)
        f.y += strength * (1.0f - (pos.y + 1.0f) / margin);
    if (pos.y >  1.0f - margin)
        f.y -= strength * (1.0f - (1.0f - pos.y) / margin);

    return f;
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
            if (*vel_arr[i] < 0) *vel_arr[i] *= -ENERGY_RETENTION_F;
        } else if (*pos_arr[i] >= hi) {
            *pos_arr[i] = hi;
            if (*vel_arr[i] > 0) *vel_arr[i] *= -ENERGY_RETENTION_F;
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

float Particles::smoothingKernelLaplacian(float distance){
    if (distance >= SMOOTHING_RADIUS) return 0.0f;
    float h = SMOOTHING_RADIUS;
    return 36.0f * (h - distance) / (5.0f * PI * std::powf(h, 5));
}

float Particles::calculateDensity(Vec3& position, std::vector<int>& neighbours){
    float density = 0.0f; 

    for (int particle_index : neighbours){
        float distance = calculateDistance(position, predicted_positions[particle_index]);
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

Vec3 Particles::calculatePressureForce(int particle_index, std::vector<int>& neighbours){
    Vec3 pressure_force{0.0f, 0.0f, 0.0f};

    for (int other_particle_index : neighbours){
        if (particle_index == other_particle_index) continue;

        Vec3 offset = positions[other_particle_index] - positions[particle_index];
        float dst = offset.magnitude();
        Vec3 dir = dst == 0 ? Vec3{0.1f,0.1f,0.0f} : offset / dst;

        float slope = smoothingKernelDerivative(dst);
        float density = densities[other_particle_index];
        if (density == 0.0f) continue;
        float shared_pressure = calculateSharedPressure(density, densities[particle_index]);
        pressure_force += dir * shared_pressure * slope * MASS/ density;
    }

    return pressure_force;
}

Vec3 Particles::calculateViscosity(int particle_index, std::vector<int>& neighbours){
    Vec3 viscosity{0.0f, 0.0f, 0.0f};

    for (int other_particle_index : neighbours){
        if (particle_index == other_particle_index) continue;

        Vec3 veldiff = velocities[other_particle_index] - velocities[particle_index];
        float dst = calculateDistance(positions[other_particle_index], positions[particle_index]);
        float laplacian = smoothingKernelLaplacian(dst);
        float density = densities[other_particle_index];
        if (density == 0.0f) continue;
        viscosity += veldiff * (MASS * laplacian / density);
    }

    return viscosity * VISCOSITY_COEFFICIENT;
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

void Particles::updateSpatialLut(std::vector<Vec3>& pos)
{
    for (size_t i = 0; i < pos.size(); ++i) {
        auto cell = positionToCellCord(pos[i]);
        unsigned int hash = hashCell(cell);
        spatial_lut[i] = { hash, (unsigned int)i };
    }

    std::sort(spatial_lut.begin(), spatial_lut.end(),
        [](const auto& a, const auto& b) { return a[0] < b[0]; });

    start_index_by_hash.clear();
    start_index_by_hash.reserve(spatial_lut.size());

    for (size_t i = 0; i < spatial_lut.size(); ++i) {
        unsigned int hash = spatial_lut[i][0];
        if (i == 0 || hash != spatial_lut[i - 1][0]) {
            start_index_by_hash[hash] = (unsigned int)i;
        }
    }
}

std::array<int, 2> Particles::positionToCellCord(Vec3 point){
    std::array<int, 2> cell_coord;
    cell_coord[0] = (int)std::floor(point.x / SMOOTHING_RADIUS);
    cell_coord[1] = (int)std::floor(point.y / SMOOTHING_RADIUS);
    return cell_coord;
}

unsigned int Particles::hashCell(std::array<int, 2> cell){
    unsigned int a = (unsigned int)cell[0] * 130399;
    unsigned int b = (unsigned int)cell[1] * 184043;
    return a ^ b; 
}

unsigned int Particles::getKeyFromHash(unsigned int hash){
    return hash % (unsigned int)start_indices.size(); 
}

void Particles::foreachPointInRadius(Vec3 point, std::vector<int>& out)
{
    out.clear();
    auto centre = positionToCellCord(point);
    float radius_sq = SMOOTHING_RADIUS * SMOOTHING_RADIUS;

    for (auto offset : cell_offsets) {
        std::array<int,2> cell = { centre[0] + offset[0], centre[1] + offset[1] };
        unsigned int hash = hashCell(cell);

        auto it = start_index_by_hash.find(hash);
        if (it == start_index_by_hash.end()) continue;

        for (unsigned int i = it->second; i < spatial_lut.size(); ++i) {
            if (spatial_lut[i][0] != hash) break;       
            int particle_index = (int)spatial_lut[i][1];

            Vec3 d = positions[particle_index] - point;
            float dst_sq = d.x*d.x + d.y*d.y + d.z*d.z;

            if (dst_sq <= radius_sq) out.push_back(particle_index);
        }
    }
}