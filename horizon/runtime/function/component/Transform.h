/*****************************************************************//**
 * \file   transform.h
 * \brief  
 * 
 * \author hylu
 * \date   December 2022
 *********************************************************************/
#pragma once

#include "Component.h"

#include <runtime/core/math/Math.h>

namespace Horizon {

class Transform : public Component {
  public:
    Transform();
    ~Transform();

    void SetPosition(const Math::float3 &pos) noexcept;
    void SetRotation(const Math::float3 &rot) noexcept;
    void SetScale(const Math::float3 &scale) noexcept;

    Math::float4x4 GetTransformMatrix() noexcept;
    Math::float3 GetOrientation() noexcept;
    Math::float3 GetTranslation() const noexcept;
    Math::float3 GetRotation() const noexcept;
    Math::float3 GetScale() const noexcept;

  private:
    bool need_recalculation = true;
    Math::float4x4 m_transform_matrix = Math::float4x4::Identity;
    Math::float3 m_position;
    Math::quaternion m_rotation;
    Math::float3 m_scale = Math::float3(1.0, 1.0, 1.0);
    Math::float3 m_orientation = Math::DefaultOrientation;
};

} // namespace Horizon
