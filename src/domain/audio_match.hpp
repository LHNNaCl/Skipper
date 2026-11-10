#pragma once

#include "domain/limits.hpp"
#include "domain/types.hpp"

#include <optional>
#include <vector>

namespace skipper {

struct AlignedPair {
  TimeRange left;
  TimeRange right;
};

std::optional<AlignedPair> matchFingerprints(const Fingerprint& lhs,
                                             const Fingerprint& rhs,
                                             const DetectionLimits& limits);

std::vector<std::optional<TimeRange>> bestPairwise(
    const std::vector<std::optional<Fingerprint>>& fingerprints,
    const std::vector<Window>& windows,
    const DetectionLimits& limits);

std::optional<TimeRange> matchTemplate(const Fingerprint& templ,
                                       const Fingerprint& target,
                                       const Window& window,
                                       const DetectionLimits& limits);

}  // namespace skipper
