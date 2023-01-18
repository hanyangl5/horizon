#include "Light.h"

// standard libraries

// third party libraries

// project headers

namespace Horizon {

void Light::SetColor(const math::Vector3f &color) noexcept {
    params.color_intensity.x = color.x;
    params.color_intensity.y = color.y;
    params.color_intensity.z = color.z;
}

void Light::SetIntensity(f32 intensity) noexcept {
    switch (m_type) {
    case LightType::DIRECTIONAL_LIGHT:
    case LightType::POINT_LIGHT:
        params.intensity = intensity;
        break;
    case LightType::SPOT_LIGHT:
        // https://google.github.io/filament/Filament.htm
        params.intensity = intensity / math::_2PI * (1 - cos(params.spot_cone_inner_outer.y / 2));
        break;
    }
}

void Light::SetPosition(const math::Vector3f position) noexcept {
    assert(m_type != LightType::DIRECTIONAL_LIGHT);
    params.position = position;
}

void Light::SetFalloff(f32 falloff) noexcept {
    params.falloff = falloff;
}

void Light::SetDirection(const math::Vector3f &direction) noexcept {
    assert(m_type == LightType::DIRECTIONAL_LIGHT || m_type == LightType::SPOT_LIGHT);
    params.direction = direction;
}

void Light::SetSpotLightInnerCone(f32 inner) noexcept {
    params.spot_cone_inner_outer.x = inner; 
}

void Light::SetSpotLightOuterCone(f32 outer) noexcept {
    params.spot_cone_inner_outer.y = outer; 
}

void Light::SetRadius(f32 radius) noexcept {
    params.radius_length.x = radius; 
}

void Light::SetLength(f32 length) noexcept {
    params.radius_length.y = length; 
}

void Light::SetOrientation(const math::Vector3f &orientation) noexcept {
    params.orientation = orientation; 
}

DirectionalLight::DirectionalLight(const math::Vector3f &color, f32 intensity, const math::Vector3f &direction) noexcept {\
    m_type = LightType::DIRECTIONAL_LIGHT;
    params.type = static_cast<u32>(LightType::DIRECTIONAL_LIGHT);
    SetColor(color);
    SetIntensity(intensity);
    SetDirection(direction);
}

PointLight::PointLight(const math::Vector3f &color, f32 intensity, const math::Vector3f &position, f32 radius) noexcept {
    m_type = LightType::POINT_LIGHT;
    params.type = static_cast<u32>(LightType::POINT_LIGHT);
    SetColor(color);
    SetIntensity(intensity);
    SetPosition(position);
    SetFalloff(radius);
}

SpotLight::SpotLight(const math::Vector3f &color, f32 intensity, const math::Vector3f &position,
                     const math::Vector3f &direction, f32 radius, f32 inner_cone, f32 outer_cone) noexcept {
    m_type = LightType::SPOT_LIGHT;
    params.type = static_cast<u32>(LightType::SPOT_LIGHT);
    SetColor(color);
    SetPosition(position);
    SetDirection(direction);
    SetFalloff(radius);
    SetSpotLightInnerCone(inner_cone);
    SetSpotLightOuterCone(outer_cone);
    SetIntensity(intensity);
}

} // namespace Horizon