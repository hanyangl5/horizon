#pragma once

#include <stdint.h>

// RootConstant0 slot offsets shared by the geometry-style shaders (Geometry.slang, ShadowMap.slang).
// Slots 0..2 hold the per-draw identity, 3..7 the frame and asset table indices, 11 the draw data.
constexpr uint32_t kGeometryBindingSlot = 3;
constexpr uint32_t kGeometryDrawSlot = 11;
