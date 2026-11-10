#pragma once

#include <optional>
#include <string>

namespace skipper {

struct TimeParse {
  bool empty = false;
  bool ok = false;
  double seconds = 0;
};

TimeParse parseTime(const std::string& text);
std::string formatTime(double seconds);

}  // namespace skipper
