#include "particle_config.h"

bool  tricklerMode      = false;
float tricklerSpawnRate = 500.0f;
float tricklerSpread    = 0.06f;
float tricklerOriginX   = 0.1f;
float tricklerOriginY   = 0.8f;
float tricklerOriginZ   = 0.1f;

float gravity        = -3.5f;
float xsphC          =  0.1f;
float vorticityEpsilon = 1000.0f;
float scorrCoefficient =  0.000006f;
float relaxation       =  15000.0f;
int   numIterations    =  2;
float pushStrength     =  15.0f;
float pullStrength     = -15.0f;
float pushRadius       =  0.3f;
float pullRadius = 0.3f;
float initOffsetX      =  0.0f;
float initOffsetY      = 0.0f;
float initOffsetZ      = 0.0f;
float radiusLogical    =  7.0f;

float energyRetention = 0.7f;

#ifdef USE_CUDA
unsigned int numParticles = 500'000;
float initSpacing = 0.013f;
float smoothingRadius  =  0.035f;
#else
unsigned int numParticles = 10'000;
float initSpacing = 0.025f;
float smoothingRadius  =  0.065f;
#endif;
