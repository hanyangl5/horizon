#include "Light.h"

namespace Horizon {

void Light::SetColor(const Math::float3 &color) noexcept {
    params.color_intensity.x = color.x;
    params.color_intensity.y = color.y;
    params.color_intensity.z = color.z;
}

void Light::SetIntensity(f32 intensity, LightUnit unit) noexcept {

    float luminousPower = intensity;
    float luminousIntensity;
    switch (m_type) {
    case LightType::DIRECTIONAL_LIGHT:
        // luminousPower is in lux, nothing to do.
        luminousIntensity = luminousPower;
        break;
    case LightType::POINT_LIGHT:
        if (unit == LightUnit::LUMEN) {
            // li = lp / (4 * pi)
            luminousIntensity = luminousPower * Math::_1DIVPI * 0.25f;
        } else if (unit == LightUnit::CANDELA) {
            // intensity specified directly in candela, no conversion needed
            luminousIntensity = luminousPower;
        }
        break;
    case LightType::SPOT_LIGHT: {

        //float cosOuter = std::sqrt(spotParams.cosOuterSquared);
        //if (unit == LightUnit::LUMEN) {
        //    // li = lp / (2 * pi * (1 - cos(cone_outer / 2)))
        //    luminousIntensity = luminousPower / (Math::_2PI * (1.0f - cosOuter));
        //} else if (unit == LightUnit::CANDELA)
        //    // intensity specified directly in candela, no conversion needed
        //    luminousIntensity = luminousPower;
        //// lp = li * (2 * pi * (1 - cos(cone_outer / 2)))

        //luminousPower = luminousIntensity * (Math::_2PI * (1.0f - cosOuter));

        // spotParams.luminousPower = luminousPower;
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
    SetColor(color);
    SetIntensity(intensity, LightUnit::LUX);
    SetDirection(direction);
}

PointLight::PointLight(const Math::float3 &color, f32 intensity, f32 radius) noexcept {
    SetColor(color);
    SetIntensity(intensity, LightUnit::LUMEN);
    SetFalloffRadius(radius);
}

SpotLight::SpotLight(const Math::float3 &color, f32 intensity, const Math::float3 &direction, f32 radius,
                     f32 inner_cone, f32 outer_cone) noexcept {
    SetColor(color);
    SetDirection(direction);
    SetFalloffRadius(radius);
    SetSpotLightCone(inner_cone, outer_cone);
    SetIntensity(intensity, LightUnit::LUMEN);
}

} // namespace Horizon