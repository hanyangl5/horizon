/* Copyright (c) 2026 Horizon */
#pragma once

#define IMEMORY_FROM_HEADER
#include "Core/IMemory.h"

namespace hz
{
template<typename T>
class UniquePtr
{
public:
    using ElementType = T;
    using Pointer = T*;

    constexpr UniquePtr() noexcept = default;
    constexpr UniquePtr(decltype(nullptr)) noexcept {}
    explicit UniquePtr(Pointer Value) noexcept: Ptr(Value) {}

    UniquePtr(const UniquePtr&) = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;

    UniquePtr(UniquePtr&& Other) noexcept: Ptr(Other.Release()) {}
    UniquePtr& operator=(UniquePtr&& Other) noexcept
    {
        if (this != &Other)
        {
            Reset(Other.Release());
        }
        return *this;
    }

    UniquePtr& operator=(decltype(nullptr)) noexcept
    {
        Reset();
        return *this;
    }

    ~UniquePtr() { Reset(); }

    T&       operator*() const noexcept { return *Ptr; }
    Pointer  operator->() const noexcept { return Ptr; }
    explicit operator bool() const noexcept { return Ptr != nullptr; }

    Pointer Get() const noexcept { return Ptr; }

    Pointer Release() noexcept
    {
        Pointer Raw = Ptr;
        Ptr = nullptr;
        return Raw;
    }

    void Reset(Pointer NewPtr = nullptr) noexcept
    {
        Pointer Old = Ptr;
        Ptr = NewPtr;
        MemDeleteInternal(Old, __FILE__, __LINE__, __FUNCTION__);
    }

    void Swap(UniquePtr& Other) noexcept
    {
        Pointer Raw = Ptr;
        Ptr = Other.Ptr;
        Other.Ptr = Raw;
    }

private:
    Pointer Ptr = nullptr;
};

template<typename T, typename... Args>
UniquePtr<T> MakeUnique(Args&&... args)
{
    return UniquePtr<T>(MemNewInternal<T>(__FILE__, __LINE__, __FUNCTION__, std::forward<Args>(args)...));
}
} // namespace hz
