#include "infrastructure/file_listing.hpp"

#include "domain/output_name.hpp"
#include "infrastructure/process.hpp"

#include <algorithm>
#include <cwctype>
#include <unordered_set>

namespace skipper {
namespace {

std::wstring lowerExt(std::wstring text) {
  for (wchar_t& ch : text) {
    if (ch >= L'A' && ch <= L'Z') ch = static_cast<wchar_t>(ch - L'A' + L'a');
  }
  return text;
}

bool nameLess(const std::string& left, const std::string& right) {
  auto key = [](std::string name) {
    for (char& ch : name) {
      if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    return name;
  };
  const std::string a = key(fileNameOf(left));
  const std::string b = key(fileNameOf(right));
  return std::lexicographical_compare(reinterpret_cast<const unsigned char*>(a.data()),
                                      reinterpret_cast<const unsigned char*>(a.data() + a.size()),
                                      reinterpret_cast<const unsigned char*>(b.data()),
                                      reinterpret_cast<const unsigned char*>(b.data() + b.size()));
}

}  // namespace

std::vector<std::string> FileListingFs::list(const std::string& directory) {
  namespace fs = std::filesystem;
  const std::unordered_set<std::wstring> extensions = {
      L".mp4", L".mkv", L".avi", L".mov", L".webm", L".ts", L".m2ts", L".wmv", L".flv"};
  std::error_code error;
  const fs::path root = fsPath(directory);
  if (!fs::is_directory(root, error)) return {};

  std::vector<std::string> paths;
  fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, error);
  const fs::directory_iterator end;
  for (; !error && it != end; it.increment(error)) {
    std::error_code statusError;
    if (!it->is_regular_file(statusError)) continue;
    if (extensions.count(lowerExt(it->path().extension().wstring())) == 0) continue;
    std::error_code absoluteError;
    const fs::path absolute = fs::absolute(it->path(), absoluteError);
    paths.push_back(pathToUtf8(absoluteError ? it->path() : absolute));
  }
  std::sort(paths.begin(), paths.end(), nameLess);
  return paths;
}

}  // namespace skipper
