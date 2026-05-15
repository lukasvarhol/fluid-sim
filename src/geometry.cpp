#include "geometry.h"
#include "objects3d/grid_state.h"

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

std::vector<Vec3> BuildCellGridLines()
{
    std::vector<Vec3> lines;
    lines.reserve(GRID_X * GRID_Y * GRID_Z * 24);

    for (int z = 0; z < GRID_Z; ++z)
    for (int y = 0; y < GRID_Y; ++y)
    for (int x = 0; x < GRID_X; ++x) {
        float cx = -1.0f + (x + 0.5f) * CELL_SIZE;
        float cy = -1.0f + (y + 0.5f) * CELL_SIZE;
        float cz = -1.0f + (z + 0.5f) * CELL_SIZE;
        float h  = CELL_SIZE * 0.5f;

        Vec3 v[8] = {
            {cx-h, cy-h, cz-h}, {cx+h, cy-h, cz-h},
            {cx+h, cy-h, cz+h}, {cx-h, cy-h, cz+h},
            {cx-h, cy+h, cz-h}, {cx+h, cy+h, cz-h},
            {cx+h, cy+h, cz+h}, {cx-h, cy+h, cz+h},
        };

        // 12 edges of a cube as line pairs
        int edges[24] = { 0,1, 1,2, 2,3, 3,0,   // bottom face
                          4,5, 5,6, 6,7, 7,4,   // top face
                          0,4, 1,5, 2,6, 3,7 }; // vertical edges
        for (int e = 0; e < 24; e += 2) {
            lines.push_back(v[edges[e]]);
            lines.push_back(v[edges[e+1]]);
        }
    }
    return lines;
}
