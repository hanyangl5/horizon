/*****************************************************************//**
 * \file   light.h
 * \brief  
 * 
 * \author hylu
 * \date   January 2023
 *********************************************************************/

#pragma once

// standard libraries

// third party libraries

// project headers
#include <runtime/core/math/math.h>

namespace Horizon {

// light unit: lux for direct light, lumen for punctual light
enum class LightType { DIRECTIONAL_LIGHT = 0, POINT_LIGHT, SPOT_LIGHT };

struct LightParams {
    math::Vector3f color_intensity; // r, g, b
    f32 intensity;                   // intensity
    math::Vector3f position;
    u32 type;
    math::Vector3f direction;
    f32 falloff;
    math::Vector2f spot_cone_inner_outer;
    math::Vector2f radius_length;
    math::Vector3f orientation = math::Vector3f{0, 0, 1};
    f32 pad;
};

class Light {
  public:
    Light() noexcept = default;
    ~Light() noexcept = default;
    LightParams GetParamBuffer() { return params; }

  protected:
    void SetColor(const math::Vector3f &color) noexcept;
    void SetDirection(const math::Vector3f &direction) noexcept;
    void SetIntensity(f32 intensity) noexcept;
    void SetPosition(const math::Vector3f position) noexcept;
    void SetFalloff(f32 falloff) noexcept;
    void SetSpotLightInnerCone(f32 inner) noexcept;
    void SetSpotLightOuterCone(f32 outer) noexcept;
    void SetRadius(f32 radius) noexcept;
    void SetLength(f32 length) noexcept;
    void SetOrientation(const math::Vector3f &orientation) noexcept;

  protected:
    LightType m_type{};
    LightParams params{};
};

class DirectionalLight : public Light {
  public:
    DirectionalLight(const math::Vector3f &color, f32 intensity, const math::Vector3f &direction) noexcept;
};

class PointLight : public Light {
  public:
    PointLight(const math::Vector3f &color, f32 intensity, const math::Vector3f &position, f32 radius) noexcept;
};

class SpotLight : public Light {
  public:
    SpotLight(const math::Vector3f &color, f32 intensity, const math::Vector3f &position, const math::Vector3f &direction,
              f32 radius, f32 inner_cone, f32 outer_cone) noexcept;
};

class AreaLight {};
} // namespace Horizon