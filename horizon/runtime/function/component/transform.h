/*****************************************************************//**
 * \file   transform.h
 * \brief  
 * 
 * \author hylu
 * \date   December 2022
 *********************************************************************/

#pragma once

// standard libraries

// third party libraries

// project headers

#include "Component.h"

#include <runtime/core/math/math.h>

namespace Horizon {

class Transform : public Component {
  public:
    Transform();
    ~Transform();

    void SetPosition(const math::Vector3f &pos) noexcept;
    void SetRotation(const math::Vector3f &rot) noexcept;
    void SetScale(const math::Vector3f &scale) noexcept;

    math::Matrix44f GetTransformMatrix() noexcept;
    math::Vector3f GetOrientation() noexcept;
    math::Vector3f GetTranslation() const noexcept;
    math::Vector3f GetRotation() const noexcept;
    math::Vector3f GetScale() const noexcept;

  private:
    bool need_recalculation = true;
    math::Matrix44f m_transform_matrix;
    math::Vector3f m_position;
    math::Quat m_rotation;
    math::Vector3f m_scale = math::Vector3f(1.0, 1.0, 1.0);
    math::Vector3f m_orientation = math::DEFAULT_ORIENTATION;
};

} // namespace Horizon
