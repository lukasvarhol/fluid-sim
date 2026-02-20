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

struct vec3 {
    float entries[3];
};

mat4 create_matrix_transform(vec3 translation);
mat4 create_matrix_scaling(vec3 scaling);
mat4 mat4_multiply(mat4 a, mat4 b);