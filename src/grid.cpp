#include "grid.h"

Grid::Grid(int halfSize, float spacing, float yOffset = 0.0f) {
  std::vector<float> verts;

  for (int i = -halfSize; i <= halfSize; ++i) {
    float offset = i * spacing;
    float extent = halfSize * spacing;

    // lines parallel to X axis
    verts.insert(verts.end(), {-extent, yOffset, offset});
    verts.insert(verts.end(), { extent, yOffset, offset});

    // lines parallel to Z axis
    verts.insert(verts.end(), {offset, yOffset, -extent});
    verts.insert(verts.end(), {offset, yOffset,  extent});
  }

  lineCount = verts.size() / 3;
  glGenVertexArrays(1, &VAO);
  glBindVertexArray(VAO);

  glGenBuffers(1, &VBO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  glBindVertexArray(0);
}

void Grid::draw(unsigned int shader, const float* proj, const float* view)
{
    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, proj);
    glUniformMatrix4fv(glGetUniformLocation(shader, "view"),       1, GL_FALSE, view);

    glBindVertexArray(VAO);
    glDrawArrays(GL_LINES, 0, lineCount);
    glBindVertexArray(0);
}

Grid::~Grid()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}
