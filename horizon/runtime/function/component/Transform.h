/*****************************************************************//**
 * \file   Transform.h
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
    Math::float4x4 GetTransformMatrix() const noexcept;
    Math::float3 GetOrientation() const noexcept;
    Math::float3 GetTranslation() const noexcept;
    Math::float3 GetRotation() const noexcept;
    Math::float3 GetScale() const noexcept;
  private:
    Math::float4x4 transform_matrix = Math::float4x4::Identity;
    Math::float3 position;
    Math::float3 rotation;
    Math::float3 scale;
};

} // namespace Horizon
