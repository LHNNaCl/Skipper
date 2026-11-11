#include "infrastructure/ffmpeg_export.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>

namespace skipper {
namespace {

void removeQuiet(const std::string& path) {
  std::error_code error;
  std::filesystem::remove(fsPath(path), error);
}

bool isMatroska(const std::string& formatName) {
  std::string token;
  auto matches = [](const std::string& name) { return name == "matroska" || name == "webm"; };
  for (char ch : formatName) {
    if (ch == ',') {
      if (matches(token)) return true;
      token.clear();
    } else {
      token += ch;
    }
  }
  return matches(token);
}

// ffmpeg_demux.c seeks 3/23 s earlier on containers without AVFMT_SEEK_TO_PTS
// (Matroska, WebM) when any stream has B-frames. A start that sits on a keyframe
// then lands on the previous one. Video and subtitles keep that GOP, while audio
// is cut at the requested time, so the sound plays ahead of the picture.
double bFrameSeekSlack(const std::string& formatName, int maxBFrames, double startSec) {
  if (!(startSec > 0) || maxBFrames <= 0 || !isMatroska(formatName)) return 0;
  return 3.0 / 23.0 + 0.001;
}

std::string concatLine(std::string path) {
  for (char& ch : path) {
    if (ch == '\\') ch = '/';
  }
  std::string escaped;
  for (char ch : path) {
    if (ch == '\'') escaped += "'\\''";
    else escaped += ch;
  }
  return "file '" + escaped + "'\n";
}

}  // namespace

FfmpegExport::FfmpegExport(ProcessRunner& runner, ExternalTools& tools) : runner_(runner), tools_(tools) {}

WriteStatus FfmpegExport::from(const ProcessRunner::Result& result, CancelToken& cancel) const {
  if (result.cancelled || cancel.cancelled()) return WriteStatus::Cancelled;
  if (!result.started || result.exitCode != 0) return WriteStatus::Failed;
  return WriteStatus::Ok;
}

double FfmpegExport::seekSlack(const std::string& source, double startSec, CancelToken& cancel) const {
  if (!(startSec > 0) || cancel.cancelled() || !tools_.ffprobe()) return 0;
  const auto result = runner_.run({*tools_.ffprobe(), "-v", "error", "-print_format", "json", "-show_entries",
                                   "format=format_name:stream=has_b_frames", "-i", source},
                                  cancel);
  if (cancel.cancelled() || result.cancelled || !result.started || result.exitCode != 0) return 0;
  const auto json = nlohmann::json::parse(result.out, nullptr, false);
  if (json.is_discarded()) return 0;
  int maxBFrames = 0;
  if (json.contains("streams") && json.at("streams").is_array()) {
    for (const auto& stream : json.at("streams")) {
      if (stream.contains("has_b_frames") && stream.at("has_b_frames").is_number_integer()) {
        maxBFrames = std::max(maxBFrames, stream.at("has_b_frames").get<int>());
      }
    }
  }
  const std::string formatName =
      json.contains("format") ? json.at("format").value("format_name", std::string()) : std::string();
  return bFrameSeekSlack(formatName, maxBFrames, startSec);
}

WriteStatus FfmpegExport::cut(const std::string& source,
                              const TimeRange& range,
                              const std::string& dest,
                              CancelToken& cancel) {
  if (cancel.cancelled()) return WriteStatus::Cancelled;
  const double slack = seekSlack(source, range.startSec, cancel);
  if (cancel.cancelled()) return WriteStatus::Cancelled;
  std::vector<std::string> args = {*tools_.ffmpeg(), "-y", "-hide_banner", "-loglevel", "error", "-ss",
                                   formatSeconds(range.startSec + slack)};
  if (slack > 0) {
    args.push_back("-itsoffset");
    args.push_back(formatSeconds(slack));
  }
  args.insert(args.end(), {"-to", formatSeconds(range.endSec), "-i", source, "-map", "0", "-c", "copy",
                           "-avoid_negative_ts", "auto", dest});
  const auto result = runner_.run(args, cancel);
  return from(result, cancel);
}

WriteStatus FfmpegExport::write(const std::string& source,
                                const std::string& temp,
                                const std::vector<TimeRange>& ranges,
                                CancelToken& cancel) {
  if (ranges.empty() || !tools_.ffmpeg()) return WriteStatus::Failed;
  if (cancel.cancelled()) return WriteStatus::Cancelled;
  if (ranges.size() == 1) {
    const WriteStatus status = cut(source, ranges[0], temp, cancel);
    if (status != WriteStatus::Ok) removeQuiet(temp);
    return status;
  }

  const auto tempFs = fsPath(temp);
  const std::wstring tempStem = tempFs.stem().wstring();
  const std::wstring tempExt = tempFs.extension().wstring();
  std::vector<std::string> parts;
  parts.reserve(ranges.size());
  for (std::size_t i = 0; i < ranges.size(); ++i) {
    const std::string part =
        pathToUtf8(tempFs.parent_path() / (tempStem + L"." + std::to_wstring(i) + tempExt));
    parts.push_back(part);
    const WriteStatus status = cut(source, ranges[i], part, cancel);
    if (status != WriteStatus::Ok) {
      for (const std::string& path : parts) removeQuiet(path);
      removeQuiet(temp);
      return status;
    }
  }

  const std::string listPath = temp + ".list";
  {
    std::ofstream list(fsPath(listPath), std::ios::binary);
    if (!list) {
      for (const std::string& path : parts) removeQuiet(path);
      return WriteStatus::Failed;
    }
    for (const std::string& path : parts) list << concatLine(path);
  }
  const auto result = runner_.run({*tools_.ffmpeg(), "-y", "-hide_banner", "-loglevel", "error", "-f", "concat",
                                   "-safe", "0", "-i", listPath, "-map", "0", "-c", "copy", temp},
                                  cancel);
  for (const std::string& path : parts) removeQuiet(path);
  removeQuiet(listPath);
  const WriteStatus status = from(result, cancel);
  if (status != WriteStatus::Ok) removeQuiet(temp);
  return status;
}

}  // namespace skipper
