// https://github.com/BoomingTech/Pilot/blob/main/engine/source/runtime/core/base/public_singleton.h
#pragma once

#include <type_traits>

namespace Horizon
{

    template<typename T>
    class PublicSingleton
    {
    protected:
        PublicSingleton() = default;

    public:
        static T& GetInstance() noexcept(std::is_nothrow_constructible<T>::value)
        {
            static T instance;
            return instance;
        }
        virtual ~PublicSingleton() noexcept = default;
        PublicSingleton(const PublicSingleton&) = delete;
        PublicSingleton& operator=(const PublicSingleton&) = delete;
    };
} // namespace Horizon