#pragma once
#include <vector>

class TriangleMesh {
public:
    TriangleMesh();
    void draw();
    ~TriangleMesh();

private:
    unsigned int EBO, VAO, vertexCount;
    unsigned int VBO;
};