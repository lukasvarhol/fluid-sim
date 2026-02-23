#include "particle.h"
#include "colors.h"
#include <algorithm>

std::vector<float> gravity = {0.0f, -3.5f, 0.0f};  
float max_speed = 3.5f; 
float energy_loss = 0.85;

float radius_ndc_y; 
float radius_ndc_x;
float halfsize_x;
float halfsize_y; 
float floorY;
float ceilY;
float leftX;
float rightX;

std::vector<float> getColor(const std::vector<float> vel);
void keepInBoundaries(Particle* particle, float g_fb_w, float g_fb_h);

Particle::Particle(std::vector<float> pos, std::vector<float> vel, unsigned int radius_px){
    this->pos = pos;
    this->vel = vel;
    this->radius_px = radius_px;
    this->color = {0.0f, 0.0f, 1.0f, 1.0f};
    
}

void Particle::update(float dt, float g_fb_w, float g_fb_h){
    vel = addVector(vel,scaleVector(gravity,dt));
    pos = addVector(pos,scaleVector(vel,dt));
    color = getColor(vel);
    keepInBoundaries(this, g_fb_w, g_fb_h);
}

std::vector<float> getColor(const std::vector<float> vel){

    float magnitude = std::sqrt(sumVectorComponents(elemwiseMultiply(vel,vel)));

    float s = std::clamp(magnitude / max_speed, 0.0f, 1.0f);

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

void keepInBoundaries(Particle* particle, float g_fb_w, float g_fb_h){
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
            particle->vel[1] *= -1.0f * energy_loss;
        }

        if (particle->pos[1] >= ceilY) {
            particle->pos[1] = ceilY;
            particle->vel[1] *= -1.0f * energy_loss;
        }

        if (particle->pos[0] <= leftX) {
            particle->pos[0] = leftX;
            particle->vel[0] *= -1.0f * energy_loss;
        }

        if (particle->pos[0] >= rightX) {
            particle->pos[0] = rightX;
            particle->vel[0] *= -1.0f * energy_loss;
        }
}