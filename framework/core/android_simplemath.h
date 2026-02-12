/*****************************************************************/ /**
                                                                     * \file   android_simplemath.h
                                                                     * \brief  Android-compatible SimpleMath wrapper
                                                                     *         Provides SimpleMath-compatible API for
                                                                     *Android platform
                                                                     *
                                                                     * \author Auto-generated
                                                                     * \date   2024
                                                                     *********************************************************************/

#pragma once

#ifdef __ANDROID__

#include <algorithm>
#include <cmath>
#include <cstring>

namespace DirectX
{
namespace SimpleMath
{
// Forward declarations
struct Vector2;
struct Vector3;
struct Vector4;
struct Matrix;
struct Quaternion;
struct Plane;
struct Ray;
struct Color;

//------------------------------------------------------------------------------
// 2D Vector
struct Vector2
{
    float x, y;

    Vector2() noexcept : x(0.0f), y(0.0f)
    {
    }
    constexpr explicit Vector2(float v) noexcept : x(v), y(v)
    {
    }
    constexpr Vector2(float ix, float iy) noexcept : x(ix), y(iy)
    {
    }
    explicit Vector2(const float *pArray) noexcept : x(pArray[0]), y(pArray[1])
    {
    }

    Vector2(const Vector2 &) = default;
    Vector2 &operator=(const Vector2 &) = default;

    bool operator==(const Vector2 &V) const noexcept
    {
        return x == V.x && y == V.y;
    }
    bool operator!=(const Vector2 &V) const noexcept
    {
        return x != V.x || y != V.y;
    }

    Vector2 &operator+=(const Vector2 &V) noexcept
    {
        x += V.x;
        y += V.y;
        return *this;
    }
    Vector2 &operator-=(const Vector2 &V) noexcept
    {
        x -= V.x;
        y -= V.y;
        return *this;
    }
    Vector2 &operator*=(const Vector2 &V) noexcept
    {
        x *= V.x;
        y *= V.y;
        return *this;
    }
    Vector2 &operator*=(float S) noexcept
    {
        x *= S;
        y *= S;
        return *this;
    }
    Vector2 &operator/=(float S) noexcept
    {
        float inv = 1.0f / S;
        x *= inv;
        y *= inv;
        return *this;
    }

    Vector2 operator+() const noexcept
    {
        return *this;
    }
    Vector2 operator-() const noexcept
    {
        return Vector2(-x, -y);
    }

    float Length() const noexcept
    {
        return std::sqrt(x * x + y * y);
    }
    float LengthSquared() const noexcept
    {
        return x * x + y * y;
    }
    float Dot(const Vector2 &V) const noexcept
    {
        return x * V.x + y * V.y;
    }

    void Normalize() noexcept
    {
        float len = Length();
        if (len > 0.0f)
        {
            float inv = 1.0f / len;
            x *= inv;
            y *= inv;
        }
    }

    void Normalize(Vector2 &result) const noexcept
    {
        float len = Length();
        if (len > 0.0f)
        {
            float inv = 1.0f / len;
            result.x = x * inv;
            result.y = y * inv;
        }
        else
        {
            result.x = 0.0f;
            result.y = 0.0f;
        }
    }

    static const Vector2 Zero;
    static const Vector2 One;
    static const Vector2 UnitX;
    static const Vector2 UnitY;
};

//------------------------------------------------------------------------------
// 3D Vector
struct Vector3
{
    float x, y, z;

    Vector3() noexcept : x(0.0f), y(0.0f), z(0.0f)
    {
    }
    constexpr explicit Vector3(float v) noexcept : x(v), y(v), z(v)
    {
    }
    constexpr Vector3(float ix, float iy, float iz) noexcept : x(ix), y(iy), z(iz)
    {
    }
    explicit Vector3(const float *pArray) noexcept : x(pArray[0]), y(pArray[1]), z(pArray[2])
    {
    }

    Vector3(const Vector3 &) = default;
    Vector3 &operator=(const Vector3 &) = default;

    bool operator==(const Vector3 &V) const noexcept
    {
        return x == V.x && y == V.y && z == V.z;
    }
    bool operator!=(const Vector3 &V) const noexcept
    {
        return x != V.x || y != V.y || z != V.z;
    }

    Vector3 &operator+=(const Vector3 &V) noexcept
    {
        x += V.x;
        y += V.y;
        z += V.z;
        return *this;
    }
    Vector3 &operator-=(const Vector3 &V) noexcept
    {
        x -= V.x;
        y -= V.y;
        z -= V.z;
        return *this;
    }
    Vector3 &operator*=(const Vector3 &V) noexcept
    {
        x *= V.x;
        y *= V.y;
        z *= V.z;
        return *this;
    }
    Vector3 &operator*=(float S) noexcept
    {
        x *= S;
        y *= S;
        z *= S;
        return *this;
    }
    Vector3 &operator/=(float S) noexcept
    {
        float inv = 1.0f / S;
        x *= inv;
        y *= inv;
        z *= inv;
        return *this;
    }

