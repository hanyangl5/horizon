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
		Math::vec4 colorIntensity = Math::vec4(0.0); // r, g, b, intensity
		Math::vec4 positionType = Math::vec4(0.0);
		Math::vec4 direction = Math::vec4(0.0);
		Math::vec4 radiusInnerOuter = Math::vec4(0.0); // radius, innerConeAngle, outerConeAngle
	};
}