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

// third party libraries

// project headers
#include <runtime/core/utils/definations.h>
#include <runtime/core/math/3d/vector.hpp>

namespace Horizon::math {

template<u32 dimension_m, u32 dimension_n, typename T = f32>
class Matrix {
  public:

    constexpr Matrix() noexcept {
        static_assert(dimension_m <= MAX_DIMENSION && dimension_n <= MAX_DIMENSION);
        if constexpr (dimension_m == dimension_n) {
            SetIdentity();
        }
    }

    constexpr Matrix(const T (&_e)[dimension_m * dimension_n]) noexcept {
        static_assert(dimension_m <= MAX_DIMENSION && dimension_n <= MAX_DIMENSION);
        for (u32 i = 0; i < dimension_m; i++) {
            for (u32 j = 0; j < dimension_n; j++) {
                e[i * dimension_n + j] = _e[i * dimension_n + j];
            }
        }
    }

    constexpr Matrix(const T (&_e)[16]) noexcept {
        static_assert(dimension_m == 4 && dimension_n == 4);
        for (u32 i = 0; i < dimension_m; i++) {
            for (u32 j = 0; j < dimension_n; j++) {
                e[i * dimension_n + j] = _e[i * dimension_n + j];
            }
        }
    }

    constexpr Matrix(const Vector<dimension_n, T> *_e[dimension_m]) noexcept {
        for (u32 i = 0; i < dimension_m; i++) {
            for (u32 j = 0; j < dimension_n; j++) {
                e(i, j) = _e(i, j);
            }
        }
    }

    ~Matrix() noexcept {
    }

    constexpr T at(u32 i) const {
      assert(i <= dimension_m * dimension_n);
      return e[i]; 
    }

    constexpr T at(u32 row, u32 column) const {
      assert(row <= dimension_m && column <= dimension_n);
      return this->at(row * dimension_n + column); 
    }
    
    constexpr T& operator()(u32 i) {
      assert(i <= dimension_m * dimension_n);
      return e[i]; 
    }

    constexpr T& operator()(u32 row, u32 column) {
      assert(row <= dimension_m && column <= dimension_n);
      return (*this)(row * dimension_n + column); 
    }

        
    // overload operators
    constexpr auto operator+(const Matrix<dimension_m, dimension_n, T>& rhs) {

    }

    constexpr auto operator-(const Matrix<dimension_m, dimension_n, T>& rhs) {

    }

    constexpr auto operator*(const Matrix<dimension_m, dimension_n, T>& rhs) {

    }

    constexpr auto operator/(const Matrix<dimension_m, dimension_n, T>& rhs) {

    }

    constexpr auto& operator+=(const Matrix<dimension_m, dimension_n, T>& rhs) {

    }

    constexpr auto& operator-=(const Matrix<dimension_m, dimension_n, T>& rhs) {

    }

    constexpr auto& operator*=(const Matrix<dimension_m, dimension_n, T>& rhs) {

    }

    constexpr auto& operator/=(const Matrix<dimension_m, dimension_n, T>& rhs) {

    }

    // matrix vector multiplication
    constexpr auto operator*(const Vector<dimension_n, T>& rhs)
    {
        Vector<dimension_m, T> ret;
        for (u32 i = 0; i < dimension_m;i++) {
            for (u32 j = 0; j < dimension_n;j++){
                ret(i) += this->at(i, j) * rhs.at(j);
            }
        }
        return ret;
    }

    template<u32 dimension_m, u32 dimension_n, u32 dimension_o, typename T = f32>
    constexpr auto operator*(const Matrix<dimension_n, dimension_o, T>& rhs)
    {
        Matrix<dimension_m, dimension_o, T> ret;
        for (u32 i = 0; i < dimension_m; i++) {
            for (u32 j = 0; j < dimension_o; j++) {
                for (u32 k = 0; k < dimension_n; k++) {
                    ret(i, j) += this->at(i, k) * rhs.at(k, j);
                }
            }
        }
        return ret;
    }

    Matrix(const Matrix &rhs) noexcept = default;
    Matrix &operator=(const Matrix &rhs) noexcept = default;
    Matrix(Matrix &&rhs) noexcept = default;
    Matrix &operator=(Matrix &&rhs) noexcept = default;

    void SetIdentity() noexcept { 
        if constexpr (dimension_m != dimension_n) {
            // TODO(hylu): warning msg
            return;
        }
        for (u32 i = 0; i < dimension_m;i++) {
            (*this)(i, i) = T(1);
        }
    }

  private:
    std::array<T, dimension_m * dimension_n> e{}; // row major
};

}