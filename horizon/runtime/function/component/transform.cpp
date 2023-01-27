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

void Transform::SetPosition(const math::Vector3f &pos) noexcept {
    need_recalculation = true;
    m_position = pos;
}

void Transform::SetRotation(const math::Vector3f &rot) noexcept {
    need_recalculation = true;
    m_rotation.x() = rot.x(); //FIXME:
    //m_rotation = math::quaternion::CreateFromYawPitchRoll(rot.y, rot.x, rot.z);
}

void Transform::SetScale(const math::Vector3f &scale) noexcept {
    need_recalculation = true;
    m_scale = scale;
}

math::Matrix44f Horizon::Transform::GetTransformMatrix() noexcept {
    if (need_recalculation) {
        math::Matrix44f t = math::CreateTranslation(m_position);
        math::Matrix44f r = math::CreateRotationFromQuaternion(m_rotation);
        math::Matrix44f s = math::CreateScale(m_scale);
        m_transform_matrix = s * r * t;
        need_recalculation = false;
    }
    return m_transform_matrix;
}

math::Vector3f Transform::GetOrientation() noexcept {

    //math::Matrix44f r = math::Matrix44f::CreateRotationFromQuaternion(m_rotation);
    //m_orientation =
    //    math::Vector4f(math::DefaultOrientation.x, math::DefaultOrientation.y, math::DefaultOrientation.z, 0.0f) * r;
    return m_orientation;
}

math::Vector3f Transform::GetTranslation() const noexcept { return m_position; }

math::Vector3f Transform::GetRotation() const noexcept { return math::ToEular(m_rotation); }

math::Vector3f Transform::GetScale() const noexcept { return m_scale; }

} // namespace Horizon