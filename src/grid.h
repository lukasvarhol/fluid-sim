#pragma once
#include <vector>
#include <glad/glad.h>

class Grid {
public:
  Grid(int halfSize, float spacing, float yOffset);
  void draw(unsigned int shader, const float *proj, const float *view);
  ~Grid();

private:
  unsigned int VAO, VBO;
  int lineCount;
};
