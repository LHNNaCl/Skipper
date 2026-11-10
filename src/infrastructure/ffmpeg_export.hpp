#pragma once

#include "application/ports.hpp"
#include "infrastructure/process.hpp"

namespace skipper {

class FfmpegExport final : public VideoExport {
 public:
  FfmpegExport(ProcessRunner& runner, ExternalTools& tools);

  WriteStatus write(const std::string& source,
                    const std::string& temp,
                    const std::vector<TimeRange>& ranges,
                    CancelToken& cancel) override;

 private:
  WriteStatus from(const ProcessRunner::Result& result, CancelToken& cancel) const;
  WriteStatus cut(const std::string& source, const TimeRange& range, const std::string& dest, CancelToken& cancel);

  ProcessRunner& runner_;
  ExternalTools& tools_;
};

}  // namespace skipper
