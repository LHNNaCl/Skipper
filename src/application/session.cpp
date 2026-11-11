#include "application/session.hpp"

#include "domain/edit_check.hpp"
#include "domain/keep_ranges.hpp"
#include "domain/limits.hpp"
#include "domain/output_name.hpp"
#include "domain/resolver.hpp"
#include "domain/window.hpp"

#include <algorithm>
#include <utility>

namespace skipper {
namespace {

AnalyzeResult cancelledResult() {
  AnalyzeResult result;
  result.reason = Reason::Cancelled;
  return result;
}

bool sideEmpty(const Endpoint& endpoint) {
  return !endpoint.startSec && !endpoint.endSec;
}

std::optional<TimeRange> takenRange(const Endpoint& endpoint) {
  if (!endpoint.startSec || !endpoint.endSec) return std::nullopt;
  return TimeRange{*endpoint.startSec, *endpoint.endSec};
}

bool containsPath(const std::vector<std::string>& paths, const std::string& path) {
  return std::find(paths.begin(), paths.end(), path) != paths.end();
}

}  // namespace

Session::Session(VideoListing& listing,
                 MediaProbe& probe,
                 AudioFingerprint& fingerprint,
                 VisualScan& visual,
                 VideoExport& videoExport,
                 OutputFiles& files)
    : listing_(listing),
      probe_(probe),
      fingerprint_(fingerprint),
      visual_(visual),
      videoExport_(videoExport),
      files_(files) {}

void Session::report(const std::function<void(const Progress&)>& progress, const Progress& info) const {
  if (progress) progress(info);
}

AnalyzeResult Session::analyze(const std::string& directory,
                               const std::optional<StandardMark>& standard,
                               const DetectionMethods& methods,
                               const std::function<void(const Progress&)>& progress,
                               CancelToken& cancel) {
  const DetectionLimits limits;
  auto paths = listing_.list(directory);
  if (paths.empty()) {
    AnalyzeResult result;
    result.reason = Reason::NoVideos;
    return result;
  }

  std::optional<StandardMark> mark = methods.standard ? standard : std::nullopt;
  std::optional<ProbeInfo> standardProbe;
  std::optional<TimeRange> standardIntro;
  std::optional<TimeRange> standardCredits;
  if (mark && sideEmpty(mark->intro) && sideEmpty(mark->credits)) mark.reset();

  if (mark) {
    if (!containsPath(paths, mark->path)) {
      AnalyzeResult result;
      result.reason = Reason::StandardInvalid;
      return result;
    }
    if (cancel.cancelled()) return cancelledResult();
    standardProbe = probe_.probe(mark->path, cancel);
    if (cancel.cancelled()) return cancelledResult();
    if (!standardProbe) {
      AnalyzeResult result;
      result.reason = Reason::StandardInvalid;
      return result;
    }
    ConfirmedEpisode episode;
    episode.path = mark->path;
    episode.intro = mark->intro;
    episode.credits = mark->credits;
    auto errors = validate({episode}, {{mark->path, standardProbe->durationSec}});
    if (!errors.empty()) {
      AnalyzeResult result;
      result.reason = Reason::StandardInvalid;
      result.fieldErrors = std::move(errors);
      return result;
    }
    standardIntro = takenRange(mark->intro);
    standardCredits = takenRange(mark->credits);
  }

  if (cancel.cancelled()) return cancelledResult();
  const bool wantAudio = methods.audio || (methods.standard && (standardIntro.has_value() || standardCredits.has_value()));
  const bool audio = wantAudio && probe_.chromaprintAvailable(cancel);
  if (cancel.cancelled()) return cancelledResult();

  struct Item {
    EpisodeEvidence evidence;
    ProbeInfo probe;
  };
  std::vector<Item> items;
  std::vector<FailedProbe> failures;
  const int total = static_cast<int>(paths.size());
  for (int index = 0; index < total; ++index) {
    const std::string& path = paths[static_cast<std::size_t>(index)];
    report(progress, Progress{index + 1, total, fileNameOf(path), Stage::Probe});
    if (cancel.cancelled()) return cancelledResult();
    std::optional<ProbeInfo> info;
    if (mark && path == mark->path && standardProbe) {
      info = standardProbe;
    } else {
      info = probe_.probe(path, cancel);
    }
    if (cancel.cancelled()) return cancelledResult();
    if (!info) {
      failures.push_back(FailedProbe{path, fileNameOf(path), Reason::CannotRead});
      continue;
    }
    Item item;
    item.probe = *info;
    item.evidence.path = path;
    item.evidence.name = fileNameOf(path);
    item.evidence.durationSec = info->durationSec;
    item.evidence.chapters = info->chapters;
    items.push_back(std::move(item));
  }

  const double introLen = standardIntro ? standardIntro->endSec - standardIntro->startSec : 0;
  const double creditsLen = standardCredits ? standardCredits->endSec - standardCredits->startSec : 0;
  std::vector<SearchWindows> windows;
  windows.reserve(items.size());
  for (const Item& item : items) {
    auto window = windowFor(item.evidence.durationSec, limits);
    if (standardIntro) window.intro = widen(window.intro, introLen, item.evidence.durationSec, true);
    if (standardCredits) window.credits = widen(window.credits, creditsLen, item.evidence.durationSec, false);
    windows.push_back(window);
  }

  std::optional<Fingerprint> introTemplate;
  std::optional<Fingerprint> creditsTemplate;
  const bool introTemplateWanted = audio && methods.standard && standardIntro.has_value();
  const bool creditsTemplateWanted = audio && methods.standard && standardCredits.has_value();
  const bool introPairs = audio && methods.audio && !standardIntro && items.size() >= 2;
  const bool creditsPairs = audio && methods.audio && !standardCredits && items.size() >= 2;

  if (audio && (introTemplateWanted || creditsTemplateWanted || introPairs || creditsPairs)) {
    if (introTemplateWanted && mark) {
      if (cancel.cancelled()) return cancelledResult();
      introTemplate = fingerprint_.fingerprint(mark->path, standardIntro->startSec, introLen, cancel);
      if (cancel.cancelled()) return cancelledResult();
    }
    if (creditsTemplateWanted && mark) {
      if (cancel.cancelled()) return cancelledResult();
      creditsTemplate = fingerprint_.fingerprint(mark->path, standardCredits->startSec, creditsLen, cancel);
      if (cancel.cancelled()) return cancelledResult();
    }

    const int count = static_cast<int>(items.size());
    for (int index = 0; index < count; ++index) {
      auto& item = items[static_cast<std::size_t>(index)];
      report(progress, Progress{index + 1, count, item.evidence.name, Stage::Fingerprint});
      if (cancel.cancelled()) return cancelledResult();
      const bool isStandard = mark && item.evidence.path == mark->path;
      const auto& window = windows[static_cast<std::size_t>(index)];
      if ((introTemplateWanted && !isStandard) || introPairs) {
        const double length = window.intro.endSec - window.intro.startSec;
        if (length > 0) {
          item.evidence.introWindow =
              fingerprint_.fingerprint(item.evidence.path, window.intro.startSec, length, cancel);
        }
        if (cancel.cancelled()) return cancelledResult();
      }
      if ((creditsTemplateWanted && !isStandard) || creditsPairs) {
        const double length = window.credits.endSec - window.credits.startSec;
        if (length > 0) {
          item.evidence.creditsWindow =
              fingerprint_.fingerprint(item.evidence.path, window.credits.startSec, length, cancel);
        }
        if (cancel.cancelled()) return cancelledResult();
      }
    }
  }

  std::vector<int> visualIndex;
  if (methods.blackFrame) {
    for (int index = 0; index < static_cast<int>(items.size()); ++index) {
      const bool isStandard = mark && items[static_cast<std::size_t>(index)].evidence.path == mark->path;
      if (standardCredits && isStandard) continue;
      visualIndex.push_back(index);
    }
  }
  const int visualCount = static_cast<int>(visualIndex.size());
  for (int step = 0; step < visualCount; ++step) {
    const int index = visualIndex[static_cast<std::size_t>(step)];
    auto& item = items[static_cast<std::size_t>(index)];
    report(progress, Progress{step + 1, visualCount, item.evidence.name, Stage::BlackFrame});
    if (cancel.cancelled()) return cancelledResult();
    const auto& window = windows[static_cast<std::size_t>(index)].credits;
    const double length = window.endSec - window.startSec;
    if (length > 0) {
      item.evidence.creditsVisual = visual_.scan(item.evidence.path, window.startSec, length, cancel);
    }
    if (cancel.cancelled()) return cancelledResult();
  }

  if (cancel.cancelled()) return cancelledResult();
  const int matchCount = static_cast<int>(items.size());
  for (int index = 0; index < matchCount; ++index) {
    report(progress, Progress{index + 1, std::max(matchCount, 1), items[static_cast<std::size_t>(index)].evidence.name,
                              Stage::Match});
  }

  std::vector<EpisodeEvidence> evidence;
  evidence.reserve(items.size());
  for (const Item& item : items) evidence.push_back(item.evidence);

  std::optional<StandardEvidence> standardEvidence;
  if (standardIntro || standardCredits) {
    StandardEvidence packed;
    packed.path = mark->path;
    packed.intro = standardIntro;
    packed.credits = standardCredits;
    packed.introTemplate = std::move(introTemplate);
    packed.creditsTemplate = std::move(creditsTemplate);
    standardEvidence = std::move(packed);
  }

  AnalyzeResult result;
  result.reason = Reason::None;
  result.plans = resolve(evidence, standardEvidence, limits, methods);
  result.failures = std::move(failures);
  result.audioUnavailable = wantAudio && !audio;

  directory_ = directory;
  durations_.clear();
  for (const EpisodePlan& plan : result.plans) durations_[plan.path] = plan.durationSec;
  ready_ = true;
  return result;
}

ExportResult Session::exportConfirmed(const std::vector<ConfirmedEpisode>& confirmed,
                                      const std::function<void(const Progress&)>& progress,
                                      CancelToken& cancel) {
  ExportResult result;
  if (!ready_) {
    result.batchReason = Reason::ExportFailed;
    return result;
  }
  auto errors = validate(confirmed, durations_);
  if (!errors.empty()) {
    result.validationFailed = true;
    result.fieldErrors = std::move(errors);
    return result;
  }
  if (!files_.createOutputDir(directory_)) {
    result.batchReason = Reason::ExportFailed;
    return result;
  }

  const int count = static_cast<int>(confirmed.size());
  for (int index = 0; index < count; ++index) {
    const ConfirmedEpisode& item = confirmed[static_cast<std::size_t>(index)];
    const std::string name = fileNameOf(item.path);
    report(progress, Progress{index + 1, count, name, Stage::Export});
    if (cancel.cancelled()) {
      result.items.push_back(ItemResult{item.path, name, ItemStatus::Failed, Reason::Cancelled});
      result.cancelled = true;
      break;
    }
    if (item.excluded) {
      result.items.push_back(ItemResult{item.path, name, ItemStatus::Excluded, Reason::None});
      continue;
    }
    if (sideEmpty(item.intro) && sideEmpty(item.credits)) {
      result.items.push_back(ItemResult{item.path, name, ItemStatus::Skipped, Reason::NoSegments});
      continue;
    }
    const std::string finalPath = files_.finalPath(directory_, outputFileName(name));
    const std::string temp = files_.tempPath(finalPath);
    if (files_.exists(finalPath)) {
      result.items.push_back(ItemResult{item.path, name, ItemStatus::Skipped, Reason::OutputExists});
      continue;
    }
    const auto duration = durations_.find(item.path);
    if (duration == durations_.end()) {
      result.items.push_back(ItemResult{item.path, name, ItemStatus::Failed, Reason::CannotRead});
      continue;
    }

    const auto intro = takenRange(item.intro);
    const auto credits = takenRange(item.credits);
    std::vector<double> cuts{0, duration->second};
    if (intro) {
      cuts.push_back(intro->startSec);
      cuts.push_back(intro->endSec);
    }
    if (credits) {
      cuts.push_back(credits->startSec);
      cuts.push_back(credits->endSec);
    }
    auto frames = probe_.keyframes(item.path, duration->second, cuts, cancel);
    if (cancel.cancelled()) {
      files_.removeFile(temp);
      result.items.push_back(ItemResult{item.path, name, ItemStatus::Failed, Reason::Cancelled});
      result.cancelled = true;
      break;
    }
    if (!frames) {
      result.items.push_back(ItemResult{item.path, name, ItemStatus::Failed, Reason::ExportFailed});
      continue;
    }
    auto ranges = planKeepRanges(duration->second, intro, credits, *frames);
    if (ranges.empty()) {
      result.items.push_back(ItemResult{item.path, name, ItemStatus::Failed, Reason::ExportFailed});
      continue;
    }
    const WriteStatus status = videoExport_.write(item.path, temp, ranges, cancel);
    if (status == WriteStatus::Cancelled || cancel.cancelled()) {
      files_.removeFile(temp);
      result.items.push_back(ItemResult{item.path, name, ItemStatus::Failed, Reason::Cancelled});
      result.cancelled = true;
      break;
    }
    if (status != WriteStatus::Ok) {
      files_.removeFile(temp);
      result.items.push_back(ItemResult{item.path, name, ItemStatus::Failed, Reason::ExportFailed});
      continue;
    }
    if (!files_.renameToFinal(temp, finalPath)) {
      files_.removeFile(temp);
      result.items.push_back(ItemResult{item.path, name, ItemStatus::Failed, Reason::ExportFailed});
      continue;
    }
    result.items.push_back(ItemResult{item.path, name, ItemStatus::Exported, Reason::None});
  }
  return result;
}

}  // namespace skipper
