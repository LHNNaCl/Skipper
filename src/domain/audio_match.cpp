#include "domain/audio_match.hpp"

#include "domain/time.hpp"
#include "domain/window.hpp"

#include <bit>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace skipper {
namespace {

struct RunChoice {
  double duration = -1;
  double leftStart = 0;
  int leftFirst = 0;
  int leftLast = 0;
  int offset = 0;
  bool found = false;
};

int hammingBits(std::uint32_t left, std::uint32_t right) {
  return std::popcount(left ^ right);
}

void considerRun(RunChoice& best,
                 const Fingerprint& lhs,
                 int offset,
                 int first,
                 int last) {
  if (last < first) return;
  const double duration = static_cast<double>(last - first + 1) * lhs.secondsPerPoint;
  const double leftStart = lhs.windowStartSec + static_cast<double>(first) * lhs.secondsPerPoint;
  const bool longer = !best.found || nearlyLess(best.duration, duration);
  const bool tie = best.found && nearlyEqual(best.duration, duration) && nearlyLess(leftStart, best.leftStart);
  if (longer || tie) {
    best.found = true;
    best.duration = duration;
    best.leftStart = leftStart;
    best.leftFirst = first;
    best.leftLast = last;
    best.offset = offset;
  }
}

void scoreOffset(RunChoice& best,
                 const Fingerprint& lhs,
                 const Fingerprint& rhs,
                 int offset,
                 int maxBits,
                 int allowedGap) {
  const int leftCount = static_cast<int>(lhs.points.size());
  const int rightCount = static_cast<int>(rhs.points.size());
  std::vector<int> lefts;
  lefts.reserve(static_cast<std::size_t>(leftCount));
  for (int i = 0; i < leftCount; ++i) {
    const int j = i + offset;
    if (j < 0 || j >= rightCount) continue;
    if (hammingBits(lhs.points[static_cast<std::size_t>(i)],
                    rhs.points[static_cast<std::size_t>(j)]) <= maxBits) {
      lefts.push_back(i);
    }
  }
  if (lefts.empty()) return;

  int runStart = 0;
  for (int k = 1; k <= static_cast<int>(lefts.size()); ++k) {
    if (k == static_cast<int>(lefts.size()) ||
        lefts[static_cast<std::size_t>(k)] - lefts[static_cast<std::size_t>(k - 1)] > allowedGap) {
      considerRun(best, lhs, offset, lefts[static_cast<std::size_t>(runStart)],
                  lefts[static_cast<std::size_t>(k - 1)]);
      runStart = k;
    }
  }
}

}  // namespace

std::optional<AlignedPair> matchFingerprints(const Fingerprint& lhs,
                                             const Fingerprint& rhs,
                                             const DetectionLimits& limits) {
  if (lhs.points.empty() || rhs.points.empty()) return std::nullopt;
  if (!(lhs.secondsPerPoint > 0) || !(rhs.secondsPerPoint > 0)) return std::nullopt;

  std::unordered_map<std::uint32_t, std::vector<int>> index;
  index.reserve(rhs.points.size());
  for (int j = 0; j < static_cast<int>(rhs.points.size()); ++j) {
    index[rhs.points[static_cast<std::size_t>(j)]].push_back(j);
  }

  std::unordered_set<std::uint32_t> ignored;
  for (const auto& entry : index) {
    if (static_cast<int>(entry.second.size()) > limits.maxBucketSize) ignored.insert(entry.first);
  }

  std::unordered_map<int, int> offsetSeeds;
  for (int i = 0; i < static_cast<int>(lhs.points.size()); ++i) {
    const auto value = lhs.points[static_cast<std::size_t>(i)];
    if (ignored.count(value) != 0) continue;
    const auto found = index.find(value);
    if (found == index.end()) continue;
    for (int j : found->second) ++offsetSeeds[j - i];
  }

  const int maxBits = std::max(0, limits.maxHammingBits);
  if (maxBits > 0) {
    // Sparse near-match seeding so offsets with few exact equals still appear.
    constexpr int kStride = 4;
    const int leftCount = static_cast<int>(lhs.points.size());
    const int rightCount = static_cast<int>(rhs.points.size());
    const int minHits = std::max(3, leftCount / (kStride * 20));
    std::unordered_map<int, int> nearSeeds;
    for (int i = 0; i < leftCount; i += kStride) {
      const auto left = lhs.points[static_cast<std::size_t>(i)];
      if (ignored.count(left) != 0) continue;
      for (int j = 0; j < rightCount; ++j) {
        if (hammingBits(left, rhs.points[static_cast<std::size_t>(j)]) <= maxBits) {
          ++nearSeeds[j - i];
        }
      }
    }
    for (const auto& entry : nearSeeds) {
      if (entry.second >= minHits) offsetSeeds.emplace(entry.first, entry.second);
    }
  }

  RunChoice best;
  const int allowedGap = 1 + std::max(0, limits.maxGapPoints);
  for (const auto& entry : offsetSeeds) {
    scoreOffset(best, lhs, rhs, entry.first, maxBits, allowedGap);
  }

  if (!best.found) return std::nullopt;

  AlignedPair pair;
  pair.left.startSec = lhs.windowStartSec + static_cast<double>(best.leftFirst) * lhs.secondsPerPoint;
  pair.left.endSec = lhs.windowStartSec + static_cast<double>(best.leftLast + 1) * lhs.secondsPerPoint;
  const int rightFirst = best.leftFirst + best.offset;
  const int rightLast = best.leftLast + best.offset;
  pair.right.startSec = rhs.windowStartSec + static_cast<double>(rightFirst) * rhs.secondsPerPoint;
  pair.right.endSec = rhs.windowStartSec + static_cast<double>(rightLast + 1) * rhs.secondsPerPoint;
  return pair;
}

