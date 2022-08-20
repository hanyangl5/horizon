#pragma once

#include <d3d12.h>
#include <directxtk12/SimpleMath.h>

#include <runtime/core/utils/Definations.h>

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

inline float3 Normalize(const float3 &f) {
    float3 ret;
    f.Normalize(ret);
    return ret;
}

inline float3 Cross(const float3 &lhs, const float3 &rhs) { return lhs.Cross(rhs); }

inline f32 Radians(f32 angle) { return _PI / 180.0f; }

inline float4x4 LookAt(const float3 &eye, const float3 &target, const float3 &up) {
    return DirectX::SimpleMath::Matrix::CreateLookAt(eye, target, up);
}
// inline float4x4 PerspectiveProjection(float width, float height,
//                                       float near_plane, float far_plane) {
//     return DirectX::SimpleMath::Matrix::CreatePerspective(width, height,
//     near_plane,
//                                                    far_plane);
// }

inline float4x4 Perspective(float fov, float aspect_ratio, float near_plane, float far_plane) {
    float rad = fov * _PI / 180.0f;
    auto mat = DirectX::SimpleMath::Matrix::CreatePerspectiveFieldOfView(rad, aspect_ratio, near_plane, far_plane);
    mat; // reversed-z
    return std::move(mat);
}

} // namespace Horizon::Math