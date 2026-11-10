#pragma once

#include "domain/types.hpp"

#include <optional>
#include <vector>

namespace skipper {

std::vector<TimeRange> planKeepRanges(double duration,
                                      const std::optional<TimeRange>& intro,
                                      const std::optional<TimeRange>& credits,
                                      std::vector<double> keyframes);

}  // namespace skipper
