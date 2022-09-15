#pragma once

#include <runtime/core/math/Math.h>
#include <runtime/core/utils/definations.h>

namespace Horizon {

enum VertexAttributeType { POSTION = 1, NORMAL = 2, TBN = 4, UV0 = 8, UV1 = 16 };

struct Vertex {
  public:
    Math::float3 pos;
    Math::float3 normal;
    // Math::float4 tbn;
    Math::float2 uv0, uv1;
};

using Index = u32;
// TODO: multiple vertex description

} // namespace Horizon