    Vector3 operator+() const noexcept
    {
        return *this;
    }
    Vector3 operator-() const noexcept
    {
        return Vector3(-x, -y, -z);
    }

    Vector3 operator+(const Vector3 &V) const noexcept
    {
        return Vector3(x + V.x, y + V.y, z + V.z);
    }
    Vector3 operator-(const Vector3 &V) const noexcept
    {
        return Vector3(x - V.x, y - V.y, z - V.z);
    }
    Vector3 operator*(const Vector3 &V) const noexcept
    {
        return Vector3(x * V.x, y * V.y, z * V.z);
    }
    Vector3 operator*(float S) const noexcept
    {
        return Vector3(x * S, y * S, z * S);
    }
    Vector3 operator/(float S) const noexcept
    {
        float inv = 1.0f / S;
        return Vector3(x * inv, y * inv, z * inv);
    }

    float Length() const noexcept
    {
        return std::sqrt(x * x + y * y + z * z);
    }
    float LengthSquared() const noexcept
    {
        return x * x + y * y + z * z;
    }
    float Dot(const Vector3 &V) const noexcept
    {
        return x * V.x + y * V.y + z * V.z;
    }

    Vector3 Cross(const Vector3 &V) const noexcept
    {
        return Vector3(y * V.z - z * V.y, z * V.x - x * V.z, x * V.y - y * V.x);
    }

    void Normalize() noexcept
    {
        float len = Length();
        if (len > 0.0f)
        {
            float inv = 1.0f / len;
            x *= inv;
            y *= inv;
            z *= inv;
        }
    }

    void Normalize(Vector3 &result) const noexcept
    {
        float len = Length();
        if (len > 0.0f)
        {
            float inv = 1.0f / len;
            result.x = x * inv;
            result.y = y * inv;
            result.z = z * inv;
        }
        else
        {
            result.x = 0.0f;
            result.y = 0.0f;
            result.z = 0.0f;
        }
    }

    static const Vector3 Zero;
    static const Vector3 One;
    static const Vector3 UnitX;
    static const Vector3 UnitY;
    static const Vector3 UnitZ;
    static const Vector3 Up;
    static const Vector3 Down;
    static const Vector3 Right;
    static const Vector3 Left;
    static const Vector3 Forward;
    static const Vector3 Backward;
};

//------------------------------------------------------------------------------
// 4D Vector
struct Vector4
{
    float x, y, z, w;

    Vector4() noexcept : x(0.0f), y(0.0f), z(0.0f), w(0.0f)
    {
    }
    constexpr explicit Vector4(float v) noexcept : x(v), y(v), z(v), w(v)
    {
    }
    constexpr Vector4(float ix, float iy, float iz, float iw) noexcept : x(ix), y(iy), z(iz), w(iw)
    {
    }
    explicit Vector4(const float *pArray) noexcept : x(pArray[0]), y(pArray[1]), z(pArray[2]), w(pArray[3])
    {
    }

    Vector4(const Vector4 &) = default;
    Vector4 &operator=(const Vector4 &) = default;

    bool operator==(const Vector4 &V) const noexcept
    {
        return x == V.x && y == V.y && z == V.z && w == V.w;
    }
    bool operator!=(const Vector4 &V) const noexcept
    {
        return x != V.x || y != V.y || z != V.z || w != V.w;
    }

    Vector4 &operator+=(const Vector4 &V) noexcept
    {
        x += V.x;
        y += V.y;
        z += V.z;
        w += V.w;
        return *this;
    }
    Vector4 &operator-=(const Vector4 &V) noexcept
    {
        x -= V.x;
        y -= V.y;
        z -= V.z;
        w -= V.w;
        return *this;
    }
    Vector4 &operator*=(const Vector4 &V) noexcept
    {
        x *= V.x;
        y *= V.y;
        z *= V.z;
        w *= V.w;
        return *this;
    }
    Vector4 &operator*=(float S) noexcept
    {
        x *= S;
        y *= S;
        z *= S;
        w *= S;
        return *this;
    }
    Vector4 &operator/=(float S) noexcept
    {
        float inv = 1.0f / S;
        x *= inv;
        y *= inv;
        z *= inv;
        w *= inv;
        return *this;
    }

    Vector4 operator+() const noexcept
    {
        return *this;
    }
    Vector4 operator-() const noexcept
    {
        return Vector4(-x, -y, -z, -w);
    }

