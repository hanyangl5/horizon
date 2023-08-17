/*****************************************************************/ /**
 * \file   Math.h
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#pragma once

#include <runtime/core/utils/definations.h>

#include <DirectXCollision.h>
#include <SimpleMath.h>

namespace Horizon {

template <typename T> inline T Lerp(T a, T b, f32 t) { return a + t * (b - a); }

template <typename T> T AlignUp(T a, T b) { return (a + T(b - 1)) / b; }

} // namespace Horizon

namespace Horizon::Math {

static constexpr f32 _PI = 3.141592654f;
static constexpr f32 _2PI = 6.283185307f;
static constexpr f32 _1DIVPI = 0.318309886f;
static constexpr f32 _1DIV2PI = 0.159154943f;
static constexpr f32 _PIDIV2 = 1.570796327f;
static constexpr f32 _PIDIV4 = 0.785398163f;

using float2 = DirectX::SimpleMath::Vector2;
using float3 = DirectX::SimpleMath::Vector3;
using float4 = DirectX::SimpleMath::Vector4;
using float4x4 = DirectX::SimpleMath::Matrix;
using quaternion = DirectX::SimpleMath::Quaternion;
using plane = DirectX::SimpleMath::Plane;
using ray = DirectX::SimpleMath::Ray;
using color = DirectX::SimpleMath::Color;

using BoundingFrustum = DirectX::BoundingFrustum;

inline float3 Normalize(const float3 &f) {
    float3 ret;
    f.Normalize(ret);
    return ret;
}

inline float3 Cross(const float3 &lhs, const float3 &rhs) { return lhs.Cross(rhs); }

inline f32 Radians(f32 angle) { return angle * Math::_PI / 180.0f; }

inline float4x4 LookAt(const float3 &eye, const float3 &target, const float3 &up) {
    return DirectX::SimpleMath::Matrix::CreateLookAt(eye, target, up);
}

inline float4x4 Perspective(float fov, float aspect_ratio, float near_plane, float far_plane) {
    auto mat = DirectX::SimpleMath::Matrix::CreatePerspectiveFieldOfView(fov, aspect_ratio, near_plane, far_plane);
    return mat;
}

inline float4x4 Ortho(float width, float height, float near_plane, float far_plane) {
    auto mat = DirectX::SimpleMath::Matrix::CreateOrthographic(width, height, near_plane, far_plane);
    return mat;
}

} // namespace Horizon::Math
