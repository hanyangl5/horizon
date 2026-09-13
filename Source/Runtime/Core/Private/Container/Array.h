/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/ILog.h"
#include "Core/ISpan.h"

#include <ThirdParty/stb/stb_ds.h>

namespace hz
{
template<typename T>
class Array
{
public:
    Array() = default;
    explicit Array(uint32_t count) { resize(count); }
    explicit Array(Span<T> values)
    {
        ASSERT(values.pData || !values.count);
        reserve(values.count);
        for (uint32_t i = 0; i < values.count; ++i)
            ::new ((void*)(pData + i)) T(values.pData[i]);
        stbds_arrsetlen(pData, values.count);
    }

    Array(const Array& other): Array(Span<T>(other.data(), other.size())) {}
    Array& operator=(const Array& other)
    {
        if (this != &other)
        {
            Array copy(other);
            *this = std::move(copy);
        }
        return *this;
    }

    Array(Array&& other) noexcept: pData(other.pData) { other.pData = nullptr; }
    Array& operator=(Array&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            pData = other.pData;
            other.pData = nullptr;
        }
        return *this;
    }

    ~Array() { reset(); }

    uint32_t size() const { return (uint32_t)stbds_arrlenu(pData); }
    uint32_t capacity() const { return (uint32_t)stbds_arrcap(pData); }
    bool     empty() const { return size() == 0; }

    T*       data() { return pData; }
    const T* data() const { return pData; }
    T*       begin() { return pData; }
    const T* begin() const { return pData; }
    T*       end() { return pData ? pData + size() : nullptr; }
    const T* end() const { return pData ? pData + size() : nullptr; }

    T& operator[](uint32_t index)
    {
        ASSERT(index < size());
        return pData[index];
    }
    const T& operator[](uint32_t index) const
    {
        ASSERT(index < size());
        return pData[index];
    }

    void reserve(uint32_t count)
    {
        if (count <= capacity())
            return;
        // stb_ds realloc cannot relocate objects or preserve an over-aligned payload's offset.
        if constexpr (__is_trivially_copyable(T) && alignof(T) <= 16)
            stbds_arrsetcap(pData, count);
        else
        {
            const uint32_t oldCount = size();
            const uint32_t doubledCapacity = capacity() * 2;
            T*             pNew = nullptr;
            stbds_arrsetcap(pNew, count > doubledCapacity ? count : doubledCapacity);
            for (uint32_t i = 0; i < oldCount; ++i)
                relocate(pNew + i, pData[i]);
            stbds_arrsetlen(pNew, oldCount);
            reset();
            pData = pNew;
        }
    }
    void resize(uint32_t count)
    {
        const uint32_t oldCount = size();
        destroyFrom(count);
        reserve(count);
        for (uint32_t i = oldCount; i < count; ++i)
            ::new ((void*)(pData + i)) T{};
        stbds_arrsetlen(pData, count);
    }
    template<typename... Args>
    T& emplaceBack(Args&&... args)
    {
        const uint32_t count = size();
        if (count == capacity())
        {
            // Arguments may refer to elements invalidated by growth.
            T value(std::forward<Args>(args)...);
            reserve(count + 1);
            relocate(pData + count, value);
        }
        else
            ::new ((void*)(pData + count)) T(std::forward<Args>(args)...);
        stbds_arrsetlen(pData, count + 1);
        return pData[count];
    }
    void pushBack(const T& value) { emplaceBack(value); }
    void pushBack(T&& value) { emplaceBack(std::move(value)); }
    void popBack()
    {
        ASSERT(!empty());
        destroyFrom(size() - 1);
        stbds_arrsetlen(pData, size() - 1);
    }
    void clear()
    {
        destroyFrom(0);
        stbds_arrsetlen(pData, 0);
    }
    void reset()
    {
        clear();
        stbds_arrfree(pData);
    }

private:
    static void relocate(T* pDestination, T& source)
    {
        if constexpr (__is_constructible(T, T&&))
            ::new ((void*)pDestination) T(std::move(source));
        else
            ::new ((void*)pDestination) T(source);
    }
    void destroyFrom(uint32_t first)
    {
        if constexpr (!__is_trivially_destructible(T))
        {
            for (uint32_t i = size(); i > first; --i)
                pData[i - 1].~T();
        }
    }

    T* pData = nullptr;
};
} // namespace hz