    float Length() const noexcept
    {
        return std::sqrt(x * x + y * y + z * z + w * w);
    }
    float LengthSquared() const noexcept
    {
        return x * x + y * y + z * z + w * w;
    }
    float Dot(const Vector4 &V) const noexcept
    {
        return x * V.x + y * V.y + z * V.z + w * V.w;
    }

    void Normalize() noexcept
    {
        float len = Length();
        if (len > 0.0f)
        {
            float inv = 1.0f / len;
            x *= inv;
            y *= inv;
            z *= inv;
            w *= inv;
        }
    }

    void Normalize(Vector4 &result) const noexcept
    {
        float len = Length();
        if (len > 0.0f)
        {
            float inv = 1.0f / len;
            result.x = x * inv;
            result.y = y * inv;
            result.z = z * inv;
            result.w = w * inv;
        }
        else
        {
            result.x = 0.0f;
            result.y = 0.0f;
            result.z = 0.0f;
            result.w = 0.0f;
        }
    }

    static const Vector4 Zero;
    static const Vector4 One;
    static const Vector4 UnitX;
    static const Vector4 UnitY;
    static const Vector4 UnitZ;
    static const Vector4 UnitW;
};

//------------------------------------------------------------------------------
// Matrix (4x4)
struct Matrix
{
    float m[4][4];

    Matrix() noexcept
    {
        std::memset(m, 0, sizeof(m));
    }
    Matrix(const Matrix &) = default;
    Matrix &operator=(const Matrix &) = default;

    float &operator()(size_t row, size_t col) noexcept
    {
        return m[row][col];
    }
    const float &operator()(size_t row, size_t col) const noexcept
    {
        return m[row][col];
    }

    Matrix operator*(const Matrix &M) const noexcept
    {
        Matrix result;
        for (size_t i = 0; i < 4; ++i)
        {
            for (size_t j = 0; j < 4; ++j)
            {
                result.m[i][j] = m[i][0] * M.m[0][j] + m[i][1] * M.m[1][j] + m[i][2] * M.m[2][j] + m[i][3] * M.m[3][j];
            }
        }
        return result;
    }

    Matrix &operator*=(const Matrix &M) noexcept
    {
        *this = *this * M;
        return *this;
    }

    static Matrix CreateLookAt(const Vector3 &eye, const Vector3 &target, const Vector3 &up) noexcept
    {
        Vector3 zaxis = target - eye;
        zaxis.Normalize();
        Vector3 xaxis = up.Cross(zaxis);
        xaxis.Normalize();
        Vector3 yaxis = zaxis.Cross(xaxis);

        Matrix result;
        result.m[0][0] = xaxis.x;
        result.m[0][1] = yaxis.x;
        result.m[0][2] = zaxis.x;
        result.m[0][3] = 0.0f;
        result.m[1][0] = xaxis.y;
        result.m[1][1] = yaxis.y;
        result.m[1][2] = zaxis.y;
        result.m[1][3] = 0.0f;
        result.m[2][0] = xaxis.z;
        result.m[2][1] = yaxis.z;
        result.m[2][2] = zaxis.z;
        result.m[2][3] = 0.0f;
        result.m[3][0] = -xaxis.Dot(eye);
        result.m[3][1] = -yaxis.Dot(eye);
        result.m[3][2] = -zaxis.Dot(eye);
        result.m[3][3] = 1.0f;
        return result;
    }

    static Matrix CreatePerspectiveFieldOfView(float fov, float aspectRatio, float nearPlane, float farPlane) noexcept
    {
        float tanHalfFov = std::tan(fov * 0.5f);
        float range = farPlane - nearPlane;

        Matrix result;
        std::memset(&result, 0, sizeof(result));
        result.m[0][0] = 1.0f / (aspectRatio * tanHalfFov);
        result.m[1][1] = 1.0f / tanHalfFov;
        result.m[2][2] = -(farPlane + nearPlane) / range;
        result.m[2][3] = -1.0f;
        result.m[3][2] = -(2.0f * farPlane * nearPlane) / range;
        return result;
    }

    static Matrix CreatePerspective(float width, float height, float nearPlane, float farPlane) noexcept
    {
        float range = farPlane - nearPlane;
        Matrix result;
        std::memset(&result, 0, sizeof(result));
        result.m[0][0] = 2.0f * nearPlane / width;
        result.m[1][1] = 2.0f * nearPlane / height;
        result.m[2][2] = -(farPlane + nearPlane) / range;
        result.m[2][3] = -1.0f;
        result.m[3][2] = -(2.0f * farPlane * nearPlane) / range;
        return result;
    }

