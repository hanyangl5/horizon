#pragma once

#include <runtime/core/math/Math.h>

namespace Horizon {

// light unit: lux for direct light, lumen for punctual light
enum class LightType { DIRECT_LIGHT = 0, POINT_LIGHT, SPOT_LIGHT };

struct LightParams {
    Math::float4 color_intensity = Math::float4(0.0); // r, g, b, intensity
    Math::float3 position = Math::float3(0.0);
    u32 type;
    Math::float3 direction = Math::float3(0.0);
    f32 pad0;
    Math::float4 radius_inner_outer = Math::float4(0.0); // radius, innerConeAngle, outerConeAngle
};

class Light {
  public:
    Light() noexcept = default;
    ~Light() noexcept = default;

  public:
    LightType m_type{};
    LightParams params{};
};

class DirectionalLight : public Light {
  public:
    DirectionalLight(Math::color color, f32 intensity, Math::float3 direction) noexcept {
        m_type = LightType::DIRECT_LIGHT;
        params.color_intensity = Math::float4(color.x, color.y, color.z, intensity);
        params.direction = direction;
        params.direction.Normalize();
        params.type = static_cast<u32>(m_type);
    }
};
class PointLight : public Light {
  public:
    PointLight(Math::float3 color, f32 intensity, f32 radius) noexcept { m_type = LightType::POINT_LIGHT; }
};
class SpotLight : public Light {
  public:
    SpotLight(Math::float3 color, f32 intensity, f32 inner_cone, f32 outer_cone) noexcept {
        m_type = LightType::SPOT_LIGHT;
    }
};

class AreaLight {};
} // namespace Horizon