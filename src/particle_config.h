#pragma once

// mouse interaction
static float PUSH_STREN = -30.0f;
static float PULL_STREN = 20.0f;
static float PUSH_RAD = 0.15; // radius of pushing force in NDC units
static float PULL_RAD = 0.3f; // radius of pulling force in NDC units

// particle initialisation layout
const float INIT_SPACING = 0.014f;        // distance between particles in NDC units. Also affects the target desnity
const float INIT_OFFSET_X = -0.3;         // horizontal offset amount in NDC units (negative -> left : positive -> right)
const float INIT_OFFSET_Y = -0.3;         // vertical offset amount in NDC units (negative -> down : positive -> up)
static float radius_logical = 2.0f;        // size of particle to be drawn on screen in pixels
static unsigned int NUM_PARTICLES = 5000; // number of particles in simulation

// spatial hashing settings
const float smoothingRadius = 0.05f;       // defines the area of influence of particle (Increase: slower simulation, greater instability, more accuracy)
static constexpr int MAX_NEIGHBOURS = 256; // size of data structure to hold neighbouring particles (Too small: simulation will explode. Too big: large memory consumption, slower iteration)

// particle behaviour
const float RELAXATION_F = 15000.0f;
const float ENERGY_RETENTION_F = 0.7f;   // amount of energy retained after colliding with an object (wall, etc.)
const float MAX_SPEED = 3.0f;            // value to which speed is clamped
const Vec2 gravity = Vec2{-0.0f, -3.5f}; // gravity (arbitrary value chosen which looks somewhat realistic)
const float k = 0.000006f;               // scorr coefficient. (Lower = more instability, less damping)
const int NUM_ITERATIONS = 4;            // number of times to run constraints solver (Lower = greater instability, greater performance)

// viscocity
const float xsphC = 0.1f; // increase for greater viscocity
const float maxDv = 0.2f; // acceleration clamp

// vorticity
const float vorticityEpsilon = 4000.0f; // higher = more turbulence