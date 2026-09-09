/* Copyright (c) 2026 Horizon */
#pragma once

#define IMEMORY_FROM_HEADER
#include "Core/IMemory.h"

namespace hz
{
template<typename T>
class unique_ptr
{
public:
    using element_type = T;
    using pointer = T*;

    constexpr unique_ptr() noexcept = default;
    constexpr unique_ptr(decltype(nullptr)) noexcept {}
    explicit unique_ptr(pointer ptr) noexcept: pPtr(ptr) {}

    unique_ptr(const unique_ptr&) = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;

    unique_ptr(unique_ptr&& other) noexcept: pPtr(other.release()) {}
    unique_ptr& operator=(unique_ptr&& other) noexcept
    {
        if (this != &other)
            reset(other.release());
        return *this;
    }

    unique_ptr& operator=(decltype(nullptr)) noexcept
    {
        reset();
        return *this;
    }

    ~unique_ptr() { reset(); }

    T& operator*() const noexcept { return *pPtr; }
    pointer operator->() const noexcept { return pPtr; }
    explicit operator bool() const noexcept { return pPtr != nullptr; }

    pointer get() const noexcept { return pPtr; }

    pointer release() noexcept
    {
        pointer ptr = pPtr;
        pPtr = nullptr;
        return ptr;
    }

    void reset(pointer ptr = nullptr) noexcept
    {
        pointer old = pPtr;
        pPtr = ptr;
        tf_delete_internal(old, __FILE__, __LINE__, __FUNCTION__);
    }

    void swap(unique_ptr& other) noexcept
    {
        pointer ptr = pPtr;
        pPtr = other.pPtr;
        other.pPtr = ptr;
    }

private:
    pointer pPtr = nullptr;
};

template<typename T, typename... Args>
unique_ptr<T> make_unique(Args&&... args)
{
    return unique_ptr<T>(tf_new_internal<T>(__FILE__, __LINE__, __FUNCTION__, std::forward<Args>(args)...));
}
} // namespace hz
