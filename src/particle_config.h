#pragma once
// mouse interaction
extern float PUSH_STREN;
extern float PULL_STREN;
extern float PUSH_RAD; // radius of pushing force in NDC units
extern float PULL_RAD; // radius of pulling force in NDC units

// particle initialisation layout
extern float INIT_SPACING;        // distance between particles in NDC units. Also affects the target desnity
extern float INIT_OFFSET_X;         // horizontal offset amount in NDC units (negative -> left : positive -> right)
extern float INIT_OFFSET_Y;         // vertical offset amount in NDC units (negative -> down : positive -> up)
extern float radius_logical;        // size of particle to be drawn on screen in pixels
extern unsigned int NUM_PARTICLES; // number of particles in simulation

// spatial hashing settings
extern float smoothingRadius;       // defines the area of influence of particle (Increase: slower simulation, greater instability, more accuracy)
const constexpr int MAX_NEIGHBOURS = 256; // size of data structure to hold neighbouring particles (Too small: simulation will explode. Too big: large memory consumption, slower iteration)

// particle behaviour
extern float RELAXATION_F;
extern float ENERGY_RETENTION_F;   // amount of energy retained after colliding with an object (wall, etc.)
const float MAX_SPEED = 3.0f;            // value to which speed is clamped
extern float gravity; // gravity (arbitrary value chosen which looks somewhat realistic)
extern float k;               // scorr coefficient. (Lower = more instability, less damping)
extern int NUM_ITERATIONS;            // number of times to run constraints solver (Lower = greater instability, greater performance)

// viscocity
extern float xsphC; // increase for greater viscocity
const float maxDv = 0.2f; // acceleration clamp

// vorticity
extern float vorticityEpsilon; // higher = more turbulence
