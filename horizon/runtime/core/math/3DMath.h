/*****************************************************************//**
 * \file   3DMath.h
 * \brief  
 * 
 * \author hylu
 * \date   December 2022
 *********************************************************************/

#pragma once

#include <DirectXCollision.h>
#include <d3d12.h>
#include <directxtk12/SimpleMath.h>

#include <runtime/core/utils/Definations.h>

namespace Horizon::Math {

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

inline f32 Radians(f32 angle) { return angle * _PI / 180.0f; }

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
    auto mat = DirectX::SimpleMath::Matrix::CreatePerspectiveFieldOfView(fov, aspect_ratio, near_plane, far_plane);
    mat; // reversed-z
    return std::move(mat);
}

inline float4x4 Ortho(float width, float height, float near_plane, float far_plane) {
    auto mat = DirectX::SimpleMath::Matrix::CreateOrthographic(width, height, near_plane, far_plane);
    mat; // reversed-z
    return std::move(mat);
}

static float3 DefaultOrientation = Math::float3{0, 0, 1};

}
