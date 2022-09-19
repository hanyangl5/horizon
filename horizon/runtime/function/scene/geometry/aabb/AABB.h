#pragma once

#include <runtime/core/math/Math.h>
#include <runtime/core/utils/definations.h>

namespace Horizon {

class AABB {
  public:
    AABB() noexcept = default;
    ~AABB() noexcept = default;

    AABB(const AABB &rhs) noexcept = default;
    AABB &operator=(const AABB &rhs) noexcept = default;
    AABB(AABB &&rhs) noexcept = default;
    AABB &operator=(AABB &&rhs) noexcept = default;

  public:
    Math::float3 min;
    Math::float3 max;
};

} // namespace Horizon