#include "line_renderer.h"

LineRenderer::LineRenderer(const std::vector<Vec3> &lines) {
  lineCount = lines.size();
  glGenVertexArrays(1, &VAO);
  glBindVertexArray(VAO);

  glGenBuffers(1, &VBO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, lines.size() * 3 * sizeof(float), lines.data(),
	       GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  glBindVertexArray(0);
}

void LineRenderer::Draw(unsigned int shader, const float *projection,
                        const float *view) {
  glUseProgram(shader);
  glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, projection);
  glUniformMatrix4fv(glGetUniformLocation(shader, "view"),       1, GL_FALSE, view);

  glBindVertexArray(VAO);
  glDrawArrays(GL_LINES, 0, lineCount);
  glBindVertexArray(0);
}

LineRenderer::~LineRenderer() {
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
}
