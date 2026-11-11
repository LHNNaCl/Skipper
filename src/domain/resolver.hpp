#pragma once

#include "domain/limits.hpp"
#include "domain/types.hpp"

#include <optional>
#include <vector>

namespace skipper {

std::vector<EpisodePlan> resolve(const std::vector<EpisodeEvidence>& episodes,
                                 const std::optional<StandardEvidence>& standard,
                                 const DetectionLimits& limits,
                                 const DetectionMethods& methods = {});

}  // namespace skipper
