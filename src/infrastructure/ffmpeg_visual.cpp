#include "infrastructure/ffmpeg_visual.hpp"

#include <optional>
#include <sstream>

namespace skipper {
namespace {

std::optional<double> numberAfter(const std::string& line, const std::string& key) {
  const auto pos = line.find(key);
  if (pos == std::string::npos) return std::nullopt;
  try {
    std::size_t used = 0;
    const double value = std::stod(line.substr(pos + key.size()), &used);
    if (used == 0) return std::nullopt;
    return value;
  } catch (...) {
    return std::nullopt;
  }
}

std::vector<VisualSample> parseVisualLog(const std::string& text, double start) {
  std::vector<VisualSample> samples;
  std::optional<double> time;
  std::optional<double> black;
  std::optional<double> saturation;
  auto commit = [&] {
    if (time && black && saturation) samples.push_back(VisualSample{*time + start, *black, *saturation});
    time.reset();
    black.reset();
    saturation.reset();
  };

  std::istringstream input(text);
  std::string line;
  while (std::getline(input, line)) {
    if (line.find("pts_time:") != std::string::npos) {
      commit();
      time = numberAfter(line, "pts_time:");
    }
    if (const auto value = numberAfter(line, "lavfi.blackframe.pblack=")) black = value;
    if (const auto value = numberAfter(line, "lavfi.signalstats.SATAVG=")) saturation = value;
  }
  commit();
  return samples;
}

}  // namespace

FfmpegVisual::FfmpegVisual(ProcessRunner& runner, ExternalTools& tools) : runner_(runner), tools_(tools) {}

std::vector<VisualSample> FfmpegVisual::scan(const std::string& path,
                                             double startSec,
                                             double lengthSec,
                                             CancelToken& cancel) {
  if (cancel.cancelled() || !(lengthSec > 0) || !tools_.ffmpeg()) return {};
  const auto result = runner_.run({*tools_.ffmpeg(), "-hide_banner", "-loglevel", "info", "-ss",
                                   formatSeconds(startSec), "-t", formatSeconds(lengthSec), "-i", path, "-an", "-vf",
                                   "fps=2,blackframe=amount=0:threshold=32,signalstats,metadata=print", "-f", "null",
                                   "-"},
                                  cancel);
  if (cancel.cancelled() || result.cancelled || !result.started || result.exitCode != 0) return {};
  auto samples = parseVisualLog(result.err, startSec);
  if (samples.empty()) samples = parseVisualLog(result.out, startSec);
  return samples;
}

}  // namespace skipper
