#pragma once

#include <runtime/core/math/Math.h>
#include <runtime/core/utils/definations.h>

namespace Horizon {

enum VertexAttributeType { POSTION = 0, NORMAL = 1, TBN = 2, UV1 = 4, UV2 = 8 };

struct Vertex {
  public:
    Math::float3 pos;

  private:
    f32 pad0;

  public:
    Math::float3 normal;

  private:
    f32 pad1;

  public:
    Math::float4 tbn;
    Math::float2 uv1, uv2;
};

// TODO: multiple vertex description

} // namespace Horizon