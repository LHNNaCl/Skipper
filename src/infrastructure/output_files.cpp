#include "infrastructure/output_files.hpp"

#include "infrastructure/process.hpp"

namespace skipper {

std::string OutputFilesFs::finalPath(const std::string& directory, const std::string& trimmedName) const {
  const auto path = fsPath(directory) / L"output" / fsPath(trimmedName);
  return pathToUtf8(path);
}

std::string OutputFilesFs::tempPath(const std::string& finalPath) const {
  // Keep a real media extension so ffmpeg can pick a muxer (".mkv.part" cannot).
  const auto path = fsPath(finalPath);
  return pathToUtf8(path.parent_path() / (path.stem().wstring() + L".part" + path.extension().wstring()));
}

bool OutputFilesFs::exists(const std::string& path) const {
  std::error_code error;
  return std::filesystem::is_regular_file(fsPath(path), error);
}

bool OutputFilesFs::createOutputDir(const std::string& directory) {
  std::error_code error;
  std::filesystem::create_directories(fsPath(directory) / L"output", error);
  return !error;
}

bool OutputFilesFs::renameToFinal(const std::string& temp, const std::string& finalPath) {
  std::error_code error;
  if (std::filesystem::exists(fsPath(finalPath), error)) return false;
  std::filesystem::rename(fsPath(temp), fsPath(finalPath), error);
  return !error;
}

void OutputFilesFs::removeFile(const std::string& path) {
  std::error_code error;
  std::filesystem::remove(fsPath(path), error);
}

}  // namespace skipper
