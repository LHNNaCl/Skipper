#pragma once

#include "domain/limits.hpp"
#include "domain/types.hpp"

#include <optional>
#include <vector>

namespace skipper {

std::optional<TimeRange> pickBlackFrame(std::vector<VisualSample> samples,
                                        const Window& window,
                                        double duration,
                                        const DetectionLimits& limits);

}  // namespace skipper
