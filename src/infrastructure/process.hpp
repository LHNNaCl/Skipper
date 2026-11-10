#pragma once

#include "application/cancel.hpp"

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace skipper {

class ProcessRunner {
 public:
  struct Result {
    bool started = false;
    bool cancelled = false;
    int exitCode = -1;
    std::string out;
    std::string err;
  };

  Result run(const std::vector<std::string>& args, CancelToken& cancel);
  void terminate();

 private:
  std::mutex mutex_;
  void* process_ = nullptr;
};

class ExternalTools {
 public:
  ExternalTools();

  const std::optional<std::string>& ffmpeg() const { return ffmpeg_; }
  const std::optional<std::string>& ffprobe() const { return ffprobe_; }
  const std::optional<std::string>& fpcalc() const { return fpcalc_; }
  bool chromaprintMuxer() const { return muxer_; }
  bool chromaprintAvailable(ProcessRunner& runner, CancelToken& cancel);

 private:
  std::optional<std::string> ffmpeg_;
  std::optional<std::string> ffprobe_;
  std::optional<std::string> fpcalc_;
  bool muxerKnown_ = false;
  bool muxer_ = false;
};

std::wstring utf8ToWide(const std::string& text);
std::string wideToUtf8(const std::wstring& text);
std::filesystem::path fsPath(const std::string& text);
std::string pathToUtf8(const std::filesystem::path& path);
std::string formatSeconds(double value);

}  // namespace skipper
