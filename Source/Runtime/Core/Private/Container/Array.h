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
    explicit Array(uint32_t Count) { Resize(Count); }
    explicit Array(Span<T> values)
    {
        ASSERT(values.Data || !values.Count);
        Reserve(values.Count);
        for (uint32_t i = 0; i < values.Count; ++i)
        {
            ::new ((void*)(Data + i)) T(values.Data[i]);
        }
        stbds_arrsetlen(Data, values.Count);
    }

    Array(const Array& other): Array(Span<T>(other.GetData(), other.Size())) {}
    Array& operator=(const Array& other)
    {
        if (this != &other)
        {
            Array copy(other);
            *this = std::move(copy);
        }
        return *this;
    }

    Array(Array&& other) noexcept: Data(other.Data) { other.Data = nullptr; }
    Array& operator=(Array&& other) noexcept
    {
        if (this != &other)
        {
            Reset();
            Data = other.Data;
            other.Data = nullptr;
        }
        return *this;
    }

    ~Array() { Reset(); }

    uint32_t Size() const { return (uint32_t)stbds_arrlenu(Data); }
    uint32_t Capacity() const { return (uint32_t)stbds_arrcap(Data); }
    bool     Empty() const { return Size() == 0; }

    T*       GetData() { return Data; }
    const T* GetData() const { return Data; }
    T*       begin() { return Data; }
    const T* begin() const { return Data; }
    T*       end() { return Data ? Data + Size() : nullptr; }
    const T* end() const { return Data ? Data + Size() : nullptr; }

    T& operator[](uint32_t index)
    {
        ASSERT(index < Size());
        return Data[index];
    }
    const T& operator[](uint32_t index) const
    {
        ASSERT(index < Size());
        return Data[index];
    }

    void Reserve(uint32_t Count)
    {
        if (Count <= Capacity())
        {
            return;
        }
        // stb_ds realloc cannot relocate objects or preserve an over-aligned payload's offset.
        if constexpr (__is_trivially_copyable(T) && alignof(T) <= 16)
            stbds_arrsetcap(Data, Count);
        else
        {
            const uint32_t oldCount = Size();
            const uint32_t doubledCapacity = Capacity() * 2;
            T*             pNew = nullptr;
            stbds_arrsetcap(pNew, Count > doubledCapacity ? Count : doubledCapacity);
            for (uint32_t i = 0; i < oldCount; ++i)
                relocate(pNew + i, Data[i]);
            stbds_arrsetlen(pNew, oldCount);
            Reset();
            Data = pNew;
        }
    }
    void Resize(uint32_t Count)
    {
        const uint32_t oldCount = Size();
        DestroyFrom(Count);
        Reserve(Count);
        for (uint32_t i = oldCount; i < Count; ++i)
        {
            ::new ((void*)(Data + i)) T{};
        }
        stbds_arrsetlen(Data, Count);
    }
    template<typename... Args>
    T& EmplaceBack(Args&&... args)
    {
        const uint32_t Count = Size();
        if (Count == Capacity())
        {
            // Arguments may refer to elements invalidated by growth.
            T value(std::forward<Args>(args)...);
            Reserve(Count + 1);
            relocate(Data + Count, value);
        }
        else
            ::new ((void*)(Data + Count)) T(std::forward<Args>(args)...);
        stbds_arrsetlen(Data, Count + 1);
        return Data[Count];
    }
    void PushBack(const T& value) { EmplaceBack(value); }
    void PushBack(T&& value) { EmplaceBack(std::move(value)); }
    void PopBack()
    {
        ASSERT(!Empty());
        DestroyFrom(Size() - 1);
        stbds_arrsetlen(Data, Size() - 1);
    }
    void Clear()
    {
        DestroyFrom(0);
        stbds_arrsetlen(Data, 0);
    }
    void Reset()
    {
        Clear();
        stbds_arrfree(Data);
    }

private:
    static void relocate(T* pDestination, T& source)
    {
        if constexpr (__is_constructible(T, T &&))
            ::new ((void*)pDestination) T(std::move(source));
        else
            ::new ((void*)pDestination) T(source);
    }
    void DestroyFrom(uint32_t first)
    {
        if constexpr (!__is_trivially_destructible(T))
        {
            for (uint32_t i = Size(); i > first; --i)
            {
                Data[i - 1].~T();
            }
        }
    }

    T* Data = nullptr;
};
} // namespace hz
