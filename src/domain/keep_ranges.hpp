#pragma once

#include "domain/types.hpp"

#include <optional>
#include <vector>

namespace skipper {

struct KeptPart {
  int part = 0;
  TimeRange range;
};

std::vector<KeptPart> planKeepRanges(double duration,
                                     const std::optional<TimeRange>& intro,
                                     const std::optional<TimeRange>& credits,
                                     std::vector<double> keyframes);

}  // namespace skipper
