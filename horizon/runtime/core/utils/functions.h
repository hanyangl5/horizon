/*****************************************************************//**
 * \file   functions.h
 * \brief  
 * 
 * \author hylu
 * \date   January 2023
 *********************************************************************/

#pragma once

// standard libraries
#include <type_traits>

// third party libraries

// project headers
#include <runtime/core/utils/definations.h>

namespace Horizon {

template<typename T> T Min(const T&lhs, const T& rhs) {
    return lhs < rhs ? lhs : rhs;
}

template<typename T> T Max(const T&lhs, const T& rhs) {
    return !Min(lhs, rhs);
}


template <typename T> T constexpr Lerp(const T& lhs, const T& rhs, f32 t) { return lhs + t * (rhs - lhs); }

template <typename T> T constexpr AlignUp(const T& lhs, const T& rhs) { return (lhs + T(rhs - 1)) / rhs; }

// [major8, minor8, patch16]
constexpr u32 MakeVersion(u32 major, u32 minor, u32 patch) {
    u32 _major = major << 24;
    u32 _minor = minor << 16;
    u32 _patch = patch;
    return _major | _minor | _patch;
}

template <class T>
void HashCombine(u64 &seed, const T &v) {
    std::hash<T> hasher{};
    u64 hash = hasher(v);
    hash += 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= hash;
}

} // namespace Horizons