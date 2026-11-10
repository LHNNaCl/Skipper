#pragma once

#include "application/ports.hpp"
#include "infrastructure/process.hpp"

namespace skipper {

class FfmpegProbe final : public MediaProbe {
 public:
  FfmpegProbe(ProcessRunner& runner, ExternalTools& tools);

  std::optional<ProbeInfo> probe(const std::string& path, CancelToken& cancel) override;
  std::optional<std::vector<double>> keyframes(const std::string& path,
                                               double duration,
                                               const std::vector<double>& cuts,
                                               CancelToken& cancel) override;
  bool chromaprintAvailable(CancelToken& cancel) override;

 private:
  ProcessRunner& runner_;
  ExternalTools& tools_;
};

}  // namespace skipper
