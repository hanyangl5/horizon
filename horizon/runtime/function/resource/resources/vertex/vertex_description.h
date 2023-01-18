#pragma once

#include <runtime/core/utils/definations.h>
#include <runtime/core/math/math.h>

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
    math::Vector3f pos;
    math::Vector3f normal;
    math::Vector2f uv0, uv1;
    math::Vector3f tangent;
};

using Index = u32;
// TODO(hylu): multiple vertex description

} // namespace Horizon