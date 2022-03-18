#pragma once
#include "utils.h"
#include "UniformBuffer.h"
#include "Descriptors.h"
#include "UniformBuffer.h"

enum LightType
{
    DIRECT_LIGHT = 0,
    POINT_LIGHT,
    SPOT_LIGHT
};

struct LightParams {
    //LightType lightType;
    vec4 colorIntensity = glm::vec4(0.0); // r, g, b, intensity
    vec4 positionType = glm::vec4(0.0);
    vec4 direction = glm::vec4(0.0);
    vec4 radiusInnerOuter = glm::vec4(0.0); // radius, innerradius, outerradius
};
