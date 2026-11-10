#pragma once

#include "application/ports.hpp"
#include "infrastructure/process.hpp"

namespace skipper {

class FfmpegFingerprint final : public AudioFingerprint {
 public:
  FfmpegFingerprint(ProcessRunner& runner, ExternalTools& tools);

  std::optional<Fingerprint> fingerprint(const std::string& path,
                                         double startSec,
                                         double lengthSec,
                                         CancelToken& cancel) override;

 private:
  ProcessRunner& runner_;
  ExternalTools& tools_;
};

}  // namespace skipper
