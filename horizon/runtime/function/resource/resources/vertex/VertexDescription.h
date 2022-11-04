#pragma once

#include <runtime/core/math/Math.h>
#include <runtime/core/utils/definations.h>

namespace Horizon {

enum VertexAttributeType {
    POSTION = 1,
    NORMAL = 2,
    UV0 = 4,
    UV1 = 8,
    TANGENT = 16,
    // BITANGENT = 32
};

struct Vertex {
  public:
    Math::float3 pos;
    Math::float3 normal;
    Math::float2 uv0, uv1;
    Math::float3 tangent;
};

using Index = u32;
// TODO: multiple vertex description

} // namespace Horizon