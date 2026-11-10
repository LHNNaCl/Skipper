#include "infrastructure/ffmpeg_fingerprint.hpp"

#include <windows.h>

#include <cmath>
#include <fstream>
#include <sstream>

namespace skipper {
namespace {

std::vector<std::uint32_t> parseRaw(const std::string& bytes) {
  std::vector<std::uint32_t> points;
  const std::size_t count = bytes.size() / 4;
  points.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    const auto* data = reinterpret_cast<const unsigned char*>(bytes.data() + i * 4);
    const std::uint32_t value = static_cast<std::uint32_t>(data[0]) |
                                (static_cast<std::uint32_t>(data[1]) << 8) |
                                (static_cast<std::uint32_t>(data[2]) << 16) |
                                (static_cast<std::uint32_t>(data[3]) << 24);
    points.push_back(value);
  }
  return points;
}

std::optional<std::vector<std::uint32_t>> parseFpcalc(const std::string& text) {
  std::string body = text;
  const auto marker = body.find("FINGERPRINT=");
  if (marker != std::string::npos) body = body.substr(marker + 12);
  const auto lineEnd = body.find_first_of("\r\n");
  if (lineEnd != std::string::npos) body = body.substr(0, lineEnd);
  if (body.empty()) return std::nullopt;

  std::vector<std::uint32_t> points;
  std::stringstream stream(body);
  std::string item;
  while (std::getline(stream, item, ',')) {
    if (item.empty()) continue;
    try {
      points.push_back(static_cast<std::uint32_t>(std::stoul(item)));
    } catch (...) {
      return std::nullopt;
    }
  }
  if (points.empty()) return std::nullopt;
  return points;
}

std::string tempWavPath() {
  const auto path = std::filesystem::temp_directory_path() /
                    (L"skipper-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                     std::to_wstring(GetTickCount64()) + L".wav");
  return pathToUtf8(path);
}

Fingerprint makePrint(double start, double length, std::vector<std::uint32_t> points) {
  Fingerprint fingerprint;
  fingerprint.windowStartSec = start;
  fingerprint.secondsPerPoint = length / static_cast<double>(points.size());
  fingerprint.points = std::move(points);
  return fingerprint;
}

}  // namespace

FfmpegFingerprint::FfmpegFingerprint(ProcessRunner& runner, ExternalTools& tools)
    : runner_(runner), tools_(tools) {}

std::optional<Fingerprint> FfmpegFingerprint::fingerprint(const std::string& path,
                                                          double startSec,
                                                          double lengthSec,
                                                          CancelToken& cancel) {
  if (cancel.cancelled() || !(lengthSec > 0)) return std::nullopt;
  tools_.chromaprintAvailable(runner_, cancel);
  if (cancel.cancelled()) return std::nullopt;

  const std::string start = formatSeconds(startSec);
  const std::string length = formatSeconds(lengthSec);
  if (tools_.chromaprintMuxer() && tools_.ffmpeg()) {
    const auto result = runner_.run({*tools_.ffmpeg(), "-hide_banner", "-loglevel", "error", "-ss", start, "-t",
                                     length, "-i", path, "-vn", "-ac", "1", "-ar", "11025", "-f", "chromaprint",
                                     "-fp_format", "raw", "-"},
                                    cancel);
    if (cancel.cancelled() || result.cancelled || !result.started || result.exitCode != 0) return std::nullopt;
    auto points = parseRaw(result.out);
    if (points.empty()) return std::nullopt;
    return makePrint(startSec, lengthSec, std::move(points));
  }

  if (!tools_.fpcalc() || !tools_.ffmpeg()) return std::nullopt;
  const std::string wav = tempWavPath();
  const auto extracted = runner_.run({*tools_.ffmpeg(), "-y", "-hide_banner", "-loglevel", "error", "-ss", start, "-t",
                                      length, "-i", path, "-vn", "-ac", "1", "-ar", "11025", wav},
                                     cancel);
  if (cancel.cancelled() || extracted.cancelled || !extracted.started || extracted.exitCode != 0) {
    std::error_code error;
    std::filesystem::remove(fsPath(wav), error);
    return std::nullopt;
  }
  const int seconds = std::max(1, static_cast<int>(std::ceil(lengthSec)));
  const auto printed = runner_.run({*tools_.fpcalc(), "-raw", "-plain", "-length", std::to_string(seconds), wav},
                                   cancel);
  std::error_code error;
  std::filesystem::remove(fsPath(wav), error);
  if (cancel.cancelled() || printed.cancelled || !printed.started || printed.exitCode != 0) return std::nullopt;
  auto points = parseFpcalc(printed.out.empty() ? printed.err : printed.out);
  if (!points) return std::nullopt;
  return makePrint(startSec, lengthSec, std::move(*points));
}

}  // namespace skipper
