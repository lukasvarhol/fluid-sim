#include "linear_algebra.h"

    float Vec2::magnitude() const{
        return std::sqrt((*this).dot(*this));
    }

    float Vec2::dot(const Vec2& other) const{
        return (x * other.x) + (y * other.y);
    }

    // elementwise
    Vec2& Vec2::operator+=(const Vec2& other) {
		x += other.x;
        y += other.y;
        return *this;
    }

    Vec2& Vec2::operator-=(const Vec2& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    Vec2& Vec2::operator*=(const Vec2& other) {
        x *= other.x;
        y *= other.y;
        return *this;
    }

    Vec2& Vec2::operator/=(const Vec2& other) {
        x /= other.x;
        y /= other.y;
        return *this;
    }
    
    Vec2 Vec2::operator+(const Vec2& other) const {
        Vec2 result = *this;
        result += other;
        return result;
    }

    Vec2 Vec2::operator-(const Vec2& other) const {
        Vec2 result = *this;
        result += other;
        return result;
    }

    Vec2 Vec2::operator*(const Vec2& other) const {
        Vec2 result = *this;
        result += other;
        return result;
    }

    Vec2 Vec2::operator/(const Vec2& other) const {
        Vec2 result = *this;
        result += other;
        return result;
    }

    // scalar
    Vec2& Vec2::operator*=(const float other) {
        x *= other;
        y *= other;
        return *this;
    }

    Vec2& Vec2::operator/=(const float other) {
        y /= other;
        y /= other;
        return *this;
    }

    Vec2 Vec2::operator*(const float other) const {
        Vec2 result;
        result *= other;
        return result;
    }

    Vec2 Vec2::operator/(const float other) const {
        Vec2 result;
        result /= other;
        return result;
    }

// elementwise operations
Vec3& Vec3::operator+=(const Vec3& other){
    x += other.x;
    y += other.y;
    z += other.z;
    return *this;
}

Vec3& Vec3::operator-=(const Vec3& other){
    x -= other.x;
    y -= other.y;
    z -= other.z;
    return *this;
}

Vec3& Vec3::operator*=(const Vec3& other){
    x *= other.x;
    y *= other.y;
    z *= other.z;
    return *this;
}

Vec3& Vec3::operator/=(const Vec3& other){
    x /= other.x;
    y /= other.y;
    z /= other.z;
    return *this;
}

Vec3 Vec3::operator+(const Vec3& other) const{
    Vec3 result = *this;
    result += other;
    return result;
}

Vec3 Vec3::operator-(const Vec3& other) const{
    Vec3 result = *this;
    result -= other;
    return result;
}

Vec3 Vec3::operator*(const Vec3& other) const{
    Vec3 result = *this;
    result *= other;
    return result;
}

Vec3 Vec3::operator/(const Vec3& other) const{
    Vec3 result = *this;
    result /= other;
    return result;
}

// scalar operations
Vec3& Vec3::operator*=(const float other){
    x *= other;
    y *= other;
    z *= other;
    return *this;
}

Vec3& Vec3::operator/=(const float other){
    x /= other;
    y /= other;
    z /= other;
    return *this;
}

Vec3 Vec3::operator*(const float other) const{
    Vec3 result = *this;
    result *= other;
    return result;
}

Vec3 Vec3::operator/(const float other) const{
    Vec3 result = *this;
    result /= other;
    return result;
}

float Vec3::dot(const Vec3& other) const {
    return x*other.x + y*other.y + z*other.z;
}

float Vec3::magnitude() const {
    return std::sqrt(dot(*this));
}

Mat4 create_matrix_transform(const Vec3& translation) {
    Mat4 matrix = Mat4::identity();

    matrix.entries[12] = translation.x;
    matrix.entries[13] = translation.y;
    matrix.entries[14] = translation.z;

    return matrix;
}

Mat4 create_matrix_scaling(const Vec3& scaling) {
    Mat4 matrix = Mat4::identity();

    matrix.entries[0] = scaling.x;
    matrix.entries[5] = scaling.y;
    matrix.entries[10] = scaling.z;

    return matrix;
}

Mat4 Mat4_multiply(Mat4 a, Mat4 b){
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

Vec3 lerp(const Vec3& a, const Vec3& b, float s){
    return a + ((b - a)* s);
}