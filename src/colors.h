#include <cstdint>
#include <vector>

typedef struct {
    float pos;
    float r, g, b;
} ColorStop;

std::vector<ColorStop> ColorStops = {
    {0.0f, 0.0f,   0.0f,   1.0f},  
    {0.6f, 1.0f, 0.0f,   0.0f},   
    {0.8f, 1.0f, 1.0f, 0.0f},    
    {1.0f, 1.0f, 1.0f, 1.0f}    
};

