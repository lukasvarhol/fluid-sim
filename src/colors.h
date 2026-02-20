#include <cstdint>
#include <vector>

typedef struct {
    float pos;
    uint8_t r, g, b;
} ColorStop;

std::vector<ColorStop> ColorStops = {
    {0.0f, 0,   0,   255},  
    {0.6f, 255, 0,   0},   
    {0.8f,255, 255, 0},    
    {1.0f, 255, 255, 255}    
};

