#include <vector>
#include "linear_algebra.h"


struct Particle {
    std::vector<float> pos;
    std::vector<float> vel;
    std::vector<float> color;
    unsigned int radius_px;

    Particle(std::vector<float> pos, std::vector<float> vel, unsigned int radius_px);
    void update(float dt, float g_fb_w, float g_fb_h);
};
