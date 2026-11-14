#pragma once

#include "application/ports.hpp"
#include "domain/types.hpp"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace skipper {

struct FailedProbe {
  std::string path;
  std::string name;
  Reason reason = Reason::CannotRead;
};

struct AnalyzeResult {
  Reason reason = Reason::None;
  std::vector<FieldError> fieldErrors;
  std::vector<EpisodePlan> plans;
  std::vector<FailedProbe> failures;
  bool audioUnavailable = false;
};

struct ExportResult {
  bool validationFailed = false;
  std::vector<FieldError> fieldErrors;
  std::vector<ItemResult> items;
  bool cancelled = false;
  Reason batchReason = Reason::None;
};

class Session {
 public:
  Session(VideoListing& listing,
          MediaProbe& probe,
          AudioFingerprint& fingerprint,
          VisualScan& visual,
          VideoExport& videoExport,
          OutputFiles& files);

  AnalyzeResult analyze(const std::string& directory,
                        const std::optional<StandardMark>& standard,
                        const DetectionMethods& methods,
                        const std::function<void(const Progress&)>& progress,
                        CancelToken& cancel);

  ExportResult exportConfirmed(const std::vector<ConfirmedEpisode>& confirmed,
                               const std::function<void(const Progress&)>& progress,
                               CancelToken& cancel);

  bool ready() const { return ready_; }
  const std::string& directory() const { return directory_; }

 private:
  void report(const std::function<void(const Progress&)>& progress, const Progress& info) const;

  VideoListing& listing_;
  MediaProbe& probe_;
  AudioFingerprint& fingerprint_;
  VisualScan& visual_;
  VideoExport& videoExport_;
  OutputFiles& files_;

  bool ready_ = false;
  std::string directory_;
  std::unordered_map<std::string, double> durations_;
};

}  // namespace skipper
