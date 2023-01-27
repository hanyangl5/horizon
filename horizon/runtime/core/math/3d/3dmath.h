/*****************************************************************/ /**
 * \file   FILE_NAME
 * \brief  BRIEF
 * 
 * \author XXX
 * \date   XXX
 *********************************************************************/

#pragma once

// standard libraries
#include <type_traits>
#include <iostream>
// third party libraries

// project headers
#include <runtime/core/platform/platform.h>
#include <runtime/core/utils/definations.h>
#include <runtime/core/math/common.h>
#include <runtime/core/math/circular/circular.h>
#include <runtime/core/math/3d/matrix.hpp>
#include <runtime/core/math/3d/quat.hpp>
#include <runtime/core/math/3d/vector.hpp>


namespace Horizon::math {

extern Vector<3> DEFAULT_ORIENTATION;

// FIXME(hylu): current math lib is only for row major matrix right hand coord

template <u32 dimension, typename T = f32>
constexpr T dot(const Vector<dimension, T> &a, const Vector<dimension, T> &b) {
    T ret{};
    for (u32 i = 0; i < dimension; i++) {
        ret += a.at(i) * b.at(i);
    }
    return ret;
}

template <u32 dimension_m, u32 dimension_n, typename T = f32>
constexpr T dot(const Matrix<dimension_m, dimension_n, T> &a, const Matrix<dimension_m, dimension_n, T> &b) {
    T ret{};
    for (u32 i = 0; i < dimension_m; i++) {
        for (u32 j = 0; j < dimension_n; j++) {
            ret += a.at(i, j) * b.at(i, j);
        }
    }
    return ret;
}

// vector

#define ARITHMETIC_VECTOR_OPERATE_VECTOR(OP)                                                                           \
    template <u32 dimension, typename T = f32>                                                                         \
    constexpr Vector<dimension, T> operator OP(const Vector<dimension, T> &lhs, const Vector<dimension, T> &rhs) {     \
        Vector<dimension, T> ret{};                                                                                    \
        for (u32 i = 0; i < dimension; i++) {                                                                          \
            ret.at(i) = lhs.at(i) OP rhs.at(i);                                                                        \
        }                                                                                                              \
        return ret;                                                                                                    \
    }

#define ARITHMETIC_VECTOR_OPERATE_SCALAR(OP)                                                                           \
    template <u32 dimension, typename T = f32>                                                                         \
    constexpr Vector<dimension, T> operator OP(const Vector<dimension, T> &lhs, const T &rhs) {                        \
        Vector<dimension, T> ret{};                                                                                    \
        for (u32 i = 0; i < dimension; i++) {                                                                          \
            ret.at(i) = lhs.at(i) OP rhs;                                                                              \
        }                                                                                                              \
        return ret;                                                                                                    \
    }

#define ARITHMETIC_SCALAR_OPERATE_VECTOR(OP)                                                                           \
    template <u32 dimension, typename T = f32>                                                                         \
    constexpr Vector<dimension, T> operator OP(const T &lhs, const Vector<dimension, T> &rhs) {                        \
        Vector<dimension, T> ret{};                                                                                    \
        for (u32 i = 0; i < dimension; i++) {                                                                          \
            ret.at(i) = lhs OP rhs.at(i);                                                                              \
        }                                                                                                              \
        return ret;                                                                                                    \
    }

#define ASSIGNMENT_VECTOR_OPERATE_VECTOR(OP)                                                                           \
    template <u32 dimension, typename T = f32>                                                                         \
    constexpr Vector<dimension, T> &operator OP(Vector<dimension, T> &lhs, const Vector<dimension, T> &rhs) {          \
        for (u32 i = 0; i < dimension; i++) {                                                                          \
            lhs.at(i) OP rhs.at(i);                                                                                    \
        }                                                                                                              \
        return lhs;                                                                                                    \
    }

#define ASSIGNMENT_VECTOR_OPERATE_SCALAR(OP)                                                                           \
    template <u32 dimension, typename T = f32>                                                                         \
    constexpr Vector<dimension, T> &operator OP(Vector<dimension, T> &lhs, const T &rhs) {                             \
        for (u32 i = 0; i < dimension; i++) {                                                                          \
            lhs.at(i) OP rhs;                                                                                          \
        }                                                                                                              \
        return lhs;                                                                                                    \
    }

// matrix

#define ARITHMETIC_MATRIX_OPERATE_MATRIX(OP)                                                                           \
    template <u32 dimension_m, u32 dimension_n, typename T = f32>                                                      \
    constexpr Matrix<dimension_m, dimension_n, T> operator OP(const Matrix<dimension_m, dimension_n, T> &lhs,          \
                                                              const Matrix<dimension_m, dimension_n, T> &rhs) {        \
        Matrix<dimension_m, dimension_n, T> ret{};                                                                     \
        for (u32 i = 0; i < dimension_m; i++) {                                                                        \
            for (u32 j = 0; j < dimension_n; j++) {                                                                    \
                ret(i, j) = lhs.at(i, j) OP rhs.at(i, j);                                                              \
            }                                                                                                          \
        }                                                                                                              \
        return ret;                                                                                                    \
    }

#define ARITHMETIC_MATRIX_OPERATE_SCALAR(OP)                                                                           \
    template <u32 dimension_m, u32 dimension_n, typename T = f32>                                                      \
    constexpr Matrix<dimension_m, dimension_n, T> operator OP(const Matrix<dimension_m, dimension_n, T> &lhs,          \
                                                              const T &rhs) {                                          \
        Matrix<dimension_m, dimension_n, T> ret{};                                                                     \
        for (u32 i = 0; i < dimension_m; i++) {                                                                        \
            for (u32 j = 0; j < dimension_n; j++) {                                                                    \
                ret(i, j) = lhs.at(i, j) OP rhs;                                                                       \
            }                                                                                                          \
        }                                                                                                              \
        return ret;                                                                                                    \
    }
#define ARITHMETIC_SCALAR_OPERATE_MATRIX(OP)                                                                           \
    template <u32 dimension_m, u32 dimension_n, typename T = f32>                                                      \
    constexpr Matrix<dimension_m, dimension_n, T> operator OP(const T &lhs,                                            \
                                                              const Matrix<dimension_m, dimension_n, T> &rhs) {        \
        Matrix<dimension_m, dimension_n, T> ret{};                                                                     \
        for (u32 i = 0; i < dimension_m; i++) {                                                                        \
            for (u32 j = 0; j < dimension_n; j++) {                                                                    \
                ret(i, j) = lhs OP rhs.at(i, j);                                                                       \
            }                                                                                                          \
        }                                                                                                              \
        return ret;                                                                                                    \
    }

#define ASSIGNMENT_MATRIX_OPERATE_MATRIX(OP)                                                                           \
    template <u32 dimension_m, u32 dimension_n, typename T = f32>                                                      \
    constexpr Matrix<dimension_m, dimension_n, T> &operator OP(Matrix<dimension_m, dimension_n, T> &lhs,               \
                                                               const Matrix<dimension_m, dimension_n, T> &rhs) {       \
        for (u32 i = 0; i < dimension_m; i++) {                                                                        \
            for (u32 j = 0; j < dimension_n; j++) {                                                                    \
                lhs(i, j) OP rhs.at(i, j);                                                                             \
            }                                                                                                          \
        }                                                                                                              \
        return ret;                                                                                                    \
    }

#define ASSIGNMENT_MATRIX_OPERATE_SCALAR(OP)                                                                           \
    template <u32 dimension_m, u32 dimension_n, typename T = f32>                                                      \
    constexpr Matrix<dimension_m, dimension_n, T> &operator OP(Matrix<dimension_m, dimension_n, T> &lhs,               \
                                                               const T &rhs) {                                         \
        for (u32 i = 0; i < dimension_m; i++) {                                                                        \
            for (u32 j = 0; j < dimension_n; j++) {                                                                    \
                lhs(i, j) OP rhs;                                                                                      \
            }                                                                                                          \
        }                                                                                                              \
        return ret;                                                                                                    \
    }

ARITHMETIC_VECTOR_OPERATE_VECTOR(+)
ARITHMETIC_VECTOR_OPERATE_VECTOR(-)
ARITHMETIC_VECTOR_OPERATE_VECTOR(*)
ARITHMETIC_VECTOR_OPERATE_VECTOR(/)

ARITHMETIC_VECTOR_OPERATE_SCALAR(+)
ARITHMETIC_VECTOR_OPERATE_SCALAR(-)
ARITHMETIC_VECTOR_OPERATE_SCALAR(*)
ARITHMETIC_VECTOR_OPERATE_SCALAR(/)

ARITHMETIC_SCALAR_OPERATE_VECTOR(+)
ARITHMETIC_SCALAR_OPERATE_VECTOR(-)
ARITHMETIC_SCALAR_OPERATE_VECTOR(*)
ARITHMETIC_SCALAR_OPERATE_VECTOR(/)

ASSIGNMENT_VECTOR_OPERATE_VECTOR(+=)
ASSIGNMENT_VECTOR_OPERATE_VECTOR(-=)
ASSIGNMENT_VECTOR_OPERATE_VECTOR(*=)
ASSIGNMENT_VECTOR_OPERATE_VECTOR(/=)

ASSIGNMENT_VECTOR_OPERATE_SCALAR(+=)
ASSIGNMENT_VECTOR_OPERATE_SCALAR(-=)
ASSIGNMENT_VECTOR_OPERATE_SCALAR(*=)
ASSIGNMENT_VECTOR_OPERATE_SCALAR(/=)

ARITHMETIC_MATRIX_OPERATE_MATRIX(+)
ARITHMETIC_MATRIX_OPERATE_MATRIX(-)

ARITHMETIC_MATRIX_OPERATE_SCALAR(+)
ARITHMETIC_MATRIX_OPERATE_SCALAR(-)
ARITHMETIC_MATRIX_OPERATE_SCALAR(*)
ARITHMETIC_MATRIX_OPERATE_SCALAR(/)

ARITHMETIC_SCALAR_OPERATE_MATRIX(+)
ARITHMETIC_SCALAR_OPERATE_MATRIX(-)
ARITHMETIC_SCALAR_OPERATE_MATRIX(*)

ASSIGNMENT_MATRIX_OPERATE_MATRIX(+=)
ASSIGNMENT_MATRIX_OPERATE_MATRIX(-=)

ASSIGNMENT_MATRIX_OPERATE_SCALAR(+=)
ASSIGNMENT_MATRIX_OPERATE_SCALAR(-=)
ASSIGNMENT_MATRIX_OPERATE_SCALAR(*=)
ASSIGNMENT_MATRIX_OPERATE_SCALAR(/=)

// misc

// matrix vector multiplication

template <u32 dimension_m, u32 dimension_n, typename T = f32>
constexpr Vector<dimension_m, T> operator*(const Matrix<dimension_m, dimension_n, T> &lhs,
                                           const Vector<dimension_n, T> &rhs) {
    Vector<dimension_m, T> ret;
    for (u32 i = 0; i < dimension_m; i++) {
        for (u32 j = 0; j < dimension_n; j++) {
            ret.at(i) += lhs.at(i, j) * rhs.at(j);
        }
    }
    return ret;
}

template <u32 dimension_m, u32 dimension_n, u32 dimension_o, typename T = f32>
constexpr Matrix<dimension_m, dimension_o, T> operator*(const Matrix<dimension_m, dimension_n, T> &lhs,
                                                        const Matrix<dimension_n, dimension_o, T> &rhs) {
    Matrix<dimension_m, dimension_o, T> ret;
    for (u32 i = 0; i < dimension_m; i++) {
        for (u32 j = 0; j < dimension_o; j++) {
            for (u32 k = 0; k < dimension_n; k++) {
                ret.at(i, j) += lhs.at(i, k) * rhs.at(k, j);
            }
        }
    }
    return ret;
}

// TODO(hylu): SIMD code for vector4/matrix4x4 mutiplication

// printing

template <u32 dimension, typename T = f32> std::ostream &operator<<(std::ostream &os, Vector<dimension, T> &a) {
    os << "Vector" << dimension << ": [";
    for (u32 i = 0; i < dimension - 1; i++) {
        os << a.at(i) << ", ";
    }
    os << a.at(dimension - 1) << "]\n";
    return os;
}

// printing
template <u32 dimension_m, u32 dimension_n, typename T = f32>
std::ostream &operator<<(std::ostream &os, const Matrix<dimension_m, dimension_n, T> &a) {
    os << "Matrix" << dimension_m << "x" << dimension_n << ": \n";
    for (u32 i = 0; i < dimension_m; i++) {
        os << "[";
        for (u32 j = 0; j < dimension_n - 1; j++) {
            os << a.at(i, j) << ", ";
        }
        os << a.at(i, dimension_n - 1) << "]\n";
    }
    os << "\n";
    return os;
}

inline f32 Length(const Vector<3> &v) { return v.x() * v.x() + v.y() * v.y() + v.z() * v.z(); }

inline Vector<3> Normalize(const Vector<3> &v) { return v / Length(v); }

inline Vector<3> Cross(const Vector<3> &lhs, const Vector<3> &rhs) {
    return Vector<3>{lhs.y() * rhs.z() + lhs.z() * rhs.y(), lhs.z() * rhs.z() + lhs.z() * rhs.x(),
                     lhs.y() * rhs.x() + lhs.x() * rhs.y()};
}

// TODO(hylu): add invert
inline Matrix<4, 4, f32> Invert(const Matrix<4, 4, f32> &m) { return m; }

inline Matrix<4, 4, f32> Transpose(const Matrix<4, 4, f32> &m) {
    Matrix<4, 4, f32> ret{};
    for (u32 i = 0; i < 4; i++) {
        for (u32 j = 0; j < 4; j++) {
            ret.at(i, j) = m.at(i, j);
        }
    }
    return ret;
}

constexpr inline f32 Radians(f32 angle) { return angle * _PI / 180.0f; }

inline Vector<3, f32> ToEular(const Quaternion<f32> &q) {
    f32 x = q.x();
    f32 y = q.y();
    f32 z = q.z();
    f32 w = q.w();
    const f32 xx = x * x;
    const f32 yy = y * y;
    const f32 zz = z * z;

    const f32 m31 = 2.f * x * z + 2.f * y * w;
    const f32 m32 = 2.f * y * z - 2.f * x * w;
    const f32 m33 = 1.f - 2.f * xx - 2.f * yy;

    const f32 cy = sqrtf(m33 * m33 + m31 * m31);
    const f32 cx = atan2f(-m32, cy);
    if (cy > 16.f * FLT_EPSILON) {
        const f32 m12 = 2.f * x * y + 2.f * z * w;
        const f32 m22 = 1.f - 2.f * xx - 2.f * zz;

        return Vector<3>(cx, atan2f(m31, m33), atan2f(m12, m22));
    } else {
        const f32 m11 = 1.f - 2.f * yy - 2.f * zz;
        const f32 m21 = 2.f * x * y - 2.f * z * w;

        return Vector<3>(cx, 0.f, atan2f(-m21, m11));
    }
}

inline Matrix<4, 4> CreatePerspectiveProjectionMatrix1(f32 w, f32 h, f32 n, f32 f) { 
    Matrix<4, 4> ret{};
    ret.at(0, 0) = 2.0f / w;
    ret.at(1, 1) = 2.0f / h;

    ret.at(2, 2) = -(f + n) / (f - n);
    ret.at(2, 3) = -2.0f * (f * n) / (f - n);
    ret.at(3, 2) = -1.0f;
    ret.at(3, 3) = 0.0f;
    return ret;
}

inline Matrix<4, 4> CreatePerspectiveProjectionMatrix2(f32 fov, f32 aspect_ratio, f32 n, f32 f) {
    f32 tan = math::Tan(fov / 2.0f);

    Matrix<4, 4> ret{};
    ret.at(0, 0) = 1.0f / (aspect_ratio * tan);
    ret.at(1, 1) = 1.0f / tan;

    ret.at(2, 2) = -(f + n) / (f - n);
    ret.at(2, 3) = -2 * (f * n) / (f - n);
    ret.at(3, 2) = -1.0f;
    ret.at(3, 3) = 0.0f;
    return ret;
}

inline Matrix<4, 4> CreateOrthoProjectionMatrix(f32 w, f32 h, f32 n, f32 f) {
    Matrix<4, 4> ret{};
    ret.at(0, 0) = 2.0f / w;
    ret.at(1, 1) = 2.0f / h;
    ret.at(2,2) = -1.0f / (f - n);
    ret.at(2,3) = -n / (f - n);
    return ret;
}

inline Matrix<4, 4> CreateLookAt(const Vector<3> &eye, const Vector<3> &target, const Vector<3> &up) {

    Vector<3> f(math::Normalize(target - eye));
    Vector<3> s(math::Normalize(math::Cross(f, up)));
    Vector<3> u(math::Cross(s, f));

    Matrix<4, 4> ret;
    ret.at(0, 0) = s.x();
    ret.at(0, 1) = s.y();
    ret.at(0, 2) = s.z();
    ret.at(1, 0) = u.x();
    ret.at(1, 1) = u.y();
    ret.at(1, 2) = u.z();
    ret.at(2, 0) = -f.x();
    ret.at(2, 1) = -f.y();
    ret.at(2, 2) = -f.z();
    ret.at(0, 3) = -math::dot(s, eye);
    ret.at(1, 3) = -math::dot(u, eye);
    ret.at(2, 3) = -math::dot(f, eye);
    return ret;
}

inline Matrix<4, 4> CreateTranslation(const Vector<3> &t) {
    Matrix<4, 4> ret{};
    ret.at(3, 0) = t.x();
    ret.at(3, 1) = t.y();
    ret.at(3, 2) = t.z();
    return ret;
}

inline Matrix<4, 4> CreateRotationFromQuaternion(const Quaternion<> &q) {
    f32 qx = q.x();
    f32 qxx = qx * qx;

    f32 qy = q.y();
    f32 qyy = qy * qy;

    f32 qz = q.z();
    f32 qzz = qz * qz;

    f32 qw = q.w();

    Matrix<4, 4> M;
    M.at(0, 0) = 1.f - 2.f * qyy - 2.f * qzz;
    M.at(0, 1) = 2.f * qx * qy + 2.f * qz * qw;
    M.at(0, 2) = 2.f * qx * qz - 2.f * qy * qw;
    M.at(0, 3) = 0.f;
    M.at(1, 0) = 2.f * qx * qy - 2.f * qz * qw;
    M.at(1, 1) = 1.f - 2.f * qxx - 2.f * qzz;
    M.at(1, 2) = 2.f * qy * qz + 2.f * qx * qw;
    M.at(1, 3) = 0.f;
    M.at(2, 0) = 2.f * qx * qz + 2.f * qy * qw;
    M.at(2, 1) = 2.f * qy * qz - 2.f * qx * qw;
    M.at(2, 2) = 1.f - 2.f * qxx - 2.f * qyy;
    M.at(2, 3) = 0.f;
    M.at(3, 0) = 0.f;
    M.at(3, 1) = 0.f;
    M.at(3, 2) = 0.f;
    M.at(3, 3) = 1.0f;
    return M;
}

inline Matrix<4, 4> CreateScale(const Vector<3> &s) {
    Matrix<4, 4> ret{};
    ret.at(0, 0) = s.x();
    ret.at(1, 1) = s.y();
    ret.at(2, 2) = s.z();
    return ret;
}

} // namespace Horizon::math
