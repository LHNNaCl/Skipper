#pragma once

#include "application/cancel.hpp"
#include "domain/types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace skipper {

enum class WriteStatus { Ok, Failed, Cancelled };

class VideoListing {
 public:
  virtual ~VideoListing() = default;
  virtual std::vector<std::string> list(const std::string& directory) = 0;
};

class MediaProbe {
 public:
  virtual ~MediaProbe() = default;
  virtual std::optional<ProbeInfo> probe(const std::string& path, CancelToken& cancel) = 0;
  virtual std::optional<std::vector<double>> keyframes(const std::string& path,
                                                       double duration,
                                                       const std::vector<double>& cuts,
                                                       CancelToken& cancel) = 0;
  virtual bool chromaprintAvailable(CancelToken& cancel) = 0;
};

class AudioFingerprint {
 public:
  virtual ~AudioFingerprint() = default;
  virtual std::optional<Fingerprint> fingerprint(const std::string& path,
                                                 double startSec,
                                                 double lengthSec,
                                                 CancelToken& cancel) = 0;
};

class VisualScan {
 public:
  virtual ~VisualScan() = default;
  virtual std::vector<VisualSample> scan(const std::string& path,
                                         double startSec,
                                         double lengthSec,
                                         CancelToken& cancel) = 0;
};

class VideoExport {
 public:
  virtual ~VideoExport() = default;
  virtual WriteStatus write(const std::string& source,
                            const std::string& temp,
                            const std::vector<TimeRange>& ranges,
                            CancelToken& cancel) = 0;
};

class OutputFiles {
 public:
  virtual ~OutputFiles() = default;
  virtual std::string finalPath(const std::string& directory, const std::string& trimmedName) const = 0;
  virtual std::string tempPath(const std::string& finalPath) const = 0;
  virtual bool exists(const std::string& path) const = 0;
  virtual bool createOutputDir(const std::string& directory) = 0;
  virtual bool renameToFinal(const std::string& temp, const std::string& finalPath) = 0;
  virtual void removeFile(const std::string& path) = 0;
};

}  // namespace skipper
