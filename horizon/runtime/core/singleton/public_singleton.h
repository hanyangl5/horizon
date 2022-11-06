/*****************************************************************//**
 * \file   public_singleton.h
 * \brief  from https://github.com/BoomingTech/Pilot/blob/main/engine/source/runtime/core/base/public_singleton.h
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#pragma once

#include <type_traits>

namespace Horizon {

template <typename T> class PublicSingleton {
  protected:
    PublicSingleton() = default;
    virtual ~PublicSingleton() noexcept = default;

    PublicSingleton(const PublicSingleton &) = delete;
    PublicSingleton &operator=(const PublicSingleton &) = delete;
  public:
    static T &GetInstance() noexcept(std::is_nothrow_constructible<T>::value) {
        static T instance;
        return instance;
    }
};

} // namespace Horizon