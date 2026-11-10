#include "infrastructure/ffmpeg_probe.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>

namespace skipper {
namespace {

std::optional<double> readNumber(const nlohmann::json& value) {
  if (value.is_number()) return value.get<double>();
  if (!value.is_string()) return std::nullopt;
  const std::string text = value.get<std::string>();
  if (text.empty() || text == "N/A") return std::nullopt;
  try {
    std::size_t used = 0;
    const double parsed = std::stod(text, &used);
    if (used == 0) return std::nullopt;
    return parsed;
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<nlohmann::json> parseJson(const std::string& text) {
  auto json = nlohmann::json::parse(text, nullptr, false);
  if (json.is_discarded()) return std::nullopt;
  return json;
}

}  // namespace

FfmpegProbe::FfmpegProbe(ProcessRunner& runner, ExternalTools& tools) : runner_(runner), tools_(tools) {}

bool FfmpegProbe::chromaprintAvailable(CancelToken& cancel) {
  return tools_.chromaprintAvailable(runner_, cancel);
}

std::optional<ProbeInfo> FfmpegProbe::probe(const std::string& path, CancelToken& cancel) {
  if (cancel.cancelled() || !tools_.ffprobe()) return std::nullopt;
  const auto result = runner_.run({*tools_.ffprobe(), "-v", "error", "-print_format", "json", "-show_format",
                                   "-show_streams", "-show_chapters", "-i", path},
                                  cancel);
  if (cancel.cancelled() || result.cancelled || !result.started || result.exitCode != 0) return std::nullopt;
  const auto json = parseJson(result.out);
  if (!json) return std::nullopt;

  std::optional<double> duration;
  if (json->contains("format")) duration = readNumber(json->at("format").value("duration", nlohmann::json{}));
  if (!duration && json->contains("streams")) {
    for (const auto& stream : json->at("streams")) {
      if (stream.value("codec_type", std::string()) != "video") continue;
      duration = readNumber(stream.value("duration", nlohmann::json{}));
      if (duration) break;
    }
  }
  if (!duration || !(*duration > 0)) return std::nullopt;

  struct RawChapter {
    std::string title;
    double start = 0;
    bool hasStart = false;
    std::optional<double> end;
  };
  std::vector<RawChapter> raw;
  if (json->contains("chapters") && json->at("chapters").is_array()) {
    for (const auto& chapter : json->at("chapters")) {
      RawChapter item;
      if (const auto start = readNumber(chapter.value("start_time", nlohmann::json{}))) {
        item.start = *start;
        item.hasStart = true;
      }
      item.end = readNumber(chapter.value("end_time", nlohmann::json{}));
      if (chapter.contains("tags") && chapter.at("tags").is_object()) {
        const auto& tags = chapter.at("tags");
        if (tags.contains("title") && tags.at("title").is_string()) item.title = tags.at("title").get<std::string>();
      }
      if (item.hasStart) raw.push_back(std::move(item));
    }
  }

  ProbeInfo info;
  info.durationSec = *duration;
  info.chapters.reserve(raw.size());
  for (std::size_t i = 0; i < raw.size(); ++i) {
    Chapter chapter;
    chapter.title = raw[i].title;
    chapter.startSec = raw[i].start;
    if (raw[i].end) {
      chapter.endSec = *raw[i].end;
    } else if (i + 1 < raw.size()) {
      chapter.endSec = raw[i + 1].start;
    } else {
      chapter.endSec = info.durationSec;
    }
    info.chapters.push_back(std::move(chapter));
  }
  return info;
}

std::optional<std::vector<double>> FfmpegProbe::keyframes(const std::string& path,
                                                          double duration,
                                                          const std::vector<double>& cuts,
                                                          CancelToken& cancel) {
  if (cancel.cancelled() || !tools_.ffprobe()) return std::nullopt;
  if (!(duration > 0)) return std::nullopt;

  struct Interval {
    double start = 0;
    double end = 0;
  };
  std::vector<Interval> intervals;
  for (double cut : cuts) {
    Interval interval{std::max(0.0, cut - 10.0), std::min(duration, cut + 10.0)};
    if (interval.end >= interval.start) intervals.push_back(interval);
  }
  std::sort(intervals.begin(), intervals.end(), [](const Interval& a, const Interval& b) {
    return a.start < b.start;
  });
  std::vector<Interval> merged;
  for (const Interval& interval : intervals) {
    if (!merged.empty() && interval.start <= merged.back().end + 0.05) {
      merged.back().end = std::max(merged.back().end, interval.end);
    } else {
      merged.push_back(interval);
    }
  }

  std::vector<double> frames;
  for (const Interval& interval : merged) {
    if (cancel.cancelled()) return std::nullopt;
    const std::string span = formatSeconds(interval.start) + "%" + formatSeconds(interval.end);
    const auto result = runner_.run({*tools_.ffprobe(), "-v", "error", "-print_format", "json", "-select_streams",
                                     "v:0", "-skip_frame", "nokey", "-read_intervals", span, "-show_entries",
                                     "frame=best_effort_timestamp_time", "-show_frames", "-i", path},
                                    cancel);
    if (cancel.cancelled() || result.cancelled) return std::nullopt;
    if (!result.started || result.exitCode != 0) return std::nullopt;
    const auto json = parseJson(result.out);
    if (!json || !json->contains("frames") || !json->at("frames").is_array()) continue;
    for (const auto& frame : json->at("frames")) {
      const auto time = readNumber(frame.value("best_effort_timestamp_time", nlohmann::json{}));
      if (!time) continue;
      if (*time + 1e-4 >= interval.start && *time - 1e-4 <= interval.end) frames.push_back(*time);
    }
  }
  std::sort(frames.begin(), frames.end());
  return frames;
}

}  // namespace skipper
