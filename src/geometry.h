#pragma once
#include <vector>
#include "linear_algebra.h"

std::vector<Vec3> BuildGridLines(int halfSize, float spacing,
                                 float yOffset = 0.0f);
std::vector<Vec3> BuildBoundingBox();
