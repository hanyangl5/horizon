#pragma once

#include <cmath>
#include <cstddef>
#include <cstring>
#include <type_traits>

#include "h3d_math.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-anonymous-struct"
#pragma clang diagnostic ignored "-Wnested-anon-types"
#endif

namespace DirectX
{
namespace SimpleMath
{

struct Vector2
{
    float x;
    float y;

    Vector2() noexcept : x(0.0f), y(0.0f) {}
    constexpr explicit Vector2(float v) noexcept : x(v), y(v) {}
    constexpr Vector2(float ix, float iy) noexcept : x(ix), y(iy) {}
    explicit Vector2(const float *p_array) noexcept : x(p_array[0]), y(p_array[1]) {}

    Vector2(const Vector2 &) = default;
    Vector2 &operator=(const Vector2 &) = default;

    bool operator==(const Vector2 &v) const noexcept { return x == v.x && y == v.y; }
    bool operator!=(const Vector2 &v) const noexcept { return !(*this == v); }

    Vector2 &operator+=(const Vector2 &v) noexcept
    {
        x += v.x;
        y += v.y;
        return *this;
    }

    Vector2 &operator-=(const Vector2 &v) noexcept
    {
        x -= v.x;
        y -= v.y;
        return *this;
    }

    Vector2 &operator*=(const Vector2 &v) noexcept
    {
        x *= v.x;
        y *= v.y;
        return *this;
    }

    Vector2 &operator/=(const Vector2 &v) noexcept
    {
        x /= v.x;
        y /= v.y;
        return *this;
    }

    Vector2 &operator*=(float s) noexcept
    {
        x *= s;
        y *= s;
        return *this;
    }

    Vector2 &operator/=(float s) noexcept
    {
        const float inv = 1.0f / s;
        x *= inv;
        y *= inv;
        return *this;
    }

    Vector2 operator+() const noexcept { return *this; }
    Vector2 operator-() const noexcept { return Vector2(-x, -y); }

    Vector2 operator+(const Vector2 &v) const noexcept { return Vector2(x + v.x, y + v.y); }
    Vector2 operator-(const Vector2 &v) const noexcept { return Vector2(x - v.x, y - v.y); }
    Vector2 operator*(const Vector2 &v) const noexcept { return Vector2(x * v.x, y * v.y); }
    Vector2 operator/(const Vector2 &v) const noexcept { return Vector2(x / v.x, y / v.y); }
    Vector2 operator*(float s) const noexcept { return Vector2(x * s, y * s); }
    Vector2 operator/(float s) const noexcept
    {
        const float inv = 1.0f / s;
        return Vector2(x * inv, y * inv);
    }

    float Length() const noexcept { return std::sqrt(x * x + y * y); }
    float LengthSquared() const noexcept { return x * x + y * y; }
    float Dot(const Vector2 &v) const noexcept { return x * v.x + y * v.y; }

    void Normalize() noexcept
    {
        const float len = Length();
        if (len > 0.0f)
        {
            const float inv = 1.0f / len;
            x *= inv;
            y *= inv;
        }
    }

    void Normalize(Vector2 &result) const noexcept
    {
        const float len = Length();
        if (len > 0.0f)
        {
            const float inv = 1.0f / len;
            result.x = x * inv;
            result.y = y * inv;
            return;
        }
        result.x = 0.0f;
        result.y = 0.0f;
    }

    static const Vector2 Zero;
    static const Vector2 One;
    static const Vector2 UnitX;
    static const Vector2 UnitY;
};

inline Vector2 operator*(float s, const Vector2 &v) noexcept
{
    return Vector2(v.x * s, v.y * s);
}

struct Vector3
{
    float x;
    float y;
    float z;

    Vector3() noexcept : x(0.0f), y(0.0f), z(0.0f) {}
    constexpr explicit Vector3(float v) noexcept : x(v), y(v), z(v) {}
    constexpr Vector3(float ix, float iy, float iz) noexcept : x(ix), y(iy), z(iz) {}
    explicit Vector3(const float *p_array) noexcept : x(p_array[0]), y(p_array[1]), z(p_array[2]) {}

    Vector3(const Vector3 &) = default;
    Vector3 &operator=(const Vector3 &) = default;

