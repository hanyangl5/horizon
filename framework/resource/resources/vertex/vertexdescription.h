#pragma once

#include <core/definations.h>
#include <core/math.h>

namespace Horizon
{

enum VertexAttributeType
{
    POSTION = 1,
    NORMAL = 2,
    UV0 = 4,
    UV1 = 8,
    TANGENT = 16,
};

struct Vertex
{
  public:
    Math::float3 pos;
    Math::float3 normal;
    Math::float2 uv0, uv1;
    Math::float3 tangent;
    Math::float4 joint_indices{};
    Math::float4 joint_weights{};
};

using Index = u32;
// TODO(hylu): multiple vertex description

} // namespace Horizon