    static Matrix CreateOrthographic(float width, float height, float nearPlane, float farPlane) noexcept
    {
        float range = farPlane - nearPlane;
        Matrix result;
        std::memset(&result, 0, sizeof(result));
        result.m[0][0] = 2.0f / width;
        result.m[1][1] = 2.0f / height;
        result.m[2][2] = -2.0f / range;
        result.m[3][0] = -1.0f;
        result.m[3][1] = -1.0f;
        result.m[3][2] = -(farPlane + nearPlane) / range;
        result.m[3][3] = 1.0f;
        return result;
    }

    static const Matrix Identity;
};

//------------------------------------------------------------------------------
// Quaternion
struct Quaternion
{
    float x, y, z, w;

    Quaternion() noexcept : x(0.0f), y(0.0f), z(0.0f), w(1.0f)
    {
    }
    constexpr Quaternion(float ix, float iy, float iz, float iw) noexcept : x(ix), y(iy), z(iz), w(iw)
    {
    }

    Quaternion(const Quaternion &) = default;
    Quaternion &operator=(const Quaternion &) = default;
};

//------------------------------------------------------------------------------
// Plane
struct Plane
{
    Vector3 normal;
    float d;

    Plane() noexcept : normal(), d(0.0f)
    {
    }
    Plane(const Vector3 &n, float distance) noexcept : normal(n), d(distance)
    {
    }

    Plane(const Plane &) = default;
    Plane &operator=(const Plane &) = default;
};

//------------------------------------------------------------------------------
// Ray
struct Ray
{
    Vector3 position;
    Vector3 direction;

    Ray() noexcept : position(), direction()
    {
    }
    Ray(const Vector3 &pos, const Vector3 &dir) noexcept : position(pos), direction(dir)
    {
    }

    Ray(const Ray &) = default;
    Ray &operator=(const Ray &) = default;
};

//------------------------------------------------------------------------------
// Color
struct Color
{
    float r, g, b, a;

    Color() noexcept : r(0.0f), g(0.0f), b(0.0f), a(1.0f)
    {
    }
    constexpr Color(float ir, float ig, float ib, float ia = 1.0f) noexcept : r(ir), g(ig), b(ib), a(ia)
    {
    }

    Color(const Color &) = default;
    Color &operator=(const Color &) = default;
};

// Static constants
inline const Vector2 Vector2::Zero = {0.0f, 0.0f};
inline const Vector2 Vector2::One = {1.0f, 1.0f};
inline const Vector2 Vector2::UnitX = {1.0f, 0.0f};
inline const Vector2 Vector2::UnitY = {0.0f, 1.0f};

inline const Vector3 Vector3::Zero = {0.0f, 0.0f, 0.0f};
inline const Vector3 Vector3::One = {1.0f, 1.0f, 1.0f};
inline const Vector3 Vector3::UnitX = {1.0f, 0.0f, 0.0f};
inline const Vector3 Vector3::UnitY = {0.0f, 1.0f, 0.0f};
inline const Vector3 Vector3::UnitZ = {0.0f, 0.0f, 1.0f};
inline const Vector3 Vector3::Up = {0.0f, 1.0f, 0.0f};
inline const Vector3 Vector3::Down = {0.0f, -1.0f, 0.0f};
inline const Vector3 Vector3::Right = {1.0f, 0.0f, 0.0f};
inline const Vector3 Vector3::Left = {-1.0f, 0.0f, 0.0f};
inline const Vector3 Vector3::Forward = {0.0f, 0.0f, -1.0f};
inline const Vector3 Vector3::Backward = {0.0f, 0.0f, 1.0f};

inline const Vector4 Vector4::Zero = {0.0f, 0.0f, 0.0f, 0.0f};
inline const Vector4 Vector4::One = {1.0f, 1.0f, 1.0f, 1.0f};
inline const Vector4 Vector4::UnitX = {1.0f, 0.0f, 0.0f, 0.0f};
inline const Vector4 Vector4::UnitY = {0.0f, 1.0f, 0.0f, 0.0f};
inline const Vector4 Vector4::UnitZ = {0.0f, 0.0f, 1.0f, 0.0f};
inline const Vector4 Vector4::UnitW = {0.0f, 0.0f, 0.0f, 1.0f};

inline const Matrix Matrix::Identity = []() {
    Matrix tm;
    std::memset(&tm, 0, sizeof(tm));
    tm.m[0][0] = 1.0f;
    tm.m[1][1] = 1.0f;
    tm.m[2][2] = 1.0f;
    tm.m[3][3] = 1.0f;
    return tm;
}();
} // namespace SimpleMath
} // namespace DirectX

// BoundingFrustum placeholder (minimal implementation)
namespace DirectX
{
struct BoundingFrustum
{
    // Minimal implementation - add methods as needed
    BoundingFrustum() = default;
};
} // namespace DirectX

#endif // __ANDROID__
