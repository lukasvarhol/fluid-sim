#pragma once
#include <vector> 
#include <iostream>
#include <cassert>
#include <cmath>

constexpr float PI = 3.14159265358979323846f;

struct Mat4 {
  float entries[16];

  static Mat4 Identity() {
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

  float Magnitude() const { return std::sqrt(x*x + y*y); }
  float Dot(const Vec2& o) const { return x*o.x + y*o.y; }

  Vec2& operator+=(const Vec2& o) { x+=o.x; y+=o.y; return *this; }
  Vec2& operator-=(const Vec2& o) { x-=o.x; y-=o.y; return *this; }
  Vec2& operator*=(const Vec2& o) { x*=o.x; y*=o.y; return *this; }
  Vec2& operator/=(const Vec2& o) { x/=o.x; y/=o.y; return *this; }

  Vec2 operator+(const Vec2& o) const { return {x+o.x, y+o.y}; }
  Vec2 operator-(const Vec2& o) const { return {x-o.x, y-o.y}; }
  Vec2 operator*(const Vec2& o) const { return {x*o.x, y*o.y}; }
  Vec2 operator/(const Vec2& o) const { return {x/o.x, y/o.y}; }

  Vec2& operator*=(float s) { x*=s; y*=s; return *this; }
  Vec2& operator/=(float s) { float r=1.f/s; x*=r; y*=r; return *this; }
  Vec2 operator*(float s) const { return {x*s, y*s}; }
  Vec2 operator/(float s) const { float r=1.f/s; return {x*r, y*r}; }
};

struct Vec3 {
  float x, y, z;

  float Magnitude() const { return std::sqrt(x*x + y*y + z*z); }
  float Dot(const Vec3& o) const { return x*o.x + y*o.y + z*o.z; }

  Vec3& operator+=(const Vec3& o) { x+=o.x; y+=o.y; z+=o.z; return *this; }
  Vec3& operator-=(const Vec3& o) { x-=o.x; y-=o.y; z-=o.z; return *this; }
  Vec3& operator*=(const Vec3& o) { x*=o.x; y*=o.y; z*=o.z; return *this; }
  Vec3& operator/=(const Vec3& o) { x/=o.x; y/=o.y; z/=o.z; return *this; }

  Vec3 operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
  Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
  Vec3 operator*(const Vec3& o) const { return {x*o.x, y*o.y, z*o.z}; }
  Vec3 operator/(const Vec3& o) const { return {x/o.x, y/o.y, z/o.z}; }

  Vec3& operator*=(float s) { x*=s; y*=s; z*=s; return *this; }
  Vec3& operator/=(float s) { float r=1.f/s; x*=r; y*=r; z*=r; return *this; }
  Vec3 operator*(float s) const { return {x*s, y*s, z*s}; }
  Vec3 operator/(float s) const { float r = 1.f / s; return {x * r, y * r, z * r}; }
};

inline Vec3 Normalize(const Vec3& v)
{
  return v / v.Magnitude();
}

inline Vec3 Cross(const Vec3& a, const Vec3& b)
{
  return {
   a.y * b.z - a.z * b.y,
   a.z * b.x - a.x * b.z,
    a.x * b.y - a.y * b.x
  };
}

inline Mat4 CreateMatrixTransform(const Vec3 &translation)
{
  Mat4 matrix = Mat4::Identity();

  matrix.entries[12] = translation.x;
  matrix.entries[13] = translation.y;
  matrix.entries[14] = translation.z;
  return matrix;
}


inline Mat4 CreateMatrixScaling(const Vec3 &scaling)
{
  Mat4 matrix = Mat4::Identity();

  matrix.entries[0] = scaling.x;
  matrix.entries[5] = scaling.y;
  matrix.entries[10] = scaling.z;

  return matrix;
}

inline Mat4 LookAt(const Vec3 &eye, const Vec3& centre, const Vec3& up_) {
  Vec3 fwd = Normalize(centre - eye);
  Vec3 rght = Normalize(Cross(fwd, up_));
  Vec3 up = Cross(rght, fwd);

  Mat4 m = Mat4::Identity();
  m.entries[0] = rght.x;
  m.entries[4] = rght.y;
  m.entries[8] = rght.z;

  m.entries[1] = up.x;
  m.entries[5] = up.y;
  m.entries[9] = up.z;

  m.entries[2] = -fwd.x;
  m.entries[6] = -fwd.y;
  m.entries[10] = -fwd.z;

  m.entries[12] = -rght.Dot(eye);
  m.entries[13] = -up.Dot(eye);
  m.entries[14] = fwd.Dot(eye);
  return m;
}

inline Mat4 Perspective(float fovY_rad, float aspect, float near, float far) {
  float t = tan(fovY_rad / 2.0f);
  Mat4 m = {};
  m.entries[0]  = 1.0f / (aspect * t);
  m.entries[5]  = 1.0f / t;
  m.entries[10] = -(far + near) / (far - near);
  m.entries[11] = -1.0f;
  m.entries[14] = -(2.0f * far * near) / (far - near);
  return m;
}

inline Mat4 InverseView(const Mat4 &m) {
  Mat4 inv = Mat4::Identity();
  inv.entries[0]  = m.entries[0];
  inv.entries[1]  = m.entries[4];
  inv.entries[2]  = m.entries[8];
  inv.entries[4]  = m.entries[1];
  inv.entries[5]  = m.entries[5];
  inv.entries[6]  = m.entries[9];
  inv.entries[8]  = m.entries[2];
  inv.entries[9]  = m.entries[6];
  inv.entries[10] = m.entries[10];

  // recompute translation: -R^T * t
  inv.entries[12] = -(m.entries[0]*m.entries[12] + m.entries[1]*m.entries[13] + m.entries[2]*m.entries[14]);
  inv.entries[13] = -(m.entries[4]*m.entries[12] + m.entries[5]*m.entries[13] + m.entries[6]*m.entries[14]);
  inv.entries[14] = -(m.entries[8]*m.entries[12] + m.entries[9]*m.entries[13] + m.entries[10]*m.entries[14]);
  return inv;
}
  

inline Mat4 Mat4Multiply(Mat4 a, Mat4 b)
{
  Mat4 result;

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

inline Vec3 Lerp(const Vec3 &a, const Vec3 &b, float s)
{
  return a + ((b - a) * s);
}
