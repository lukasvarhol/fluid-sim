#include "particle.h"
#include "colors.h"
#include <algorithm>

const std::vector<float> gravity = {0.0f, -3.5f, 0.0f};  
const float MAX_SPEED = 3.5f; 
const float ENERGY_RETENTION_F = 0.8f;

const float mass = 1.0f;
const float smoothing_radius = 0.15f;  
const float targetDensity = 3.0f;        
const float pressureMultiplier = 0.7f; 


float radius_ndc_y; 
float radius_ndc_x;
float halfsize_x;
float halfsize_y; 
float floorY;
float ceilY;
float leftX;
float rightX;



Particle::Particle(std::vector<float> pos, std::vector<float> vel, unsigned int radius_px){
    this->pos = pos;
    this->vel = vel;
    this->radius_px = radius_px;
    this->color = {0.0f, 0.0f, 1.0f, 1.0f};
    
}

void Particle::update(float dt, float g_fb_w, float g_fb_h){
    // applyGravity(this, dt);
    pos = addVector(pos,scaleVector(vel,dt));
    color = getColor(vel);
    keepInBoundaries(this, g_fb_w, g_fb_h);

}

std::vector<float> getColor(const std::vector<float> vel){

    float magnitude = calculateMagnitude(vel);

    float s = std::clamp(magnitude / MAX_SPEED, 0.0f, 1.0f);

    for (size_t i = 0; i + 1 < ColorStops.size(); ++i){
        if (s >= ColorStops[i].pos && s <= ColorStops[i+1].pos){
            float span = (ColorStops[i+1].pos - ColorStops[i].pos);
            float t = (span > 0.0f) ? (s - ColorStops[i].pos) / span : 0.0f;

            std::vector<float> lower = {
                ColorStops[i].r,
                ColorStops[i].g,
                ColorStops[i].b,
                1.0f
            };
            std::vector<float> upper = {
                ColorStops[i+1].r,
                ColorStops[i+1].g,
                ColorStops[i+1].b,
                1.0f
            };

            return lerp(lower, upper, t);
        }
    }

    return {1.0f, 1.0f, 1.0f, 1.0f};
}

void keepInBoundaries(Particle* particle, const float g_fb_w, const float g_fb_h){
        radius_ndc_y = (particle->radius_px) / g_fb_h; 
        radius_ndc_x = (particle->radius_px) / g_fb_w;
        halfsize_x = radius_ndc_x;
        halfsize_y = radius_ndc_y; 
        floorY = -1.0f + radius_ndc_y;
        ceilY = 1.0f - radius_ndc_y;
        leftX = -1.0f + radius_ndc_x;
        rightX = 1.0f - radius_ndc_x;

        if (particle->pos[1] <= floorY) {
            particle->pos[1] = floorY;
            particle->vel[1] *= -1.0f * ENERGY_RETENTION_F;
        }

        if (particle->pos[1] >= ceilY) {
            particle->pos[1] = ceilY;
            particle->vel[1] *= -1.0f * ENERGY_RETENTION_F;
        }

        if (particle->pos[0] <= leftX) {
            particle->pos[0] = leftX;
            particle->vel[0] *= -1.0f * ENERGY_RETENTION_F;
        }

        if (particle->pos[0] >= rightX) {
            particle->pos[0] = rightX;
            particle->vel[0] *= -1.0f * ENERGY_RETENTION_F;
        }
}
void applyGravity(Particle* particle, float dt){
    particle->vel = addVector(particle->vel,scaleVector(gravity,dt));
}

float calculateDistance(Particle* a, Particle* b){
    return calculateMagnitude(subtractVector(a->pos, b->pos));
}

float calculateInfluence(float distance){
    if (distance >= smoothing_radius) return 0.0f;

    float volume = (PI * std::powf(smoothing_radius,4))/6.0f;
    return (smoothing_radius - distance) * (smoothing_radius - distance) / volume;
}

float smoothingDerivative(float distance){
    if (distance >= smoothing_radius) return 0.0f;

    float derivative = -(12.0f / (PI * powf(smoothing_radius, 6)));  // note the negative
    return (distance - smoothing_radius) * derivative;
}

float calculateDensity(Particle& particle){
    float density = 0.0f; 

    for(Particle p : particles){
        float distance = calculateMagnitude(subtractVector(particle.pos,p.pos));
        if (distance >= smoothing_radius) continue; 
        float influence = calculateInfluence(distance);
        density += mass * influence;
    }

    return density;
}

void updateDensities(){
    densities.resize(particles.size());
    for (size_t i = 0; i < particles.size(); ++i){
        densities[i] = calculateDensity(particles[i]);
    }
}

float calculateSharedPressure(float a, float b){
    float pressure_a = densityToPressure(a);
    float pressure_b = densityToPressure(b);

    return (pressure_a + pressure_b) / 2.0f;
}

std::vector<float> calculatePressureForce(int particleIndex){
    std::vector<float> pressureForce = {0.0f, 0.0f, 0.0f};

    for (size_t i = 0; i < particles.size(); ++i){
        if (i == particleIndex) continue;

        auto r = subtractVector(particles[particleIndex].pos, particles[i].pos);
        float distance = calculateMagnitude(r);

        if (distance < 1e-6f) continue;
        if (distance >= smoothing_radius) continue;

        std::vector<float> direction = scaleVector(r, 1.0f / distance);
        float slope = smoothingDerivative(distance);
        float rho = std::max(densities[i], 1e-6f);

        float sharedPressure = calculateSharedPressure(rho, densities[particleIndex]);

        pressureForce = addVector(
            pressureForce,
            scaleVector(direction, sharedPressure * slope * mass / rho)
        );
    }

    return pressureForce;
}

float densityToPressure(float density){
    float densityErr = density - targetDensity;
    float pressure = densityErr * pressureMultiplier;
    return pressure;
}

void clampVelocity(Particle& p) {
    float speed = calculateMagnitude(p.vel);
    if (speed > MAX_SPEED)
        p.vel = scaleVector(p.vel, MAX_SPEED / speed);
}
