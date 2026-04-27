#include "linear_algebra.h"

// elementwise operations
Vec3 &Vec3::operator+=(const Vec3 &other)
{
    x += other.x;
    y += other.y;
    z += other.z;
    return *this;
}

Vec3 &Vec3::operator-=(const Vec3 &other)
{
    x -= other.x;
    y -= other.y;
    z -= other.z;
    return *this;
}

Vec3 &Vec3::operator*=(const Vec3 &other)
{
    x *= other.x;
    y *= other.y;
    z *= other.z;
    return *this;
}

Vec3 &Vec3::operator/=(const Vec3 &other)
{
    x /= other.x;
    y /= other.y;
    z /= other.z;
    return *this;
}

Vec3 Vec3::operator+(const Vec3 &other) const
{
    Vec3 result = *this;
    result += other;
    return result;
}

Vec3 Vec3::operator-(const Vec3 &other) const
{
    Vec3 result = *this;
    result -= other;
    return result;
}

Vec3 Vec3::operator*(const Vec3 &other) const
{
    Vec3 result = *this;
    result *= other;
    return result;
}

Vec3 Vec3::operator/(const Vec3 &other) const
{
    Vec3 result = *this;
    result /= other;
    return result;
}

// scalar operations
Vec3 &Vec3::operator*=(const float other)
{
    x *= other;
    y *= other;
    z *= other;
    return *this;
}

Vec3 &Vec3::operator/=(const float other)
{
    x /= other;
    y /= other;
    z /= other;
    return *this;
}

Vec3 Vec3::operator*(const float other) const
{
    Vec3 result = *this;
    result *= other;
    return result;
}

Vec3 Vec3::operator/(const float other) const
{
    Vec3 result = *this;
    result /= other;
    return result;
}

float Vec3::dot(const Vec3 &other) const
{
    return x * other.x + y * other.y + z * other.z;
}

float Vec3::magnitude() const { return std::sqrt(dot(*this)); }

Vec3 normalize(const Vec3& v)
{
    return v / v.magnitude();
}

Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

Mat4 create_matrix_transform(const Vec3 &translation)
{
    Mat4 matrix = Mat4::identity();

    matrix.entries[12] = translation.x;
    matrix.entries[13] = translation.y;
    matrix.entries[14] = translation.z;

    return matrix;
}


Mat4 create_matrix_scaling(const Vec3 &scaling)
{
    Mat4 matrix = Mat4::identity();

    matrix.entries[0] = scaling.x;
    matrix.entries[5] = scaling.y;
    matrix.entries[10] = scaling.z;

    return matrix;
}

Mat4 lookAt(const Vec3 &eye, const Vec3& centre, const Vec3& up_) {
  Vec3 fwd = normalize(centre - eye);
  Vec3 rght = normalize(cross(fwd, up_));
  Vec3 up = cross(rght, fwd);

  Mat4 m = Mat4::identity();
  m.entries[0] = rght.x;
  m.entries[4] = rght.y;
  m.entries[8] = rght.z;

  m.entries[1] = up.x;
  m.entries[5] = up.y;
  m.entries[9] = up.z;

  m.entries[2] = -fwd.x;
  m.entries[6] = -fwd.y;
  m.entries[10] = -fwd.z;

  m.entries[12] = -rght.dot(eye);
  m.entries[13] = -up.dot(eye);
  m.entries[14] = fwd.dot(eye);
  return m;
}

Mat4 perspective(float fovY_rad, float aspect, float near, float far) {
  float t = tan(fovY_rad / 2.0f);
  Mat4 m = {};
  m.entries[0]  = 1.0f / (aspect * t);
  m.entries[5]  = 1.0f / t;
  m.entries[10] = -(far + near) / (far - near);
  m.entries[11] = -1.0f;
  m.entries[14] = -(2.0f * far * near) / (far - near);
  return m;
}

Mat4 inverse_view(const Mat4 &m) {
    Mat4 inv = Mat4::identity();
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
  

Mat4 Mat4_multiply(Mat4 a, Mat4 b)
{
    Mat4 result;

    for (int col = 0; col < 4; ++col)
    {
        for (int row = 0; row < 4; ++row)
        {

            result.entries[col * 4 + row] =
                a.entries[0 * 4 + row] * b.entries[col * 4 + 0] +
                a.entries[1 * 4 + row] * b.entries[col * 4 + 1] +
                a.entries[2 * 4 + row] * b.entries[col * 4 + 2] +
                a.entries[3 * 4 + row] * b.entries[col * 4 + 3];
        }
    }
    return result;
}

Vec3 lerp(const Vec3 &a, const Vec3 &b, float s)
{
    return a + ((b - a) * s);
}
