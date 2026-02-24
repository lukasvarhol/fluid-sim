#pragma once
#include <vector> 
#include <iostream>
#include <cassert>
#include <cmath>

constexpr float PI = 3.14159265358979323846f;

struct Mat4 {
    float entries[16];

    static Mat4 identity() {
        Mat4 m{};
        m.entries[0]  = 1.0f;
        m.entries[5]  = 1.0f;
        m.entries[10] = 1.0f;
        m.entries[15] = 1.0f;
        return m;
    }
};

struct Vec3 {
    float x, y ,z;

    float magnitude() const;
    float dot(const Vec3& other) const; 

    // elementwise
    Vec3& operator+=(const Vec3& other);
    Vec3& operator-=(const Vec3& other);
    Vec3& operator*=(const Vec3& other); 
    Vec3& operator/=(const Vec3& other);

    Vec3 operator+(const Vec3& other) const;
    Vec3 operator-(const Vec3& other) const;
    Vec3 operator*(const Vec3& other) const;
    Vec3 operator/(const Vec3& other) const;

    // scalar
    Vec3& operator*=(const float other); 
    Vec3& operator/=(const float other);
    Vec3 operator*(const float other) const;
    Vec3 operator/(const float other) const;
};

Mat4 create_matrix_transform(const Vec3& translation);
Mat4 create_matrix_scaling(const Vec3& scaling);
Mat4 Mat4_multiply(Mat4 a, Mat4 b);
Vec3 lerp(const Vec3& a, const Vec3& b, float s);