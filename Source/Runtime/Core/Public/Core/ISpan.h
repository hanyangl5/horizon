/* Copyright (c) 2026 Horizon */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <initializer_list>

namespace hz
{
template<typename T>
struct Span
{
    constexpr Span() = default;
    constexpr Span(const T* pValues, uint32_t valueCount): Data(pValues), Count(valueCount) {}
    constexpr Span(std::initializer_list<T> values): Data(values.begin()), Count((uint32_t)values.size()) {}

    template<size_t N>
    constexpr Span(const T (&values)[N]): Data(values), Count((uint32_t)N)
    {
    }

    constexpr const T* begin() const { return Data; }
    constexpr const T* end() const { return Data ? Data + Count : nullptr; }

    const T* Data = nullptr;
    uint32_t Count = 0;
};

} // namespace hz
