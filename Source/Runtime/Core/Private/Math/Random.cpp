/* Copyright (c) 2026 Horizon */

#include "Core/IRandom.h"

#include "Core/ILog.h"

#include <Windows.h>
#include <bcrypt.h>

namespace hz
{
bool GetSystemRandomBytes(void* Data, uint32_t Size)
{
    ASSERT(Data || !Size);
    if (!Size)
        return true;
    return BCryptGenRandom(nullptr, (PUCHAR)Data, Size, BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0;
}
} // namespace hz
