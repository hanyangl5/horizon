#include "Light.h"

namespace Horizon {

void Light::SetColor(const Math::float3 &color) noexcept {
    params.color_intensity.x = color.x;
    params.color_intensity.y = color.y;
    params.color_intensity.z = color.z;
}

void Light::SetIntensity(f32 intensity) noexcept {

    float luminousIntensity;
    switch (m_type) {
    case LightType::DIRECTIONAL_LIGHT:
    case LightType::POINT_LIGHT:
        luminousIntensity = intensity;
        break;
    case LightType::SPOT_LIGHT: {

        float cosOuter; //= std::sqrt(spotParams.cosOuterSquared);

        luminousIntensity = intensity * (Math::_2PI * (1.0f - cosOuter));
        break;
    }
    }
    params.color_intensity.w = luminousIntensity;
}

void Light::SetPosition(const Math::float3 position) noexcept { params.position = position; }

void Light::SetFalloffRadius(f32 falloff) noexcept { params.radius_inner_outer.x = falloff; }

void Light::SetSpotLightCone(f32 inner, f32 outer) noexcept {
    params.radius_inner_outer.y = inner;
    params.radius_inner_outer.z = outer;
}

void Light::SetDirection(const Math::float3 &direction) noexcept {
    assert(m_type == LightType::DIRECTIONAL_LIGHT || m_type == LightType::SPOT_LIGHT);
    params.direction = direction;
}

DirectionalLight::DirectionalLight(const Math::float3 &color, f32 intensity, const Math::float3 &direction) noexcept {
    m_type = LightType::DIRECTIONAL_LIGHT;
    params.type = static_cast<u32>(LightType::DIRECTIONAL_LIGHT);
    SetColor(color);
    SetIntensity(intensity);
    SetDirection(direction);
}

PointLight::PointLight(const Math::float3 &color, f32 intensity, const Math::float3 &position, f32 radius) noexcept {
    m_type = LightType::POINT_LIGHT;
    params.type = static_cast<u32>(LightType::POINT_LIGHT);
    SetColor(color);
    SetIntensity(intensity);
    SetPosition(position);
    SetFalloffRadius(radius);
}

SpotLight::SpotLight(const Math::float3 &color, f32 intensity, const Math::float3 &position,
                     const Math::float3 &direction, f32 radius, f32 inner_cone, f32 outer_cone) noexcept {
    m_type = LightType::SPOT_LIGHT;
    params.type = static_cast<u32>(LightType::SPOT_LIGHT);
    SetColor(color);
    SetPosition(position);
    SetDirection(direction);
    SetFalloffRadius(radius);
    SetSpotLightCone(inner_cone, outer_cone);
    SetIntensity(intensity);
}

} // namespace Horizon