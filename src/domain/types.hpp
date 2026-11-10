#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace skipper {

enum class SegmentSource {
  Standard,
  Chapter,
  Audio,
  BlackFrame,
  BlackFrameAndAudio,
  Manual
};

enum class ItemStatus { Exported, Excluded, Skipped, Failed };

enum class Reason {
  None,
  NoVideos,
  StandardInvalid,
  AudioUnavailable,
  NoSegments,
  OutputExists,
  CannotRead,
  ExportFailed,
  Cancelled
};

enum class Stage { Probe, Fingerprint, BlackFrame, Match, Export };

enum class Field {
  IntroStart,
  IntroEnd,
  CreditsStart,
  CreditsEnd,
  Intro,
  Credits,
  Pair
};

enum class FieldCode { HalfEmpty, Order, OutOfRange, Overlap };

struct TimeRange {
  double startSec = 0;
  double endSec = 0;
};

struct Chapter {
  std::string title;
  double startSec = 0;
  double endSec = 0;
};

struct Fingerprint {
  double windowStartSec = 0;
  double secondsPerPoint = 0;
  std::vector<std::uint32_t> points;
};

struct VisualSample {
  double timeSec = 0;
  double blackPercent = 0;
  double saturation = 0;
};

struct Segment {
  double startSec = 0;
  double endSec = 0;
  SegmentSource source = SegmentSource::Chapter;
};

struct EpisodePlan {
  std::string path;
  std::string name;
  double durationSec = 0;
  std::optional<Segment> intro;
  std::optional<Segment> credits;
};

struct Endpoint {
  std::optional<double> startSec;
  std::optional<double> endSec;
};

struct StandardMark {
  std::string path;
  Endpoint intro;
  Endpoint credits;
};

struct ConfirmedEpisode {
  std::string path;
  bool excluded = false;
  Endpoint intro;
  Endpoint credits;
};

struct FieldError {
  std::string path;
  Field field = Field::Intro;
  FieldCode code = FieldCode::HalfEmpty;
};

struct Progress {
  int index = 0;
  int count = 0;
  std::string name;
  Stage stage = Stage::Probe;
};

struct ItemResult {
  std::string path;
  std::string name;
  ItemStatus status = ItemStatus::Failed;
  Reason reason = Reason::None;
};

struct EpisodeEvidence {
  std::string path;
  std::string name;
  double durationSec = 0;
  std::vector<Chapter> chapters;
  std::optional<Fingerprint> introWindow;
  std::optional<Fingerprint> creditsWindow;
  std::vector<VisualSample> creditsVisual;
};

struct StandardEvidence {
  std::string path;
  std::optional<TimeRange> intro;
  std::optional<TimeRange> credits;
  std::optional<Fingerprint> introTemplate;
  std::optional<Fingerprint> creditsTemplate;
};

struct ProbeInfo {
  double durationSec = 0;
  std::vector<Chapter> chapters;
};

struct Window {
  double startSec = 0;
  double endSec = 0;
};

}  // namespace skipper
