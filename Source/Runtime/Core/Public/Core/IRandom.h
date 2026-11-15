#pragma once

#include "Core/IConfig.h"

#include "../Private/Math/Random.h"

namespace hz
{
// Fills the requested range from the system random source. On failure, discard its contents.
bool GetSystemRandomBytes(void* Data, uint32_t Size);
} // namespace hz
