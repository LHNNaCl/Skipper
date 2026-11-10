#pragma once

#include "domain/limits.hpp"
#include "domain/types.hpp"

#include <optional>
#include <vector>

namespace skipper {

std::optional<TimeRange> pickChapter(const std::vector<Chapter>& chapters,
                                     const Window& window,
                                     bool intro,
                                     const DetectionLimits& limits);

}  // namespace skipper
