/*****************************************************************//**
 * \file   singleton.h
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#pragma once

// standard libraries
#include <type_traits>
#include <memory>
// third party libraries

// project headers
#include <runtime/core/memory/memory.h>

namespace Horizon {

template <typename T> class Singleton {
  protected:
    Singleton() = default;
    virtual ~Singleton() noexcept = default;

    Singleton(const Singleton &) = delete;
    Singleton &operator=(const Singleton &) = delete;
  public:
    static T &get() noexcept(std::is_nothrow_constructible<T>::value) {
        static T instance;
        return instance;
    }
};

} // namespace Horizon