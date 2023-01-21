/*****************************************************************//**
 * \file   FILE_NAME
 * \brief  BRIEF
 * 
 * \author XXX
 * \date   XXX
 *********************************************************************/

#pragma once

// standard libraries
#include <array>
#include <iostream>
#include <cassert>

// third party libraries

// project headers
#include <runtime/core/utils/definations.h>

namespace Horizon::math {

template<u32 dimension, typename T = f32>
class Vector {
  public:
    constexpr Vector() noexcept {
      static_assert(dimension <= MAX_DIMENSION);
    }

    constexpr Vector(const T(&_e)[dimension]) noexcept {
      static_assert(dimension <= MAX_DIMENSION);
      for (u32 i = 0; i < dimension;i++) {
        e[i] = _e[i];
      }
    };

    // Vector2
    constexpr Vector(const T &x, const T &y) noexcept {
      static_assert(dimension == 2);
      e[0] = x;
      e[1] = y;
    };

    // Vector3
    constexpr Vector(const T &x, const T &y, const T &z) noexcept {
      static_assert(dimension == 3);
      e[0] = x;
      e[1] = y;
      e[2] = z;
    };

    // constrcut Vector3 by [Vector2, z]
    constexpr Vector(const Vector<2, T> &xy, const T &z) noexcept {
      static_assert(dimension == 3);
      e[0] = xy.at(0);
      e[1] = xy.at(1);
      e[2] = z;
    };

    // Vector4
    constexpr Vector(const T &x, const T &y, const T &z, const T &w) noexcept {
      static_assert(dimension == 4);
      e[0] = x;
      e[1] = y;
      e[2] = z;
      e[3] = w;
    };

    // constrcut Vector4 by [vec2, y, z]
    constexpr Vector(const Vector<2, T> &xy, const T &z, const T &w) noexcept {
      static_assert(dimension == 4);
      e[0] = xy.at(0);
      e[1] = xy.at(1);
      e[2] = z;
      e[3] = w;
    };

    // constrcut Vector4 by [vec3, z]
    constexpr Vector(const Vector<3, T>& xyz, const T &w) noexcept {
      static_assert(dimension == 4);
      e[0] = xyz.at(0);
      e[1] = xyz.at(1);
      e[2] = xyz.at(2);
      e[3] = w;
    };

    constexpr Vector(const Vector &rhs) noexcept = default;
    constexpr Vector &operator=(const Vector &rhs) noexcept = default;
    constexpr Vector(Vector &&rhs) noexcept = default;
    constexpr Vector &operator=(Vector &&rhs) noexcept = default;

    ~Vector() noexcept {
      static_assert(dimension <= MAX_DIMENSION);
    }

    constexpr const T& at(u32 i) const {
      assert(i <= dimension);
      return e[i]; 
    }

    constexpr T& at(u32 i) {
      assert(i <= dimension);
      return e[i]; 
    }

    constexpr T& x() {
      static_assert(dimension >= 1);
      return at(0);
    }

    constexpr T& y() {
      static_assert(dimension >= 2);
      return at(1);
    }

    constexpr T& z() {
      static_assert(dimension >= 3);
      return at(2);
    }

    constexpr T& w() {
      static_assert(dimension >= 4);
      return at(3);
    }

    constexpr const T &x() const {
      static_assert(dimension >= 1);
      return at(0);
    }

    constexpr const T &y() const  {
      static_assert(dimension >= 2);
      return at(1);
    }

    constexpr const T &z() const {
      static_assert(dimension >= 3);
      return at(2);
    }

    constexpr const T &w() const {
      static_assert(dimension >= 4);
      return at(3);
    }

    constexpr T &r() {
      static_assert(dimension >= 1);
      return at(0);
    }

    constexpr T &g() {
      static_assert(dimension >= 2);
      return at(1);
    }

    constexpr T &b() {
      static_assert(dimension >= 3);
      return at(2);
    }

    constexpr T &a() {
      static_assert(dimension >= 4);
      return at(3);
    }

    constexpr const T &r() const {
      static_assert(dimension >= 1);
      return at(0);
    }

    constexpr const T &g() const {
      static_assert(dimension >= 2);
      return at(1);
    }

    constexpr const T &b() const {
      static_assert(dimension >= 3);
      return at(2);
    }

    constexpr const T &a() const {
      static_assert(dimension >= 4);
      return at(3);
    }

  private:
  std::array<T, dimension> e{};
};

}