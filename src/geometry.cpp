#include "geometry.h"

std::vector<Vec3> BuildGridLines(int halfSize, float spacing,
                                 float yOffset) {
  std::vector<Vec3> lines;
  for (int i = -halfSize; i <= halfSize; ++i) {
    float offset = i * spacing;
    float extent = halfSize * spacing;

    lines.push_back({-extent, yOffset, offset});
    lines.push_back({extent, yOffset, offset});

    lines.push_back({offset, yOffset, -extent});
    lines.push_back({offset, yOffset, extent});
  }
  return lines;
}

std::vector<Vec3> BuildBoundingBox() {
  std::vector<Vec3> lines {
    {
      {-1.0f, -1.0f, -1.0f}, {-1.0f, -1.0f, 1.0f}, {-1.0f, -1.0f, 1.0f},
      {1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, -1.0f},
      {1.0f, -1.0f, -1.0f}, {-1.0f, -1.0f, -1.0f},

      {-1.0f, -1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f}, {1.0f, -1.0f, -1.0f},
      {1.0f, 1.0f, -1.0f}, {-1.0f, -1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f},
      {1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 1.0f},

      {-1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f},
      {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, -1.0f},
      {1.0f, 1.0f, -1.0f}, { -1.0f, 1.0f, -1.0f}
    }
  };
  return lines;
}
    
	  
