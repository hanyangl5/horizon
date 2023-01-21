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

// third party libraries

// project headers
#include <runtime/core/math/3d/matrix.hpp>
#include <runtime/core/math/3d/quat.hpp>
#include <runtime/core/math/3d/vector.hpp>
#include <runtime/core/math/common.h>
#include <runtime/core/utils/definations.h>

namespace Horizon::math {

Matrix<4, 4> CreatePerspectiveProjectionMatrix(f32 fov, f32 aspect_ratio, f32 near_plane, f32 far_plane) { return {}; }

//Matrix<4, 4> CreatePerspectiveProjectionMatrix(f32 w, f32 h, f32 near_plane, f32 far_plane) { return {}; }
// inline float4x4 Ortho(f32 width, f32 height, f32 near_plane, f32 far_plane) {
//     return DirectX::SimpleMath::Matrix::CreateOrthographic(width, height, near_plane, far_plane);
// }

Matrix<4, 4> CreateOrthoProjectionMatrix() { return {}; }

template<typename T>
Matrix<4, 4> CreateLookAt(const Vector<3, T>& eye, const Vector<3, T>& target, const Vector<3, T>& up) {
    return {};
}

template <typename T> Matrix<4, 4> CreateTranslation(const Vector<3, T>& t) { return {}; }
template <typename T> Matrix<4, 4> CreateFromQuaternion(const Quaternion<T>& q) { return {}; }
template <typename T> Matrix<4, 4> CreateScale(const Vector<3, T>& s) { return {}; }

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

// overload operators

template <u32 dimension, typename T = f32>
constexpr Vector<dimension, T> operator+(const Vector<dimension, T> &lhs, const Vector<dimension, T> &rhs) {
    Vector<dimension, T> ret{};
    for (u32 i = 0; i < dimension; i++) {
        ret.at(i) = lhs.at(i) + rhs.at(i);
    }
    return ret;
}

template <u32 dimension, typename T = f32>
constexpr Vector<dimension, T> operator+(const Vector<dimension, T> &lhs, const T &rhs) {
    Vector<dimension, T> ret{};
    for (u32 i = 0; i < dimension; i++) {
        ret.at(i) = lhs.at(i) + rhs;
    }
    return ret;
}

template <u32 dimension, typename T = f32>
constexpr Vector<dimension, T> operator+(const T &lhs, const Vector<dimension, T> &rhs) {
    Vector<dimension, T> ret{};
    for (u32 i = 0; i < dimension; i++) {
        ret.at(i) = lhs + rhs.at(i);
    }
    return ret;
}

template <u32 dimension, typename T = f32>
constexpr Vector<dimension, T> operator-(const Vector<dimension, T> &lhs, const Vector<dimension, T> &rhs) {
    Vector<dimension, T> ret{};
    for (u32 i = 0; i < dimension; i++) {
        ret.at(i) = lhs.at(i) - rhs.at(i);
    }
    return ret;
}

template <u32 dimension, typename T = f32>
constexpr Vector<dimension, T> operator-(const Vector<dimension, T> &lhs, const T &rhs) {
    Vector<dimension, T> ret{};
    for (u32 i = 0; i < dimension; i++) {
        ret.at(i) = lhs.at(i) - rhs;
    }
    return ret;
}

template <u32 dimension, typename T = f32>
constexpr Vector<dimension, T> operator-(const T &lhs, const Vector<dimension, T> &rhs) {
    Vector<dimension, T> ret{};
    for (u32 i = 0; i < dimension; i++) {
        ret.at(i) = lhs - rhs.at(i);
    }
    return ret;
}

template <u32 dimension, typename T = f32>
constexpr Vector<dimension, T> operator*(const Vector<dimension, T> &lhs, const Vector<dimension, T> &rhs) {
    Vector<dimension, T> ret{};
    for (u32 i = 0; i < dimension; i++) {
        ret.at(i) = lhs.at(i) * rhs.at(i);
    }
    return ret;
}

template <u32 dimension, typename T = f32>
constexpr Vector<dimension, T> operator*(const Vector<dimension, T> &lhs, const T &rhs) {
    Vector<dimension, T> ret{};
    for (u32 i = 0; i < dimension; i++) {
        ret.at(i) = lhs.at(i) * rhs;
    }
    return ret;
}

template <u32 dimension, typename T = f32>
constexpr Vector<dimension, T> operator*(const T &lhs, const Vector<dimension, T> &rhs) {
    Vector<dimension, T> ret{};
    for (u32 i = 0; i < dimension; i++) {
        ret.at(i) = lhs * rhs.at(i);
    }
    return ret;
}

template <u32 dimension, typename T = f32>
constexpr Vector<dimension, T> operator/(const Vector<dimension, T> &lhs, const Vector<dimension, T> &rhs) {
    Vector<dimension, T> ret{};
    for (u32 i = 0; i < dimension; i++) {
        ret.at(i) = lhs.at(i) / rhs.at(i);
    }
    return ret;
}

template <u32 dimension, typename T = f32>
constexpr Vector<dimension, T> operator/(const Vector<dimension, T> &lhs, const T &rhs) {
    Vector<dimension, T> ret{};
    for (u32 i = 0; i < dimension; i++) {
        ret.at(i) = lhs.at(i) / rhs;
    }
    return ret;
}

template <u32 dimension, typename T = f32>
constexpr Vector<dimension, T> operator/(const T &lhs, const Vector<dimension, T> &rhs) {
    Vector<dimension, T> ret{};
    for (u32 i = 0; i < dimension; i++) {
        ret.at(i) = lhs / rhs.at(i);
    }
    return ret;
}

template <u32 dimension, typename T = f32>
constexpr Vector<dimension, T> &operator+=(Vector<dimension, T> &lhs, const Vector<dimension, T> &rhs) {
    for (u32 i = 0; i < dimension; i++) {
        lhs.at(i) += rhs.at(i);
    }
    return lhs;
}

template <u32 dimension, typename T = f32>
constexpr Vector<dimension, T> &operator+=(Vector<dimension, T> &lhs, const T &rhs) {
    for (u32 i = 0; i < dimension; i++) {
        lhs(i) += rhs;
    }
    return lhs;
}

template <u32 dimension, typename T = f32>
constexpr Vector<dimension, T> &operator-=(Vector<dimension, T> &lhs, const Vector<dimension, T> &rhs) {
    for (u32 i = 0; i < dimension; i++) {
        lhs.at(i) -= rhs.at(i);
    }
    return lhs;
}

template <u32 dimension, typename T = f32>
constexpr Vector<dimension, T> &operator-=(Vector<dimension, T> &lhs, const T &rhs) {
    for (u32 i = 0; i < dimension; i++) {
        lhs(i) -= rhs;
    }
    return lhs;
}

template <u32 dimension, typename T = f32>
constexpr Vector<dimension, T> &operator*=(Vector<dimension, T> &lhs, const Vector<dimension, T> &rhs) {
    for (u32 i = 0; i < dimension; i++) {
        lhs(i) *= rhs.at(i);
    }
    return lhs;
}

template <u32 dimension, typename T = f32>
constexpr Vector<dimension, T> &operator*=(Vector<dimension, T> &lhs, const T &rhs) {
    for (u32 i = 0; i < dimension; i++) {
        lhs(i) *= rhs;
    }
    return lhs;
}

template <u32 dimension, typename T = f32>
constexpr Vector<dimension, T> &operator/=(Vector<dimension, T> &lhs, const Vector<dimension, T> &rhs) {
    for (u32 i = 0; i < dimension; i++) {
        lhs(i) /= rhs.at(i);
    }
    return lhs;
}

template <u32 dimension, typename T = f32>
constexpr Vector<dimension, T> &operator/=(Vector<dimension, T> &lhs, const T &rhs) {
    for (u32 i = 0; i < dimension; i++) {
        lhs(i) /= rhs;
    }
    return lhs;
}

// matrix

template <u32 dimension_m, u32 dimension_n, typename T = f32>
constexpr Matrix<dimension_m, dimension_n, T> operator+(const Matrix<dimension_m, dimension_n, T> &lhs, const Matrix<dimension_m, dimension_n, T> &rhs) {
    Matrix<dimension_m, dimension_n, T> ret{};
    for (u32 i = 0; i < dimension_m; i++) {
        for (u32 j = 0; j < dimension_n; j++) {
            ret(i, j) = lhs.at(i, j) + rhs.at(i, j);
        }
    }
    return ret;
}

template <u32 dimension_m, u32 dimension_n, typename T = f32>
constexpr Matrix<dimension_m, dimension_n, T> operator+(const Matrix<dimension_m, dimension_n, T> &lhs, const T &rhs) {
    Matrix<dimension_m, dimension_n, T> ret{};
    for (u32 i = 0; i < dimension_m; i++) {
        for (u32 j = 0; j < dimension_n; j++) {
            ret(i, j) = lhs.at(i, j) + rhs;
        }
    }
    return ret;
}

template <u32 dimension_m, u32 dimension_n, typename T = f32>
constexpr Matrix<dimension_m, dimension_n, T> operator+(const T &lhs, const Matrix<dimension_m, dimension_n, T> &rhs) {
    Matrix<dimension_m, dimension_n, T> ret{};
    for (u32 i = 0; i < dimension_m; i++) {
        for (u32 j = 0; j < dimension_n; j++) {
            ret(i, j) = lhs + rhs.at(i, j);
        }
    }
    return ret;
}

template <u32 dimension_m, u32 dimension_n, typename T = f32>
constexpr Matrix<dimension_m, dimension_n, T> operator-(const Matrix<dimension_m, dimension_n, T> &lhs, const Matrix<dimension_m, dimension_n, T> &rhs) {
    Matrix<dimension_m, dimension_n, T> ret{};
    for (u32 i = 0; i < dimension_m; i++) {
        for (u32 j = 0; j < dimension_n; j++) {
            ret(i, j) = lhs.at(i, j) - rhs.at(i, j);
        }
    }
    return ret;
}

template <u32 dimension_m, u32 dimension_n, typename T = f32>
constexpr Matrix<dimension_m, dimension_n, T> operator-(const Matrix<dimension_m, dimension_n, T> &lhs, const T &rhs) {
    Matrix<dimension_m, dimension_n, T> ret{};
    for (u32 i = 0; i < dimension_m; i++) {
        for (u32 j = 0; j < dimension_n; j++) {
            ret(i, j) = lhs.at(i, j) - rhs;
        }
    }
    return ret;
}

template <u32 dimension_m, u32 dimension_n, typename T = f32>
constexpr Matrix<dimension_m, dimension_n, T> operator-(const T &lhs, const Matrix<dimension_m, dimension_n, T> &rhs) {
    Matrix<dimension_m, dimension_n, T> ret{};
    for (u32 i = 0; i < dimension_m; i++) {
        for (u32 j = 0; j < dimension_n; j++) {
            ret(i, j) = lhs - rhs.at(i, j);
        }
    }
    return ret;
}

template <u32 dimension_m, u32 dimension_n, typename T = f32>
constexpr Matrix<dimension_m, dimension_n, T> operator*(const Matrix<dimension_m, dimension_n, T> &lhs, const T &rhs) {
    Matrix<dimension_m, dimension_n, T> ret{};
    for (u32 i = 0; i < dimension_m; i++) {
        for (u32 j = 0; j < dimension_n; j++) {
            ret(i, j) = lhs.at(i, j) * rhs;
        }
    }
    return ret;
}

// multiply a constant
template <u32 dimension_m, u32 dimension_n, typename T = f32>
constexpr Matrix<dimension_m, dimension_n, T> operator*(const T &lhs, const Matrix<dimension_m, dimension_n, T> &rhs) {
    Matrix<dimension_m, dimension_n, T> ret{};
    for (u32 i = 0; i < dimension_m; i++) {
        for (u32 j = 0; j < dimension_n; j++) {
            ret(i, j) = lhs * rhs.at(i, j);
        }
    }
    return ret;
}

// divide a constant
template <u32 dimension_m, u32 dimension_n, typename T = f32>
constexpr Matrix<dimension_m, dimension_n, T> operator/(const Matrix<dimension_m, dimension_n, T> &lhs, const T &rhs) {
    Matrix<dimension_m, dimension_n, T> ret{};
    for (u32 i = 0; i < dimension_m; i++) {
        for (u32 j = 0; j < dimension_n; j++) {
            ret(i, j) = lhs.at(i, j) / rhs;
        }
    }
    return ret;
}

template <u32 dimension_m, u32 dimension_n, typename T = f32>
constexpr Matrix<dimension_m, dimension_n, T> &operator+=(Matrix<dimension_m, dimension_n, T> &lhs, const Matrix<dimension_m, dimension_n, T> &rhs) {
    for (u32 i = 0; i < dimension_m; i++) {
        for (u32 j = 0; j < dimension_n; j++) {
            lhs(i, j) += rhs.at(i, j);
        }
    }
    return ret;
}

template <u32 dimension_m, u32 dimension_n, typename T = f32>
constexpr Matrix<dimension_m, dimension_n, T> &operator+=(Matrix<dimension_m, dimension_n, T> &lhs, const T &rhs) {
    for (u32 i = 0; i < dimension_m; i++) {
        for (u32 j = 0; j < dimension_n; j++) {
            lhs(i, j) += rhs;
        }
    }
    return ret;
}

template <u32 dimension_m, u32 dimension_n, typename T = f32>
constexpr Matrix<dimension_m, dimension_n, T> &operator-=(Matrix<dimension_m, dimension_n, T> &lhs, const Matrix<dimension_m, dimension_n, T> &rhs) {
    for (u32 i = 0; i < dimension_m; i++) {
        for (u32 j = 0; j < dimension_n; j++) {
            lhs(i, j) -= rhs.at(i, j);
        }
    }
    return ret;
}

template <u32 dimension_m, u32 dimension_n, typename T = f32>
constexpr Matrix<dimension_m, dimension_n, T> &operator-=(Matrix<dimension_m, dimension_n, T> &lhs, const T &rhs) {
    for (u32 i = 0; i < dimension_m; i++) {
        for (u32 j = 0; j < dimension_n; j++) {
            lhs(i, j) -= rhs;
        }
    }
    return ret;
}

// multiply a constant
template <u32 dimension_m, u32 dimension_n, typename T = f32>
constexpr Matrix<dimension_m, dimension_n, T> &operator*=(Matrix<dimension_m, dimension_n, T> &lhs, const T &rhs) {
    for (u32 i = 0; i < dimension_m; i++) {
        for (u32 j = 0; j < dimension_n; j++) {
            lhs(i, j) *= rhs;
        }
    }
    return ret;
}

// divide a constant
template <u32 dimension_m, u32 dimension_n, typename T = f32>
constexpr Matrix<dimension_m, dimension_n, T> &operator/=(Matrix<dimension_m, dimension_n, T> &lhs, const T &rhs) {
    for (u32 i = 0; i < dimension_m; i++) {
        for (u32 j = 0; j < dimension_n; j++) {
            lhs(i, j) /= rhs;
        }
    }
    return ret;
}

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
                ret(i, j) += lhs.at(i, k) * rhs.at(k, j);
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

inline Vector<3> Normalize(const Vector<3> &f) { return {}; }

inline Vector<3> Cross(const Vector<3> &lhs, const Vector<3> &rhs) { return {}; }

// TODO(hylu): add invert
Matrix<4, 4, f32> Invert(const Matrix<4, 4, f32>& m) { return {}; }

Matrix<4, 4, f32> Transpose(const Matrix<4, 4, f32> &m) {
    Matrix<4, 4, f32> ret{};
    for (u32 i = 0; i < 4; i++) {
        for (u32 j = 0; j < 4; j++) {
            ret.at(i, j) = m.at(i, j);
        }
    }
    return ret;
}

constexpr f32 Radians(f32 angle) {
    return angle * _PI / 180.0f;
}

Vector<3, f32> ToEular(const Quaternion<f32>& q) {
    return {};
    
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

Matrix<4, 4, f32> CreateFromQuaternion(Quaternion<f32> q) {

}

} // namespace Horizon::math
