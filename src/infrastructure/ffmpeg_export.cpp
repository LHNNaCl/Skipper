#include "infrastructure/ffmpeg_export.hpp"

#include <fstream>

namespace skipper {
namespace {

void removeQuiet(const std::string& path) {
  std::error_code error;
  std::filesystem::remove(fsPath(path), error);
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

WriteStatus FfmpegExport::cut(const std::string& source,
                              const TimeRange& range,
                              const std::string& dest,
                              CancelToken& cancel) {
  if (cancel.cancelled()) return WriteStatus::Cancelled;
  const auto result = runner_.run({*tools_.ffmpeg(), "-y", "-hide_banner", "-loglevel", "error", "-ss",
                                   formatSeconds(range.startSec), "-to", formatSeconds(range.endSec), "-i", source,
                                   "-map", "0", "-c", "copy", "-avoid_negative_ts", "make_zero", dest},
                                  cancel);
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
