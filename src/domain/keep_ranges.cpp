#include "domain/keep_ranges.hpp"

#include "domain/time.hpp"

#include <algorithm>

namespace skipper {

std::vector<TimeRange> planKeepRanges(double duration,
                                      const std::optional<TimeRange>& intro,
                                      const std::optional<TimeRange>& credits,
                                      std::vector<double> keyframes) {
  if (duration < 0) duration = 0;

  struct Piece {
    double start = 0;
    double end = 0;
  };
  std::vector<Piece> logical;
  if (intro && intro->startSec > kTimeEpsilon) logical.push_back(Piece{0, intro->startSec});
  const double bodyStart = intro ? intro->endSec : 0;
  const double bodyEnd = credits ? credits->startSec : duration;
  if (nearlyLess(bodyStart, bodyEnd)) logical.push_back(Piece{bodyStart, bodyEnd});
  if (credits && duration - credits->endSec > kTimeEpsilon) {
    logical.push_back(Piece{credits->endSec, duration});
  }

  keyframes.push_back(0);
  keyframes.push_back(duration);
  std::sort(keyframes.begin(), keyframes.end());
  std::vector<double> unique;
  for (double time : keyframes) {
    if (unique.empty() || !nearlyEqual(unique.back(), time)) unique.push_back(time);
  }

  auto floorKey = [&](double time) {
    double best = 0;
    bool found = false;
    for (double key : unique) {
      if (key <= time + kTimeEpsilon) {
        best = key;
        found = true;
      }
    }
    return found ? best : 0.0;
  };
  auto ceilKey = [&](double time) {
    for (double key : unique) {
      if (key + kTimeEpsilon >= time) return key;
    }
    return duration;
  };

  std::vector<TimeRange> aligned;
  for (const Piece& piece : logical) {
    const double start = floorKey(piece.start);
    const double end = ceilKey(piece.end);
    if (nearlyLess(start, end)) aligned.push_back(TimeRange{start, end});
  }
  std::sort(aligned.begin(), aligned.end(), [](const TimeRange& a, const TimeRange& b) {
    return a.startSec < b.startSec;
  });

  std::vector<TimeRange> merged;
  for (const TimeRange& range : aligned) {
    if (!merged.empty() && !nearlyLess(merged.back().endSec, range.startSec)) {
      merged.back().endSec = std::max(merged.back().endSec, range.endSec);
    } else {
      merged.push_back(range);
    }
  }
  return merged;
}

}  // namespace skipper
