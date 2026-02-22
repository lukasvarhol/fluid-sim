#include "linear_algebra.h"

mat4 create_matrix_transform(const std::vector<float>& translation) {
    mat4 matrix = mat4::identity();

    matrix.entries[12] = translation[0];
    matrix.entries[13] = translation[1];
    matrix.entries[14] = translation[2];

    return matrix;
}

mat4 create_matrix_scaling(const std::vector<float>& scaling) {
    mat4 matrix = mat4::identity();

    matrix.entries[0] = scaling[0];
    matrix.entries[5] = scaling[1];
    matrix.entries[10] = scaling[2];

    return matrix;
}

mat4 mat4_multiply(mat4 a, mat4 b){
    mat4 result;

    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {

            result.entries[col * 4 + row] = 
                a.entries[0 * 4 + row] * b.entries[col * 4 + 0] +
                a.entries[1 * 4 + row] * b.entries[col * 4 + 1] +
                a.entries[2 * 4 + row] * b.entries[col * 4 + 2] +
                a.entries[3 * 4 + row] * b.entries[col * 4 + 3];
        }
    }
    return result;
}

std::vector<float> addVector(const std::vector<float>& a, const std::vector<float>& b){
    if (a.size() != b.size()){
        std::cerr << "vector dimensions do not match" << std::endl;
        return a;
    } 
    size_t size = a.size();
    std::vector<float> result;

    for (size_t i = 0; i < size; ++i){
        result.push_back(a[i] + b[i]);
    }

    return result;
}

std::vector<float> subtractVector(const std::vector<float>& a, const std::vector<float>& b){
    if (a.size() != b.size()){
        std::cerr << "vector dimensions do not match" << std::endl;
        return a;
    } 
    size_t size = a.size();
    std::vector<float> result;

    for (size_t i = 0; i < size; ++i){
        result.push_back(a[i] - b[i]);
    }

    return result;
}

std::vector<float> scaleVector(const std::vector<float>& a, const float& s){
    std::vector<float> result;

    for (float element : a){
        result.push_back(element * s);
    }

    return result;
}

std::vector<float> elemwiseMultiply(const std::vector<float>& a, const std::vector<float>& b){
    if (a.size() != b.size()){
        std::cerr << "vector dimensions do not match" << std::endl;
        return a;
    } 
    size_t size = a.size();

    std::vector<float> result;

    for (size_t i = 0; i < size; ++i){
        result.push_back(a[i] * b[i]);
    }

    return result;
}

float sumVectorComponents(const std::vector<float>& a){
    float sum = 0;
    for (float element : a){
        sum += element;
    }

    return sum;
}

std::vector<float> lerp(const std::vector<float>& a, const std::vector<float>& b, float s){
    if (a.size() != b.size()){
        std::cerr << "vector dimensions do not match" << std::endl;
        return a;
    }
    return addVector(a,scaleVector(subtractVector(b,a),s));
}