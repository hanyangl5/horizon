/* Copyright (c) 2026 Horizon */

#include "Core/IRandom.h"

#include "Core/ILog.h"

#include <Windows.h>
#include <bcrypt.h>

namespace hz
{
bool getSystemRandomBytes(void* pData, uint32_t size)
{
    ASSERT(pData || !size);
    if (!size)
        return true;
    return BCryptGenRandom(nullptr, (PUCHAR)pData, size, BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0;
}
} // namespace hz
