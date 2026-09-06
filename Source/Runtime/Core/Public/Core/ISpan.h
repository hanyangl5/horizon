/* Copyright (c) 2026 Horizon */
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace hz
{
template<typename T>
struct Span
{
    constexpr Span() = default;
    constexpr Span(const T* pValues, uint32_t valueCount): pData(pValues), count(valueCount) {}

    template<size_t N>
    constexpr Span(const T (&values)[N]): pData(values), count((uint32_t)N)
    {
    }

    constexpr const T* begin() const { return pData; }
    constexpr const T* end() const { return pData ? pData + count : nullptr; }

    const T* pData = nullptr;
    uint32_t count = 0;
};

} // namespace hz
