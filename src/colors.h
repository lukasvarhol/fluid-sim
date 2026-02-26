#include <cstdint>
#include <vector>
#include "helpers.h"

typedef struct {
    float pos;
    float r, g, b;
} ColorStop;

std::array<float,3> color1 = rgba_normalizer(117, 14, 227);
std::array<float,3> color2 = rgba_normalizer(80, 199, 187);
std::array<float,3> color3 = rgba_normalizer(230, 213, 25);
std::array<float,3> color4 = rgba_normalizer(255, 0, 0);

std::vector<ColorStop> ColorStops = {
    {0.0f,  color1[0], color1[1], color1[2]},  
    {0.2f, color2[0], color2[1], color2[2]},   
    {0.6f, color3[0], color3[1], color3[2]},    
    {1.0f,  color4[0], color4[1], color4[2]}    
};

