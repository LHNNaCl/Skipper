#include "domain/keep_ranges.hpp"

#include "domain/time.hpp"

#include <algorithm>

namespace skipper {

constexpr double kMinSideSec = 10;

std::vector<KeptPart> planKeepRanges(double duration,
                                     const std::optional<TimeRange>& intro,
                                     const std::optional<TimeRange>& credits,
                                     std::vector<double> keyframes) {
  if (duration < 0) duration = 0;

  struct Piece {
    int part = 0;
    double start = 0;
    double end = 0;
  };
  std::vector<Piece> logical;
  if (intro && intro->startSec > kTimeEpsilon && !nearlyLess(intro->startSec, kMinSideSec)) {
    logical.push_back(Piece{1, 0, intro->startSec});
  }
  const double bodyStart = intro ? intro->endSec : 0;
  const double bodyEnd = credits ? credits->startSec : duration;
  if (nearlyLess(bodyStart, bodyEnd)) logical.push_back(Piece{2, bodyStart, bodyEnd});
  if (credits) {
    const double tail = duration - credits->endSec;
    if (tail > kTimeEpsilon && !nearlyLess(tail, kMinSideSec)) {
      logical.push_back(Piece{3, credits->endSec, duration});
    }
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

  std::vector<KeptPart> parts;
  for (const Piece& piece : logical) {
    const double start = floorKey(piece.start);
    const double end = ceilKey(piece.end);
    if (nearlyLess(start, end)) parts.push_back(KeptPart{piece.part, TimeRange{start, end}});
  }
  return parts;
}

}  // namespace skipper
