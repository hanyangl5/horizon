#pragma once

#include <runtime/core/math/Math.h>
#include <runtime/core/utils/definations.h>

namespace Horizon {

enum VertexAttributeType { POSTION = 0, NORMAL = 1, TBN = 2, UV1 = 4, UV2 = 8 };

struct Vertex {
  public:
    Math::float3 pos;
    Math::float3 normal;
    // Math::float4 tbn;
    Math::float2 uv1, uv2;
};

using Index = u32;
// TODO: multiple vertex description

} // namespace Horizon