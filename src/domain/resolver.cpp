#include "domain/resolver.hpp"

#include "domain/audio_match.hpp"
#include "domain/black_frame.hpp"
#include "domain/chapter_picker.hpp"
#include "domain/time.hpp"
#include "domain/window.hpp"

#include <algorithm>
#include <cmath>

namespace skipper {
namespace {

double lengthOf(const TimeRange& range) { return range.endSec - range.startSec; }

double gapBetween(const TimeRange& a, const TimeRange& b) {
  if (a.endSec < b.startSec) return b.startSec - a.endSec;
  if (b.endSec < a.startSec) return a.startSec - b.endSec;
  return 0;
}

Segment fromRange(const TimeRange& range, SegmentSource source) {
  return Segment{range.startSec, range.endSec, source};
}

Segment chooseLonger(const TimeRange& black,
                     const TimeRange& audio) {
  const double blackLen = lengthOf(black);
  const double audioLen = lengthOf(audio);
  if (nearlyLess(blackLen, audioLen)) return fromRange(audio, SegmentSource::Audio);
  if (nearlyLess(audioLen, blackLen)) return fromRange(black, SegmentSource::BlackFrame);
  if (nearlyLess(black.endSec, audio.endSec)) return fromRange(audio, SegmentSource::Audio);
  return fromRange(black, SegmentSource::BlackFrame);
}

Segment chooseLaterEnd(const TimeRange& black, const TimeRange& audio) {
  if (nearlyLess(black.endSec, audio.endSec)) return fromRange(audio, SegmentSource::Audio);
  if (nearlyLess(audio.endSec, black.endSec)) return fromRange(black, SegmentSource::BlackFrame);
  return chooseLonger(black, audio);
}

std::optional<Segment> combineCredits(const std::optional<TimeRange>& black,
                                      const std::optional<TimeRange>& audio,
                                      const Window& window,
                                      const DetectionLimits& limits) {
  if (!black && !audio) return std::nullopt;
  if (black && !audio) return fromRange(*black, SegmentSource::BlackFrame);
  if (!black && audio) return fromRange(*audio, SegmentSource::Audio);

  const double gap = gapBetween(*black, *audio);
  if (!nearlyLess(limits.mergeGapSec, gap)) {
    TimeRange united{std::min(black->startSec, audio->startSec),
                     std::max(black->endSec, audio->endSec)};
    if (accepted(united, window, limits)) return fromRange(united, SegmentSource::BlackFrameAndAudio);
    return chooseLonger(*black, *audio);
  }
  return chooseLaterEnd(*black, *audio);
}

}  // namespace

std::vector<EpisodePlan> resolve(const std::vector<EpisodeEvidence>& episodes,
                                 const std::optional<StandardEvidence>& standard,
                                 const DetectionLimits& limits) {
  const int count = static_cast<int>(episodes.size());
  std::vector<Window> introWindows(static_cast<std::size_t>(count));
  std::vector<Window> creditsWindows(static_cast<std::size_t>(count));
  const bool hasIntroStandard = standard && standard->intro.has_value();
  const bool hasCreditsStandard = standard && standard->credits.has_value();
  const double introTemplateLen = hasIntroStandard ? lengthOf(*standard->intro) : 0;
  const double creditsTemplateLen = hasCreditsStandard ? lengthOf(*standard->credits) : 0;

  for (int i = 0; i < count; ++i) {
    auto windows = windowFor(episodes[static_cast<std::size_t>(i)].durationSec, limits);
    if (hasIntroStandard) {
      windows.intro = widen(windows.intro, introTemplateLen,
                            episodes[static_cast<std::size_t>(i)].durationSec, true);
    }
    if (hasCreditsStandard) {
      windows.credits = widen(windows.credits, creditsTemplateLen,
                              episodes[static_cast<std::size_t>(i)].durationSec, false);
    }
    introWindows[static_cast<std::size_t>(i)] = windows.intro;
    creditsWindows[static_cast<std::size_t>(i)] = windows.credits;
  }

  std::vector<std::optional<TimeRange>> introChapters(static_cast<std::size_t>(count));
  std::vector<std::optional<TimeRange>> creditsChapters(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    introChapters[static_cast<std::size_t>(i)] =
        pickChapter(episodes[static_cast<std::size_t>(i)].chapters,
                    introWindows[static_cast<std::size_t>(i)], true, limits);
    creditsChapters[static_cast<std::size_t>(i)] =
        pickChapter(episodes[static_cast<std::size_t>(i)].chapters,
                    creditsWindows[static_cast<std::size_t>(i)], false, limits);
  }

  std::vector<std::optional<TimeRange>> introAudio(static_cast<std::size_t>(count));
  std::vector<std::optional<TimeRange>> creditsAudio(static_cast<std::size_t>(count));
  if (!hasIntroStandard) {
    std::vector<std::optional<Fingerprint>> prints;
    prints.reserve(static_cast<std::size_t>(count));
    for (const auto& episode : episodes) prints.push_back(episode.introWindow);
    introAudio = bestPairwise(prints, introWindows, limits);
  }
  if (!hasCreditsStandard) {
    std::vector<std::optional<Fingerprint>> prints;
    prints.reserve(static_cast<std::size_t>(count));
    for (const auto& episode : episodes) prints.push_back(episode.creditsWindow);
    creditsAudio = bestPairwise(prints, creditsWindows, limits);
  }

  std::vector<std::optional<TimeRange>> introTemplate(static_cast<std::size_t>(count));
  std::vector<std::optional<TimeRange>> creditsTemplate(static_cast<std::size_t>(count));
  if (hasIntroStandard && standard->introTemplate) {
    for (int i = 0; i < count; ++i) {
      if (episodes[static_cast<std::size_t>(i)].path == standard->path) continue;
      if (!episodes[static_cast<std::size_t>(i)].introWindow) continue;
      introTemplate[static_cast<std::size_t>(i)] =
          matchTemplate(*standard->introTemplate, *episodes[static_cast<std::size_t>(i)].introWindow,
                        introWindows[static_cast<std::size_t>(i)], limits);
    }
  }
  if (hasCreditsStandard && standard->creditsTemplate) {
    for (int i = 0; i < count; ++i) {
      if (episodes[static_cast<std::size_t>(i)].path == standard->path) continue;
      if (!episodes[static_cast<std::size_t>(i)].creditsWindow) continue;
      creditsTemplate[static_cast<std::size_t>(i)] =
          matchTemplate(*standard->creditsTemplate,
                        *episodes[static_cast<std::size_t>(i)].creditsWindow,
                        creditsWindows[static_cast<std::size_t>(i)], limits);
    }
  }

  std::vector<std::optional<TimeRange>> black(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    black[static_cast<std::size_t>(i)] =
        pickBlackFrame(episodes[static_cast<std::size_t>(i)].creditsVisual,
                       creditsWindows[static_cast<std::size_t>(i)],
                       episodes[static_cast<std::size_t>(i)].durationSec, limits);
  }

  std::vector<EpisodePlan> plans;
  plans.reserve(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    const auto& episode = episodes[static_cast<std::size_t>(i)];
    const bool isStandard = standard && episode.path == standard->path;
    EpisodePlan plan;
    plan.path = episode.path;
    plan.name = episode.name;
    plan.durationSec = episode.durationSec;

    if (isStandard && hasIntroStandard) {
      plan.intro = fromRange(*standard->intro, SegmentSource::Standard);
    } else if (introTemplate[static_cast<std::size_t>(i)]) {
      plan.intro = fromRange(*introTemplate[static_cast<std::size_t>(i)], SegmentSource::Standard);
    } else if (introChapters[static_cast<std::size_t>(i)]) {
      plan.intro = fromRange(*introChapters[static_cast<std::size_t>(i)], SegmentSource::Chapter);
    } else if (introAudio[static_cast<std::size_t>(i)]) {
      plan.intro = fromRange(*introAudio[static_cast<std::size_t>(i)], SegmentSource::Audio);
    }

    if (isStandard && hasCreditsStandard) {
      plan.credits = fromRange(*standard->credits, SegmentSource::Standard);
    } else if (creditsTemplate[static_cast<std::size_t>(i)]) {
      plan.credits = fromRange(*creditsTemplate[static_cast<std::size_t>(i)], SegmentSource::Standard);
    } else if (creditsChapters[static_cast<std::size_t>(i)]) {
      plan.credits = fromRange(*creditsChapters[static_cast<std::size_t>(i)], SegmentSource::Chapter);
    } else {
      plan.credits = combineCredits(black[static_cast<std::size_t>(i)],
                                    creditsAudio[static_cast<std::size_t>(i)],
                                    creditsWindows[static_cast<std::size_t>(i)], limits);
    }
    plans.push_back(std::move(plan));
  }
  return plans;
}

}  // namespace skipper
