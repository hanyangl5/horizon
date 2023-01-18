/*****************************************************************//**
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
#include <runtime/core/utils/definations.h>
#include <runtime/core/math/3d/vector.hpp>
#include <runtime/core/math/3d/matrix.hpp>
#include <runtime/core/math/3d/quat.hpp>

namespace Horizon::math {

Matrix<4, 4> CreatePerspectiveProjectionMatrix();

Matrix<4, 4> CreateOrthoProjectionMatrix();

Matrix<4, 4> CreateLookAt();

template<u32 dimension, typename T = f32>
constexpr T dot(const Vector<dimension, T>& a, const Vector<dimension, T>& b) {
    T ret{};
    for (u32 i = 0; i < dimension;i++) {
        ret += a.at(i) * b.at(i);
    }
    return ret;
}

template<u32 dimension_m, u32 dimension_n, typename T = f32>
constexpr T dot(const Matrix<dimension_m, dimension_n, T>& a, const Matrix<dimension_m, dimension_n, T>& b) {
    T ret;
    for (u32 i = 0; i < dimension_m; i++) {
        for (u32 j = 0; j < dimension_n; j++)
        {
            ret += a.at(i, j)* b.at(i, j);
        }
    }
    return ret;
}

// template<u32 dimension, typename T = f32>
// constexpr Vector<dimension, T> operator*(const Vector<dimension, T>& a, const Vector<dimension, T>& b)
// {
//     Vector<dimension, T> ret;
//     for (u32 i = 0; i < dimension;i++) {
//         ret(i) = a.at(i) * b.at(i);
//     }
//     return ret;
// }

// TODO(hylu): SIMD code for vector4/matrix4x4 mutiplication

// printing
template<u32 dimension, typename T = f32>
std::ostream& operator<<(std::ostream& os, Vector<dimension, T>& a)
{
    os << "Vector" << dimension << ": [";
    for (u32 i = 0; i < dimension - 1; i++) {
        os << a.at(i)<<", ";
    }
    os << a.at(dimension-1) << "]\n";
    return os;
}

// printing
template<u32 dimension_m, u32 dimension_n, typename T = f32>
std::ostream& operator<<(std::ostream& os, const Matrix<dimension_m, dimension_n, T>& a)
{
    os << "Matrix" << dimension_m << "x" << dimension_n << ": \n";
    for (u32 i = 0; i < dimension_m; i++) {
        os << "[";
        for (u32 j = 0; j <dimension_n - 1 ; j++)
        {
           os << a.at(i, j)<< ", ";
        }
        os << a.at(i, dimension_n - 1) <<  "]\n";
    }
    os << "\n";
    return os;
}

inline Vector<3> Normalize(const Vector<3> &f) { return {}; }

inline Vector<3> Cross(const Vector<3> &lhs, const Vector<3> &rhs) { return {}; }

// inline f32 Radians(f32 angle) {
//     return angle * _PI / 180.0f; 
// }

// inline float4x4 LookAt(const Vector<3> &eye, const Vector<3> &target, const Vector<3> &up) {
//     return DirectX::SimpleMath::Matrix::CreateLookAt(eye, target, up);
// }

// inline float4x4 Perspective(float fov, float aspect_ratio, float near_plane, float far_plane) {
//     return DirectX::SimpleMath::Matrix::CreatePerspectiveFieldOfView(fov, aspect_ratio, near_plane, far_plane);
// }

// inline float4x4 Ortho(float width, float height, float near_plane, float far_plane) {
//     return DirectX::SimpleMath::Matrix::CreateOrthographic(width, height, near_plane, far_plane);
// }

// default orientation of transform
// static Vector<3> DefaultOrientation = math::Vector3f{0, 0, 1};

}

