/* Copyright (c) 2026 Horizon */
#pragma once

#include "Scene/SceneComponents.h"

namespace hz
{
// Sort key for MaterialOverride arrays; shared so serialization and binding reject the same order.
inline int CompareMaterialOverrides(const void* Left, const void* Right)
{
    const SubmeshID LeftId = ((const MaterialOverride*)Left)->submesh;
    const SubmeshID RightId = ((const MaterialOverride*)Right)->submesh;
    return (LeftId > RightId) - (LeftId < RightId);
}
} // namespace hz
