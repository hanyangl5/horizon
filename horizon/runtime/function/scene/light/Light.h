#pragma once

#include <runtime/core/math/Math.h>

namespace Horizon {

// light unit: lux for direct light, lumen for punctual light
enum class LightType { DIRECTIONAL_LIGHT = 0, POINT_LIGHT, SPOT_LIGHT };

struct LightParams {
    Math::float3 color_intensity; // r, g, b
    f32 intensity;                // intensity
    Math::float3 position = Math::float3(0.0);
    u32 type;
    Math::float3 direction = Math::float3(0.0);
    f32 falloff;
    Math::float2 spot_cone_inner_outer = Math::float4(0.0);
    Math::float2 radius_length;
    Math::float3 orientation = Math::float3(0, 0, 1);
    f32 pad;
};

class Light {
  public:
    Light() noexcept = default;
    ~Light() noexcept = default;
    LightParams GetParamBuffer() { return params; }

  protected:
    void SetColor(const Math::float3 &color) noexcept;
    void SetDirection(const Math::float3 &direction) noexcept;
    void SetIntensity(f32 intensity) noexcept;
    void SetPosition(const Math::float3 position) noexcept;
    void SetFalloff(f32 falloff) noexcept;
    void SetSpotLightInnerCone(f32 inner) noexcept;
    void SetSpotLightOuterCone(f32 outer) noexcept;
    void SetRadius(f32 radius) noexcept;
    void SetLength(f32 length) noexcept;
    void SetOrientation(const Math::float3 &orientation) noexcept;

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