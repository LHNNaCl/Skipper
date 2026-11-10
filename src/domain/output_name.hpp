#pragma once

#include <string>

namespace skipper {

inline std::string fileNameOf(const std::string& path) {
  const auto pos = path.find_last_of("/\\");
  if (pos == std::string::npos) return path;
  return path.substr(pos + 1);
}

inline std::string outputFileName(const std::string& fileName) {
  const auto dot = fileName.rfind('.');
  if (dot == std::string::npos || dot == 0) return fileName + "_trimmed";
  return fileName.substr(0, dot) + "_trimmed" + fileName.substr(dot);
}

}  // namespace skipper