    bool operator==(const Vector3 &v) const noexcept { return x == v.x && y == v.y && z == v.z; }
    bool operator!=(const Vector3 &v) const noexcept { return !(*this == v); }

    Vector3 &operator+=(const Vector3 &v) noexcept
    {
        x += v.x;
        y += v.y;
        z += v.z;
        return *this;
    }

    Vector3 &operator-=(const Vector3 &v) noexcept
    {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        return *this;
    }

    Vector3 &operator*=(const Vector3 &v) noexcept
    {
        x *= v.x;
        y *= v.y;
        z *= v.z;
        return *this;
    }

    Vector3 &operator/=(const Vector3 &v) noexcept
    {
        x /= v.x;
        y /= v.y;
        z /= v.z;
        return *this;
    }

    Vector3 &operator*=(float s) noexcept
    {
        x *= s;
        y *= s;
        z *= s;
        return *this;
    }

    Vector3 &operator/=(float s) noexcept
    {
        *this = FromH3d(h3d_vec3_div_scalar(ToH3d(), s));
        return *this;
    }

    Vector3 operator+() const noexcept { return *this; }
    Vector3 operator-() const noexcept { return Vector3(-x, -y, -z); }

    Vector3 operator+(const Vector3 &v) const noexcept { return FromH3d(h3d_vec3_add(ToH3d(), v.ToH3d())); }
    Vector3 operator-(const Vector3 &v) const noexcept { return FromH3d(h3d_vec3_sub(ToH3d(), v.ToH3d())); }
    Vector3 operator*(const Vector3 &v) const noexcept { return Vector3(x * v.x, y * v.y, z * v.z); }
    Vector3 operator/(const Vector3 &v) const noexcept { return Vector3(x / v.x, y / v.y, z / v.z); }
    Vector3 operator*(float s) const noexcept { return FromH3d(h3d_vec3_mul_scalar(ToH3d(), s)); }
    Vector3 operator/(float s) const noexcept { return FromH3d(h3d_vec3_div_scalar(ToH3d(), s)); }

    float Length() const noexcept { return h3d_vec3_length(ToH3d()); }
    float LengthSquared() const noexcept { return h3d_vec3_length_sq(ToH3d()); }
    float Dot(const Vector3 &v) const noexcept { return h3d_vec3_dot(ToH3d(), v.ToH3d()); }
    Vector3 Cross(const Vector3 &v) const noexcept { return FromH3d(h3d_vec3_cross(ToH3d(), v.ToH3d())); }

    void Normalize() noexcept
    {
        *this = FromH3d(h3d_vec3_normalize(ToH3d()));
    }

    void Normalize(Vector3 &result) const noexcept
    {
        result = FromH3d(h3d_vec3_normalize(ToH3d()));
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

private:
    h3d_vec3 ToH3d() const noexcept { return h3d_vec3{x, y, z}; }

    static Vector3 FromH3d(h3d_vec3 v) noexcept { return Vector3(v.x, v.y, v.z); }
};

inline Vector3 operator*(float s, const Vector3 &v) noexcept
{
    return v * s;
}

struct Vector4
{
    float x;
    float y;
    float z;
    float w;

    Vector4() noexcept : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
    constexpr explicit Vector4(float v) noexcept : x(v), y(v), z(v), w(v) {}
    constexpr Vector4(float ix, float iy, float iz, float iw) noexcept : x(ix), y(iy), z(iz), w(iw) {}
    constexpr Vector4(const Vector3 &v, float iw = 0.0f) noexcept : x(v.x), y(v.y), z(v.z), w(iw) {}
    explicit Vector4(const float *p_array) noexcept : x(p_array[0]), y(p_array[1]), z(p_array[2]), w(p_array[3]) {}

    Vector4(const Vector4 &) = default;
    Vector4 &operator=(const Vector4 &) = default;

    bool operator==(const Vector4 &v) const noexcept { return x == v.x && y == v.y && z == v.z && w == v.w; }
    bool operator!=(const Vector4 &v) const noexcept { return !(*this == v); }

    Vector4 &operator+=(const Vector4 &v) noexcept
    {
        x += v.x;
        y += v.y;
        z += v.z;
        w += v.w;
        return *this;
    }

