#pragma once

#include "Core/IMath.h"
#include <stdint.h>

// RootConstant0 slot offsets shared by the geometry-style shaders (Geometry.slang, ShadowMap.slang).
// Slots 0..2 hold the per-Draw identity, 3..7 the frame and asset table indices, 11 the Draw data.
constexpr uint32_t GeometryBindingSlot = 3;
constexpr uint32_t GeometryDrawSlot = 11;

// Matches FrameData in Common.slang.
struct FrameData
{
    Matrix4  ViewProjection = Matrix4::identity();
    Matrix4  PreviousViewProjection = Matrix4::identity();
    Matrix4  InverseViewProjection = Matrix4::identity();
    Vector4  Eye = Vector4(0, 0, 0, 1);
    float    Near = 0, Far = 0;
    uint32_t Width = 0, Height = 0;
};
static_assert(sizeof(FrameData) == 224);