std::vector<std::optional<TimeRange>> bestPairwise(
    const std::vector<std::optional<Fingerprint>>& fingerprints,
    const std::vector<Window>& windows,
    const DetectionLimits& limits) {
  const int count = static_cast<int>(fingerprints.size());
  std::vector<std::optional<TimeRange>> best(static_cast<std::size_t>(count));
  std::vector<double> bestDuration(static_cast<std::size_t>(count), -1);
  int available = 0;
  for (const auto& fingerprint : fingerprints) {
    if (fingerprint && !fingerprint->points.empty()) ++available;
  }
  if (available < 2) return best;

  auto consider = [&](int index, const TimeRange& range) {
    const double duration = range.endSec - range.startSec;
    auto& slot = best[static_cast<std::size_t>(index)];
    auto& kept = bestDuration[static_cast<std::size_t>(index)];
    const bool longer = !slot || nearlyLess(kept, duration);
    const bool tie = slot && nearlyEqual(kept, duration) && nearlyLess(range.startSec, slot->startSec);
    if (longer || tie) {
      slot = range;
      kept = duration;
    }
  };

  for (int left = 0; left < count; ++left) {
    if (!fingerprints[static_cast<std::size_t>(left)] ||
        fingerprints[static_cast<std::size_t>(left)]->points.empty()) {
      continue;
    }
    for (int right = left + 1; right < count; ++right) {
      if (!fingerprints[static_cast<std::size_t>(right)] ||
          fingerprints[static_cast<std::size_t>(right)]->points.empty()) {
        continue;
      }
      const auto match = matchFingerprints(*fingerprints[static_cast<std::size_t>(left)],
                                           *fingerprints[static_cast<std::size_t>(right)],
                                           limits);
      if (!match) continue;
      const double duration = match->left.endSec - match->left.startSec;
      const double leftWindow = windows[static_cast<std::size_t>(left)].endSec -
                                windows[static_cast<std::size_t>(left)].startSec;
      const double rightWindow = windows[static_cast<std::size_t>(right)].endSec -
                                 windows[static_cast<std::size_t>(right)].startSec;
      const double shorter = std::min(leftWindow, rightWindow);
      if (shorter > 0 && !nearlyLess(duration, limits.fullWindowRatio * shorter)) continue;
      if (!accepted(match->left, windows[static_cast<std::size_t>(left)], limits)) continue;
      if (!accepted(match->right, windows[static_cast<std::size_t>(right)], limits)) continue;
      consider(left, match->left);
      consider(right, match->right);
    }
  }
  return best;
}

std::optional<TimeRange> matchTemplate(const Fingerprint& templ,
                                       const Fingerprint& target,
                                       const Window& window,
                                       const DetectionLimits& limits) {
  const auto match = matchFingerprints(templ, target, limits);
  if (!match) return std::nullopt;
  const double templateDuration = static_cast<double>(templ.points.size()) * templ.secondsPerPoint;
  if (!(templateDuration > 0)) return std::nullopt;
  const double overlap = match->left.endSec - match->left.startSec;
  const double coverage = overlap / templateDuration;
  if (coverage < limits.templateCoverage) return std::nullopt;

  TimeRange aligned;
  aligned.startSec = match->right.startSec;
  aligned.endSec = aligned.startSec + templateDuration;
  if (nearlyLess(aligned.startSec, window.startSec - kWindowSlack)) return std::nullopt;
  if (nearlyLess(window.endSec + kWindowSlack, aligned.endSec)) return std::nullopt;
  return aligned;
}

}  // namespace skipper
