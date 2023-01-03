/*****************************************************************//**
 * \file   transform.cpp
 * \brief  
 * 
 * \author hylu
 * \date   December 2022
 *********************************************************************/

#include "Transform.h"

// standard libraries

// third party libraries

// project headers

namespace Horizon {

Horizon::Transform::Transform() {}

Horizon::Transform::~Transform() {}

void Transform::SetPosition(const Math::float3 &pos) noexcept {
    need_recalculation = true;
    m_position = pos;
}

void Transform::SetRotation(const Math::float3 &rot) noexcept {
    need_recalculation = true;
    m_rotation = Math::quaternion::CreateFromYawPitchRoll(rot.y, rot.x, rot.z);
}

void Transform::SetScale(const Math::float3 &scale) noexcept {
    need_recalculation = true;
    m_scale = scale;
}

Math::float4x4 Horizon::Transform::GetTransformMatrix() noexcept {
    if (need_recalculation) {
        Math::float4x4 t = Math::float4x4::CreateTranslation(m_position);
        Math::float4x4 r = Math::float4x4::CreateFromQuaternion(m_rotation);
        Math::float4x4 s = Math::float4x4::CreateScale(m_scale);
        m_transform_matrix = s * r * t;
        need_recalculation = false;
    }
    return m_transform_matrix;
}

Math::float3 Transform::GetOrientation() noexcept {

    //Math::float4x4 r = Math::float4x4::CreateFromQuaternion(m_rotation);
    //m_orientation =
    //    Math::float4(Math::DefaultOrientation.x, Math::DefaultOrientation.y, Math::DefaultOrientation.z, 0.0f) * r;
    return m_orientation;
}

Math::float3 Transform::GetTranslation() const noexcept { return m_position; }

Math::float3 Transform::GetRotation() const noexcept { return m_rotation.ToEuler(); }

Math::float3 Transform::GetScale() const noexcept { return m_scale; }

} // namespace Horizon