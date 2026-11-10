#pragma once

#include "application/ports.hpp"

namespace skipper {

class OutputFilesFs final : public OutputFiles {
 public:
  std::string finalPath(const std::string& directory, const std::string& trimmedName) const override;
  std::string tempPath(const std::string& finalPath) const override;
  bool exists(const std::string& path) const override;
  bool createOutputDir(const std::string& directory) override;
  bool renameToFinal(const std::string& temp, const std::string& finalPath) override;
  void removeFile(const std::string& path) override;
};

}  // namespace skipper
