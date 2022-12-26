/*****************************************************************//**
 * \file   Transform.cpp
 * \brief  
 * 
 * \author hylu
 * \date   December 2022
 *********************************************************************/

#include "Transform.h"

namespace Horizon {

Horizon::Transform::Transform() {}

Horizon::Transform::~Transform() {}

Math::float4x4 Horizon::Transform::GetTransformMatrix() const noexcept { return transform_matrix; }

Math::float3 Transform::GetTranslation() const noexcept { return position; }

} // namespace Horizon