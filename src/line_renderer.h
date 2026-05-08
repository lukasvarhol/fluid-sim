#pragma once
#include <vector>
#include <glad/glad.h>
#include "linear_algebra.h"

class LineRenderer {
public:
  LineRenderer(const std::vector<Vec3> &lines);
  void Draw(unsigned int shader, const float *projection, const float *view);
  ~LineRenderer();

private:
  unsigned int VAO, VBO;
  int lineCount;
};
