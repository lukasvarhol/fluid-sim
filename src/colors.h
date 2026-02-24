#include <cstdint>
#include <vector>

typedef struct {
    float pos;
    float r, g, b;
} ColorStop;

std::vector<ColorStop> ColorStops = {
    {0.0f, 0.20f,   0.2f,   1.0f},  
    {0.45f, 0.2f, 1.0f, 0.5f},   
    {0.75f, 0.929f, 0.902f, 0.353f},    
    {1.0f, 1.0f, 0.25f, 0.25f}    
};