    Vector4 &operator-=(const Vector4 &v) noexcept
    {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        w -= v.w;
        return *this;
    }

    Vector4 &operator*=(const Vector4 &v) noexcept
    {
        x *= v.x;
        y *= v.y;
        z *= v.z;
        w *= v.w;
        return *this;
    }

    Vector4 &operator/=(const Vector4 &v) noexcept
    {
        x /= v.x;
        y /= v.y;
        z /= v.z;
        w /= v.w;
        return *this;
    }

    Vector4 &operator*=(float s) noexcept
    {
        x *= s;
        y *= s;
        z *= s;
        w *= s;
        return *this;
    }

    Vector4 &operator/=(float s) noexcept
    {
        const float inv = 1.0f / s;
        x *= inv;
        y *= inv;
        z *= inv;
        w *= inv;
        return *this;
    }

    Vector4 operator+() const noexcept { return *this; }
    Vector4 operator-() const noexcept { return Vector4(-x, -y, -z, -w); }

    Vector4 operator+(const Vector4 &v) const noexcept { return Vector4(x + v.x, y + v.y, z + v.z, w + v.w); }
    Vector4 operator-(const Vector4 &v) const noexcept { return Vector4(x - v.x, y - v.y, z - v.z, w - v.w); }
    Vector4 operator*(const Vector4 &v) const noexcept { return Vector4(x * v.x, y * v.y, z * v.z, w * v.w); }
    Vector4 operator/(const Vector4 &v) const noexcept { return Vector4(x / v.x, y / v.y, z / v.z, w / v.w); }
    Vector4 operator*(float s) const noexcept { return Vector4(x * s, y * s, z * s, w * s); }
    Vector4 operator/(float s) const noexcept
    {
        const float inv = 1.0f / s;
        return Vector4(x * inv, y * inv, z * inv, w * inv);
    }

    float Length() const noexcept { return std::sqrt(x * x + y * y + z * z + w * w); }
    float LengthSquared() const noexcept { return x * x + y * y + z * z + w * w; }
    float Dot(const Vector4 &v) const noexcept { return x * v.x + y * v.y + z * v.z + w * v.w; }

    void Normalize() noexcept
    {
        const float len = Length();
        if (len > 0.0f)
        {
            const float inv = 1.0f / len;
            x *= inv;
            y *= inv;
            z *= inv;
            w *= inv;
        }
    }

    void Normalize(Vector4 &result) const noexcept
    {
        const float len = Length();
        if (len > 0.0f)
        {
            const float inv = 1.0f / len;
            result.x = x * inv;
            result.y = y * inv;
            result.z = z * inv;
            result.w = w * inv;
            return;
        }
        result.x = 0.0f;
        result.y = 0.0f;
        result.z = 0.0f;
        result.w = 0.0f;
    }

    static const Vector4 Zero;
    static const Vector4 One;
    static const Vector4 UnitX;
    static const Vector4 UnitY;
    static const Vector4 UnitZ;
    static const Vector4 UnitW;
};

inline Vector4 operator*(float s, const Vector4 &v) noexcept
{
    return Vector4(v.x * s, v.y * s, v.z * s, v.w * s);
}

struct Matrix
{
    union
    {
        float m[4][4];
        struct
        {
            float _11;
            float _12;
            float _13;
            float _14;
            float _21;
            float _22;
            float _23;
            float _24;
            float _31;
            float _32;
            float _33;
            float _34;
            float _41;
            float _42;
            float _43;
            float _44;
        };
    };

    Matrix() noexcept { std::memset(m, 0, sizeof(m)); }
    Matrix(const Matrix &) = default;
    Matrix &operator=(const Matrix &) = default;

    Matrix(float m00, float m01, float m02, float m03, float m10, float m11, float m12, float m13, float m20,
           float m21, float m22, float m23, float m30, float m31, float m32, float m33) noexcept
    {
        m[0][0] = m00;
        m[0][1] = m01;
        m[0][2] = m02;
        m[0][3] = m03;
        m[1][0] = m10;
        m[1][1] = m11;
        m[1][2] = m12;
        m[1][3] = m13;
        m[2][0] = m20;
        m[2][1] = m21;
        m[2][2] = m22;
        m[2][3] = m23;
        m[3][0] = m30;
        m[3][1] = m31;
        m[3][2] = m32;
        m[3][3] = m33;
    }

