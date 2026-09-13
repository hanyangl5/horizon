/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/ILog.h"

namespace hz
{
template<typename T, uint32_t N>
struct FixedArray
{
    T values[N];

    constexpr uint32_t size() const { return N; }
    constexpr bool     empty() const { return false; }
    constexpr T*       data() { return values; }
    constexpr const T* data() const { return values; }
    constexpr T*       begin() { return values; }
    constexpr const T* begin() const { return values; }
    constexpr T*       end() { return values + N; }
    constexpr const T* end() const { return values + N; }

    constexpr T& operator[](uint32_t index)
    {
        ASSERT(index < N);
        return values[index];
    }
    constexpr const T& operator[](uint32_t index) const
    {
        ASSERT(index < N);
        return values[index];
    }
};

template<typename T>
struct FixedArray<T, 0>
{
    constexpr uint32_t size() const { return 0; }
    constexpr bool     empty() const { return true; }
    constexpr T*       data() { return nullptr; }
    constexpr const T* data() const { return nullptr; }
    constexpr T*       begin() { return nullptr; }
    constexpr const T* begin() const { return nullptr; }
    constexpr T*       end() { return nullptr; }
    constexpr const T* end() const { return nullptr; }

    constexpr T& operator[](uint32_t index)
    {
        ASSERT(index < size());
        return data()[index];
    }
    constexpr const T& operator[](uint32_t index) const
    {
        ASSERT(index < size());
        return data()[index];
    }
};
} // namespace hz
