/*****************************************************************//**
 * \file   cemath.h
 * \brief  const expression math functions
 * 
 * \author hylu
 * \date   December 2022
 *********************************************************************/

#pragma once

#include <random>
#include <type_traits>

namespace Horizon::math::CE {

// template <typename T> typename constexpr bool IsOdd(T x) {
//     if constexpr (!std::is_integral<T>()) {
//         static_assert(false);
//     }
//     return x & 0x1;
// }
// template <typename T> typename constexpr bool IsEven(T x) { return !CIsOdd<T>(x); }

// //template <typename T> inline T Lerp(T a, T b, f32 t) { return a + t * (b - a); }

// template <typename T> constexpr T Log(T base, T x) {}

// template <typename T> constexpr T LogE(T x) {}

//template <typename T> using CLn = CLogE<T>;

//template <typename T> constexpr T exp_cf_recur(const T x, const int depth) noexcept {
//    return (depth < GCEM_EXP_MAX_ITER_SMALL ? // if
//                depth == 1 ? T(1) - x / exp_cf_recur(x, depth + 1)
//                           : T(1) + x / T(depth - 1) - x / depth / exp_cf_recur(x, depth + 1)
//                                            :
//                                            // else
//                T(1));
//}
//
//template <typename T> constexpr T exp_cf(const T x) noexcept { return (T(1) / exp_cf_recur(x, 1)); }
//
//template <typename T> constexpr T exp_split(const T x) noexcept {
//    return (pow_integral(GCEM_E, find_whole(x)) * exp_cf(find_fraction(x)));
//}
//
//constexpr double pow(double x, int y)
//{
//    return y == 0 ? 1.0 : x * pow(x, y-1);
//}
//
//constexpr int factorial(int x)
//{
//    return x == 0 ? 1 : x * factorial(x-1);
//}
//
//constexpr double exp(double x)
//{
//    return 1.0 + x + pow(x,2)/factorial(2) + pow(x, 3)/factorial(3)
//        + pow(x, 4)/factorial(4) + pow(x, 5)/factorial(5)
//        + pow(x,6)/factorial(6) + pow(x, 7)/factorial(7)
//        + pow(x, 8)/factorial(8) + pow(x, 9)/factorial(9);
//}

// template <typename T> constexpr T Exp(T x) {
//     if constexpr (!std::is_floating_point<T>::value) {
//         static_assert(false);
//     }
// }

// template <typename T> constexpr T Pow2(T exp) {
//     if constexpr (!std::is_integral<T>::value) {
//         static_assert(false);
//     }
//     // pow(0,0)

//     // pow(base, 0)

//     // pow(2, exp)
//     if constexpr (std::is_integral<T>()) {
//         return T{T{1} << exp};
//     }
// }

// template <typename T> constexpr T Pow(T base, T exp) {
//     if constexpr (!std::is_integral<T>::value && !std::is_floating_point<T>::value) {
//         static_assert(false);
//     }
//     // pow(0,0)

//     // pow(base, 0)

//     // pow(2, exp)
//     if (std::is_integral<T>()) {
//         if constexpr (base == 2) {
//             return T{T{1} << exp};
//         } 
     
//     }


//     //return T{CExp<T>(exp * CLogE<T>(base)};
//     //// TODO(hylu): optimize with compiler intrincs
//     //template <typename T> constexpr T ipow(T num, unsigned int pow) {
//     //    return (pow >= sizeof(unsigned int) * 8) ? 0 : pow == 0 ? 1 : num * ipow(num, pow - 1);
//     //}
//     //{
//     //    pow_impl(T x, T y) {
//     //        return x < 0 ? is_odd(y) ? -sprout::math::exp(y * sprout::math::log(-x))
//     //                                 : sprout::math::exp(y * sprout::math::log(-x))
//     //                     : sprout::math::exp(y * sprout::math::log(x));
//     //    }
//     //}
// }

// constexpr u32 PowI(u32 base,u32 exp) {
//     return (exp >= sizeof(unsigned int) * 8) ? 0 : exp == 0 ? 1 : base * PowI(base, exp - 1);
// }

//constexpr double pow(double x, int y) { return y == 0 ? 1.0 : x * pow(x, y - 1); }
//
//constexpr int factorial(int x) { return x == 0 ? 1 : x * factorial(x - 1); }
//
//constexpr double exp(double x) {
//    return 1.0 + x + pow(x, 2) / factorial(2) + pow(x, 3) / factorial(3) + pow(x, 4) / factorial(4) +
//           pow(x, 5) / factorial(5) + pow(x, 6) / factorial(6) + pow(x, 7) / factorial(7) + pow(x, 8) / factorial(8) +
//           pow(x, 9) / factorial(9);
//}

// template <typename T, u32 size, u32 dimension = 1> auto UniformDistribution() {
//     if constexpr (!std::is_integral<T>() && !std::is_floating_point<T>()) {
//         static_assert(false);
//     }
//     std::uniform_real_distribution<T> rnd_dist(0.0, 1.0);
//     std::default_random_engine generator;
//     constexpr u32 array_size = CE::PowI(size, dimension);
//     Container::FixedArray<T, array_size> sequence{};
//     for (u32 i = 0; i < array_size; i++) {
//         sequence[i] = rnd_dist(generator);
//     }
//     return sequence;
// }


} // namespace Horizon::math::CE
