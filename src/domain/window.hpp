#pragma once

#include "domain/limits.hpp"
#include "domain/types.hpp"

namespace skipper {

struct SearchWindows {
  Window intro;
  Window credits;
};

SearchWindows windowFor(double duration, const DetectionLimits& limits);

Window widen(const Window& window, double templateDuration, double duration, bool intro);

bool accepted(const TimeRange& range, const Window& window, const DetectionLimits& limits);

}  // namespace skipper
