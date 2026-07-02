#pragma once
#include "linear_algebra.h"
#include <vector>

void gravityPredict(std::vector<Vec3> &positions_h,
                    std::vector<Vec3> &velocities_h,
                    std::vector<Vec3> &predictedPositions_h, float gravity,
                    float mouseStrength, float mouseRadius, Vec3 rayOrigin,
                    Vec3 rayDir, float dt, int n);


