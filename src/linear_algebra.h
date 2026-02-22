#pragma once
#include <vector> 
#include <iostream>

struct mat4 {
    float entries[16];

    static mat4 identity() {
        mat4 m{};
        m.entries[0]  = 1.0f;
        m.entries[5]  = 1.0f;
        m.entries[10] = 1.0f;
        m.entries[15] = 1.0f;
        return m;
    }
};

mat4 create_matrix_transform(const std::vector<float>& translation);
mat4 create_matrix_scaling(const std::vector<float>& scaling);
mat4 mat4_multiply(mat4 a, mat4 b);
std::vector<float> addVector(const std::vector<float>& a, const std::vector<float>& b);
std::vector<float> subtractVector(const std::vector<float>& a, const std::vector<float>& b);
std::vector<float> scaleVector(const std::vector<float>& a, const float& s);
std::vector<float> elemwiseMultiply(const std::vector<float>& a, const std::vector<float>& b);
float sumVectorComponents(const std::vector<float>& a);
std::vector<float> lerp(const std::vector<float>& a, const std::vector<float>& b, float s);