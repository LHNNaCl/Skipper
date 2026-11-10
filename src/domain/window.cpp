#include "domain/window.hpp"

#include "domain/time.hpp"

#include <algorithm>

namespace skipper {

SearchWindows windowFor(double duration, const DetectionLimits& limits) {
  if (duration < 0) duration = 0;
  double introLen = std::min({duration * limits.introFraction, limits.introCapSec, duration});
  if (introLen < 0) introLen = 0;
  double creditsLen = std::min(limits.creditsWindowSec, duration);
  if (creditsLen < 0) creditsLen = 0;
  SearchWindows windows;
  windows.intro = Window{0, introLen};
  windows.credits = Window{duration - creditsLen, duration};
  return windows;
}

Window widen(const Window& window, double templateDuration, double duration, bool intro) {
  if (duration < 0) duration = 0;
  if (templateDuration < 0) templateDuration = 0;
  const double length = window.endSec - window.startSec;
  if (templateDuration <= length + kTimeEpsilon) return window;
  const double span = std::min(duration, templateDuration);
  if (intro) return Window{0, span};
  return Window{duration - span, duration};
}

bool accepted(const TimeRange& range, const Window& window, const DetectionLimits& limits) {
  const double length = range.endSec - range.startSec;
  if (nearlyLess(length, limits.minSegmentSec)) return false;
  if (nearlyLess(limits.maxSegmentSec, length)) return false;
  if (nearlyLess(range.startSec, window.startSec - kWindowSlack)) return false;
  if (nearlyLess(window.endSec + kWindowSlack, range.endSec)) return false;
  return true;
}

}  // namespace skipper
