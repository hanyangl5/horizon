#pragma once

#include <stdint.h>

// RootConstant0 slot offsets shared by the geometry-style shaders (Geometry.slang, ShadowMap.slang).
// Slots 0..2 hold the per-Draw identity, 3..7 the frame and asset table indices, 11 the Draw data.
constexpr uint32_t GeometryBindingSlot = 3;
constexpr uint32_t GeometryDrawSlot = 11;
