#pragma once

#include <runtime/core/math/Math.h>

namespace Horizon {

// light unit: lux for direct light, lumen for punctual light
enum class LightType { DIRECT_LIGHT = 0, POINT_LIGHT, SPOT_LIGHT };

struct LightParams {
    Math::float4 color_intensity = Math::float4(0.0); // r, g, b, intensity
    Math::float4 position_type = Math::float4(0.0);
    Math::float4 direction = Math::float4(0.0);
    Math::float4 radius_inner_outer =
        Math::float4(0.0); // radius, innerConeAngle, outerConeAngle
};

class Light {
  public:
    Light() = default;
    ~Light() = default;

  protected:
    LightType m_type;

    Math::color color;
    f32 intensity;
};

class DirectionalLight : public Light {
  public:
    DirectionalLight(Math::color color, f32 intensity, Math::float3 direction) {
        m_type = LightType::DIRECT_LIGHT;
    }
};
class PointLight : public Light {
  public:
    PointLight(Math::float3 color, f32 intensity, f32 radius) {
        m_type = LightType::POINT_LIGHT;
    }
};
class SpotLight : public Light {
  public:
    SpotLight(Math::float3 color, f32 intensity, f32 inner_cone,
              f32 outer_cone) {
        m_type = LightType::SPOT_LIGHT;
    }
};

class AreaLight {};
} // namespace Horizon