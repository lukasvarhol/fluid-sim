#pragma once
// mouse interaction
extern float pushStrength;
extern float pullStrength;
extern float pushRadius; // radius of pushing force in NDC units
extern float pullRadius; // radius of pulling force in NDC units

// particle initialisation layout
extern float initSpacing;        // distance between particles in NDC units. Also affects the target desnity
extern float initOffsetX;         // horizontal offset amount in NDC units (negative -> left : positive -> right)
extern float initOffsetY;         // vertical offset amount in NDC units (negative -> down : positive -> up)
extern float initOffsetZ;

extern float radiusLogical;        // size of particle to be drawn on screen in pixels
extern unsigned int numParticles; // number of particles in simulation

// spatial hashing settings
extern float smoothingRadius;       // defines the area of influence of particle (Increase: slower simulation, greater instability, more accuracy)
const constexpr int MAX_NEIGHBOURS = 256; // size of data structure to hold neighbouring particles (Too small: simulation will explode. Too big: large memory consumption, slower iteration)

// particle behaviour
extern float relaxation;
extern float energyRetention;   // amount of energy retained after colliding with an object (wall, etc.)
const float maxSpeed = 3.0f;            // value to which speed is clamped
extern float gravity; // gravity (arbitrary value chosen which looks somewhat realistic)
extern float scorrCoefficient;               // scorr coefficient. (Lower = more instability, less damping)
extern int numIterations;            // number of times to run constraints solver (Lower = greater instability, greater performance)

// viscocity
extern float xsphC; // increase for greater viscocity
const float maxDv = 0.2f; // acceleration clamp

// vorticity
extern float vorticityEpsilon; // higher = more turbulence
