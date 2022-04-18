#pragma once

#include <runtime/core/utils/utils.h>

namespace Horizon {

	// light unit: lux for direct light, lumen for punctual light
	enum LightType
	{
		DIRECT_LIGHT = 0,
		POINT_LIGHT,
		SPOT_LIGHT
	};

	struct LightParams {
		//LightType lightType;
		Math::vec4 color_intensity = Math::vec4(0.0); // r, g, b, intensity
		Math::vec4 position_type = Math::vec4(0.0);
		Math::vec4 direction = Math::vec4(0.0);
		Math::vec4 radius_inner_outer = Math::vec4(0.0); // radius, innerConeAngle, outerConeAngle
	};
}