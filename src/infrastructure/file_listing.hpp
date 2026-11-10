#pragma once

#include "application/ports.hpp"

namespace skipper {

class FileListingFs final : public VideoListing {
 public:
  std::vector<std::string> list(const std::string& directory) override;
};

}  // namespace skipper
