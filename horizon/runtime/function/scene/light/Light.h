#pragma once

#include <runtime/core/math/Math.h>

namespace Horizon {

// light unit: lux for direct light, lumen for punctual light
enum class LightType { DIRECTIONAL_LIGHT = 0, POINT_LIGHT, SPOT_LIGHT };

enum class LightUnit { LUX, LUMEN, CANDELA };

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
    LightParams GetParamBuffer() { return params; }

  protected:
    void SetColor(const Math::float3 &color) noexcept;
    void SetIntensity(f32 intensity, LightUnit uint) noexcept;
    void SetPosition(const Math::float3 position) noexcept;
    void SetFalloffRadius(f32 falloff) noexcept;
    void SetSpotLightCone(f32 inner, f32 outer) noexcept;
    void SetDirection(const Math::float3 &direction) noexcept;

  protected:
    LightType m_type{};
    LightParams params{};
};

class DirectionalLight : public Light {
  public:
    DirectionalLight(const Math::float3 &color, f32 intensity, const Math::float3 &direction) noexcept;
};

class PointLight : public Light {
  public:
    PointLight(const Math::float3 &color, f32 intensity, const Math::float3 &position, f32 radius) noexcept;
};

class SpotLight : public Light {
  public:
    SpotLight(const Math::float3 &color, f32 intensity, const Math::float3 &position, const Math::float3 &direction,
              f32 radius, f32 inner_cone, f32 outer_cone) noexcept;
};

class AreaLight {};
} // namespace Horizon