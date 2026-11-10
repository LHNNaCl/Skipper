#include "domain/black_frame.hpp"

#include "domain/time.hpp"
#include "domain/window.hpp"

#include <algorithm>
#include <cmath>

namespace skipper {

std::optional<TimeRange> pickBlackFrame(std::vector<VisualSample> samples,
                                        const Window& window,
                                        double duration,
                                        const DetectionLimits& limits) {
  std::sort(samples.begin(), samples.end(), [](const VisualSample& a, const VisualSample& b) {
    return a.timeSec < b.timeSec;
  });

  auto qualified = [&](const VisualSample& sample) {
    if (!nearlyLess(sample.blackPercent, limits.blackPercent)) return true;
    if (!nearlyLess(sample.blackPercent, limits.darkPercent) &&
        !nearlyLess(limits.maxSaturation, sample.saturation)) {
      return true;
    }
    return false;
  };

  struct Run {
    double start = 0;
    double end = 0;
  };
  std::vector<Run> runs;
  for (const VisualSample& sample : samples) {
    if (!qualified(sample)) continue;
    if (!runs.empty() && !nearlyLess(limits.visualGapSec, sample.timeSec - runs.back().end)) {
      runs.back().end = sample.timeSec;
    } else {
      runs.push_back(Run{sample.timeSec, sample.timeSec});
    }
  }

  std::vector<Run> merged;
  for (const Run& run : runs) {
    if (!merged.empty() && !nearlyLess(limits.mergeGapSec, run.start - merged.back().end)) {
      merged.back().end = run.end;
    } else {
      merged.push_back(run);
    }
  }

  std::optional<TimeRange> best;
  for (const Run& run : merged) {
    const TimeRange range{run.start, run.end};
    if (!accepted(range, window, limits)) continue;
    if (!best) {
      best = range;
      continue;
    }
    const double bestDistance = std::abs(duration - best->endSec);
    const double distance = std::abs(duration - range.endSec);
    const double bestLen = best->endSec - best->startSec;
    const double len = range.endSec - range.startSec;
    if (nearlyLess(distance, bestDistance) ||
        (nearlyEqual(distance, bestDistance) && nearlyLess(bestLen, len))) {
      best = range;
    }
  }
  return best;
}

}  // namespace skipper
