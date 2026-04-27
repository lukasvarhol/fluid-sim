#include "particle_config.h"

float gravity        = -3.5f;
float xsphC          =  0.1f;
float vorticityEpsilon = 1000.0f;
float k              =  0.000006f;
float RELAXATION_F   =  15000.0f;
int   NUM_ITERATIONS =  4;
float PUSH_STREN     =  15.0f;
float PULL_STREN     = -15.0f;
float PUSH_RAD       =  0.3f;
float PULL_RAD       =  0.3f;
float INIT_SPACING   =  0.03f;
float INIT_OFFSET_X  =  0.0f;
float INIT_OFFSET_Y  = 0.0f;
float INIT_OFFSET_Z   = 0.0f;
float radius_logical =  13.0f;
float smoothingRadius=  0.07f;
float ENERGY_RETENTION_F = 0.7f;
unsigned int NUM_PARTICLES = 40000;
