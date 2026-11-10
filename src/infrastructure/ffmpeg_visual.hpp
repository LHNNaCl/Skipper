#pragma once

#include "application/ports.hpp"
#include "infrastructure/process.hpp"

namespace skipper {

class FfmpegVisual final : public VisualScan {
 public:
  FfmpegVisual(ProcessRunner& runner, ExternalTools& tools);

  std::vector<VisualSample> scan(const std::string& path,
                                 double startSec,
                                 double lengthSec,
                                 CancelToken& cancel) override;

 private:
  ProcessRunner& runner_;
  ExternalTools& tools_;
};

}  // namespace skipper
