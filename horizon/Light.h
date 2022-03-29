#pragma once

#include "utils.h"

namespace Horizon {

	enum LightType
	{
		DIRECT_LIGHT = 0,
		POINT_LIGHT,
		SPOT_LIGHT
	};

	struct LightParams {
		//LightType lightType;
		vec4 colorIntensity = vec4(0.0); // r, g, b, intensity
		vec4 positionType = vec4(0.0);
		vec4 direction = vec4(0.0);
		vec4 radiusInnerOuter = vec4(0.0); // radius, innerConeAngle, outerConeAngle
	};
}