    float &operator()(size_t row, size_t col) noexcept { return m[row][col]; }
    const float &operator()(size_t row, size_t col) const noexcept { return m[row][col]; }

    Matrix operator*(const Matrix &other) const noexcept
    {
        return FromH3d(h3d_mat4_mul(ToH3d(), other.ToH3d()));
    }

    Matrix &operator*=(const Matrix &other) noexcept
    {
        *this = *this * other;
        return *this;
    }

    static Matrix CreateLookAt(const Vector3 &eye, const Vector3 &target, const Vector3 &up) noexcept
    {
        return FromH3d(h3d_mat4_look_at({eye.x, eye.y, eye.z}, {target.x, target.y, target.z}, {up.x, up.y, up.z}));
    }

    static Matrix CreatePerspectiveFieldOfView(float fov, float aspect_ratio, float near_plane, float far_plane) noexcept
    {
        return FromH3d(h3d_mat4_perspective_fov(fov, aspect_ratio, near_plane, far_plane));
    }

    static Matrix CreatePerspective(float width, float height, float near_plane, float far_plane) noexcept
    {
        return FromH3d(h3d_mat4_perspective(width, height, near_plane, far_plane));
    }

    static Matrix CreateOrthographic(float width, float height, float near_plane, float far_plane) noexcept
    {
        return FromH3d(h3d_mat4_orthographic(width, height, near_plane, far_plane));
    }

    Matrix Transpose() const noexcept
    {
        return FromH3d(h3d_mat4_transpose(ToH3d()));
    }

    Matrix Invert() const noexcept
    {
        return FromH3d(h3d_mat4_invert(ToH3d()));
    }

    static const Matrix Identity;

private:
    h3d_mat4 ToH3d() const noexcept
    {
        h3d_mat4 out;
        std::memcpy(out.m, m, sizeof(m));
        return out;
    }

    static Matrix FromH3d(h3d_mat4 in) noexcept
    {
        Matrix out;
        std::memcpy(out.m, in.m, sizeof(out.m));
        return out;
    }
};

struct Quaternion
{
    float x;
    float y;
    float z;
    float w;

    Quaternion() noexcept : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
    constexpr Quaternion(float ix, float iy, float iz, float iw) noexcept : x(ix), y(iy), z(iz), w(iw) {}

    Quaternion(const Quaternion &) = default;
    Quaternion &operator=(const Quaternion &) = default;
};

struct Plane
{
    Vector3 normal;
    float d;

    Plane() noexcept : normal(), d(0.0f) {}
    Plane(const Vector3 &n, float distance) noexcept : normal(n), d(distance) {}

    Plane(const Plane &) = default;
    Plane &operator=(const Plane &) = default;
};

struct Ray
{
    Vector3 position;
    Vector3 direction;

    Ray() noexcept : position(), direction() {}
    Ray(const Vector3 &pos, const Vector3 &dir) noexcept : position(pos), direction(dir) {}

    Ray(const Ray &) = default;
    Ray &operator=(const Ray &) = default;
};

struct Color
{
    float r;
    float g;
    float b;
    float a;

    Color() noexcept : r(0.0f), g(0.0f), b(0.0f), a(1.0f) {}
    constexpr Color(float ir, float ig, float ib, float ia = 1.0f) noexcept : r(ir), g(ig), b(ib), a(ia) {}

    Color(const Color &) = default;
    Color &operator=(const Color &) = default;
};

static_assert(sizeof(Vector3) == sizeof(h3d_vec3), "Vector3 layout must match h3d_vec3");
static_assert(std::is_standard_layout_v<Vector3>, "Vector3 must be standard layout");

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
    tm._11 = 1.0f;
    tm._22 = 1.0f;
    tm._33 = 1.0f;
    tm._44 = 1.0f;
    return tm;
}();

} // namespace SimpleMath
} // namespace DirectX

namespace DirectX
{
struct BoundingFrustum
{
    BoundingFrustum() = default;
};
} // namespace DirectX

#ifdef __clang__
#pragma clang diagnostic pop
#endif
