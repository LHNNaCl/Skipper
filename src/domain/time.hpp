#pragma once

#include <cmath>

namespace skipper {

inline constexpr double kTimeEpsilon = 0.001;
inline constexpr double kWindowSlack = 0.128;

inline bool nearlyEqual(double a, double b) {
  return std::abs(a - b) <= kTimeEpsilon;
}

inline bool nearlyLess(double a, double b) {
  return a < b - kTimeEpsilon;
}

}  // namespace skipper
