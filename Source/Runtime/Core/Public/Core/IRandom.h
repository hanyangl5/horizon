#pragma once

#include "Core/IConfig.h"

#include "../Private/Math/Random.h"

namespace hz
{
// Fills the requested range from the system random source. On failure, discard its contents.
bool getSystemRandomBytes(void* pData, uint32_t size);
} // namespace hz
