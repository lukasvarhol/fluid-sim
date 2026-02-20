#include "linear_algebra.h"

mat4 create_matrix_transform(vec3 translation) {
    mat4 matrix = mat4::identity();

    matrix.entries[12] = translation.entries[0];
    matrix.entries[13] = translation.entries[1];
    matrix.entries[14] = translation.entries[2];

    return matrix;
}

mat4 create_matrix_scaling(vec3 scaling) {
    mat4 matrix = mat4::identity();

    matrix.entries[0] = scaling.entries[0];
    matrix.entries[5] = scaling.entries[1];
    matrix.entries[10] = scaling.entries[2];

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