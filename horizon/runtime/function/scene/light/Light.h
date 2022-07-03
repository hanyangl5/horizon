#pragma once

#include <runtime/core/math/Math.h>

namespace Horizon {

// light unit: lux for direct light, lumen for punctual light
enum LightType { DIRECT_LIGHT = 0, POINT_LIGHT, SPOT_LIGHT };

struct LightParams {
    Math::float4 color_intensity = Math::float4(0.0); // r, g, b, intensity
    Math::float4 position_type = Math::float4(0.0);
    Math::float4 direction = Math::float4(0.0);
    Math::float4 radius_inner_outer =
        Math::float4(0.0); // radius, innerConeAngle, outerConeAngle
};
} // namespace Horizon