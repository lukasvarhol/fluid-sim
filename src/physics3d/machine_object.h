#pragma once
#include "linear_algebra.h"
#include <string>

namespace physics3d {

enum class MachineObjectType {
    OpenContainer,
    Channel,
    Ramp,
    BoxBlock,
    SphereBlock,
};

// halfSize semantics per type:
//   OpenContainer / Channel — inner cavity half-extents (x=half-width, y=half-height, z=half-depth)
//   Ramp                    — x=half-width, z=half-depth  (y unused; angle controls tilt)
//   BoxBlock                — box half-extents
//   SphereBlock             — x=radius  (y, z unused)
struct MachineObject {
    MachineObjectType type;
    std::string       name;
    Vec3  position;
    Vec3  halfSize;
    float wallThickness = 0.025f;   // hollow objects only
    float angle         = 0.0f;    // Ramp: tilt in degrees around Z axis (0=flat, +right-side-down)
    float restitution   = 0.2f;
    float friction      = 0.2f;
};

} // namespace physics3d
