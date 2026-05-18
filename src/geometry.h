#pragma once
#include <vector>
#include "linear_algebra.h"

std::vector<Vec3> BuildGridLines(int halfSize, float spacing,
                                 float yOffset = 0.0f);
std::vector<Vec3> BuildBoundingBox();

// 5x5x5 cell wireframe: 12 edges per cell, 125 cells = 1500 line segments
std::vector<Vec3> BuildCellGridLines();
