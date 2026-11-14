/* Copyright (c) 2026 Horizon */
#pragma once

#include "Scene/SceneComponents.h"

namespace hz
{
// Sort key for MaterialOverride arrays; shared so serialization and binding reject the same order.
inline int compareMaterialOverrides(const void* pA, const void* pB)
{
    const SubmeshID a = ((const MaterialOverride*)pA)->submesh;
    const SubmeshID b = ((const MaterialOverride*)pB)->submesh;
    return (a > b) - (a < b);
}
} // namespace hz
