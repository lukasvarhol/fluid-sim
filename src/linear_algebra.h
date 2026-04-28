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

struct Vec2 {
    float x, y;

    [[nodiscard]] float magnitude() const { return std::sqrt(x*x + y*y); }
    [[nodiscard]] float dot(const Vec2& o) const { return x*o.x + y*o.y; }

    Vec2& operator+=(const Vec2& o) { x+=o.x; y+=o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x-=o.x; y-=o.y; return *this; }
    Vec2& operator*=(const Vec2& o) { x*=o.x; y*=o.y; return *this; }
    Vec2& operator/=(const Vec2& o) { x/=o.x; y/=o.y; return *this; }

    [[nodiscard]] Vec2 operator+(const Vec2& o) const { return {x+o.x, y+o.y}; }
    [[nodiscard]] Vec2 operator-(const Vec2& o) const { return {x-o.x, y-o.y}; }
    [[nodiscard]] Vec2 operator*(const Vec2& o) const { return {x*o.x, y*o.y}; }
    [[nodiscard]] Vec2 operator/(const Vec2& o) const { return {x/o.x, y/o.y}; }

    Vec2& operator*=(float s) { x*=s; y*=s; return *this; }
    Vec2& operator/=(float s) { float r=1.f/s; x*=r; y*=r; return *this; }
    [[nodiscard]] Vec2 operator*(float s) const { return {x*s, y*s}; }
    [[nodiscard]] Vec2 operator/(float s) const { float r=1.f/s; return {x*r, y*r}; }
};

struct Vec3 {
  float x, y, z;

  [[nodiscard]] float magnitude() const { return std::sqrt(x*x + y*y + z*z); }
  [[nodiscard]] float dot(const Vec3& o) const { return x*o.x + y*o.y + z*o.z; }

  Vec3& operator+=(const Vec3& o) { x+=o.x; y+=o.y; z+=o.z; return *this; }
  Vec3& operator-=(const Vec3& o) { x-=o.x; y-=o.y; z-=o.z; return *this; }
  Vec3& operator*=(const Vec3& o) { x*=o.x; y*=o.y; z*=o.z; return *this; }
  Vec3& operator/=(const Vec3& o) { x/=o.x; y/=o.y; z/=o.z; return *this; }

  [[nodiscard]] Vec3 operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
  [[nodiscard]] Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
  [[nodiscard]] Vec3 operator*(const Vec3& o) const { return {x*o.x, y*o.y, z*o.z}; }
  [[nodiscard]] Vec3 operator/(const Vec3& o) const { return {x/o.x, y/o.y, z/o.z}; }

  Vec3& operator*=(float s) { x*=s; y*=s; z*=s; return *this; }
  Vec3& operator/=(float s) { float r=1.f/s; x*=r; y*=r; z*=r; return *this; }
  [[nodiscard]] Vec3 operator*(float s) const { return {x*s, y*s, z*s}; }
  [[nodiscard]] Vec3 operator/(float s) const { float r = 1.f / s; return {x * r, y * r, z * r}; }
};

Vec3 normalize(const Vec3& v);
Vec3 cross(const Vec3& a, const Vec3& b);

Mat4 create_matrix_transform(const Vec3& translation);
Mat4 create_matrix_scaling(const Vec3 &scaling);
Mat4 lookAt(const Vec3 &eye, const Vec3 &centre, const Vec3 &up_);
Mat4 perspective(float fovY_rad, float aspect, float near, float far);
Mat4 inverse_view(const Mat4& m);
Mat4 Mat4_multiply(Mat4 a, Mat4 b);
Vec3 lerp(const Vec3& a, const Vec3& b, float s);
