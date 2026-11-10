#include "infrastructure/process.hpp"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <thread>

namespace skipper {
namespace {

std::wstring quoteArg(const std::wstring& arg) {
  if (arg.empty()) return L"\"\"";
  if (arg.find_first_of(L" \t\"") == std::wstring::npos) return arg;
  std::wstring out = L"\"";
  int slashes = 0;
  for (wchar_t ch : arg) {
    if (ch == L'\\') {
      ++slashes;
      continue;
    }
    if (ch == L'"') {
      out.append(static_cast<std::size_t>(slashes * 2 + 1), L'\\');
      out.push_back(L'"');
      slashes = 0;
      continue;
    }
    if (slashes > 0) out.append(static_cast<std::size_t>(slashes), L'\\');
    out.push_back(ch);
    slashes = 0;
  }
  if (slashes > 0) out.append(static_cast<std::size_t>(slashes * 2), L'\\');
  out.push_back(L'"');
  return out;
}

void readPipe(HANDLE handle, std::string& dest) {
  char buffer[8192];
  DWORD count = 0;
  while (ReadFile(handle, buffer, sizeof(buffer), &count, nullptr) && count > 0) {
    dest.append(buffer, buffer + count);
  }
}

std::filesystem::path executableDir() {
  wchar_t buffer[MAX_PATH];
  const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) return {};
  return std::filesystem::path(buffer).parent_path();
}

std::optional<std::string> findTool(const wchar_t* name) {
  const auto dir = executableDir();
  if (!dir.empty()) {
    const auto local = dir / name;
    std::error_code ec;
    if (std::filesystem::is_regular_file(local, ec)) return pathToUtf8(local);
  }
  wchar_t buffer[MAX_PATH];
  const DWORD length = SearchPathW(nullptr, name, nullptr, MAX_PATH, buffer, nullptr);
  if (length == 0 || length >= MAX_PATH) return std::nullopt;
  return wideToUtf8(buffer);
}

}  // namespace

std::wstring utf8ToWide(const std::string& text) {
  if (text.empty()) return {};
  const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  if (size <= 0) return {};
  std::wstring wide(static_cast<std::size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), size);
  return wide;
}

std::string wideToUtf8(const std::wstring& text) {
  if (text.empty()) return {};
  const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
  if (size <= 0) return {};
  std::string utf8(static_cast<std::size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), utf8.data(), size, nullptr, nullptr);
  return utf8;
}

std::filesystem::path fsPath(const std::string& text) { return std::filesystem::path(utf8ToWide(text)); }

std::string pathToUtf8(const std::filesystem::path& path) { return wideToUtf8(path.wstring()); }

std::string formatSeconds(double value) {
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%.6f", value);
  return buffer;
}

ExternalTools::ExternalTools() {
  ffmpeg_ = findTool(L"ffmpeg.exe");
  ffprobe_ = findTool(L"ffprobe.exe");
  fpcalc_ = findTool(L"fpcalc.exe");
}

bool ExternalTools::chromaprintAvailable(ProcessRunner& runner, CancelToken& cancel) {
  if (!muxerKnown_) {
    bool muxer = false;
    bool decided = true;
    if (ffmpeg_ && !cancel.cancelled()) {
      const auto result = runner.run({*ffmpeg_, "-hide_banner", "-muxers"}, cancel);
      if (result.cancelled || cancel.cancelled()) {
        decided = false;
      } else if (result.started && result.exitCode == 0) {
        muxer = result.out.find("chromaprint") != std::string::npos ||
                result.err.find("chromaprint") != std::string::npos;
      }
    }
    if (decided) {
      muxer_ = muxer;
      muxerKnown_ = true;
    }
  }
  return muxer_ || fpcalc_.has_value();
}

ProcessRunner::Result ProcessRunner::run(const std::vector<std::string>& args, CancelToken& cancel) {
  Result result;
  if (args.empty() || cancel.cancelled()) {
    result.cancelled = cancel.cancelled();
    return result;
  }

  std::wstring command;
  for (std::size_t i = 0; i < args.size(); ++i) {
    if (i != 0) command.push_back(L' ');
    command += quoteArg(utf8ToWide(args[i]));
  }
  std::vector<wchar_t> mutableCommand(command.begin(), command.end());
  mutableCommand.push_back(L'\0');

  SECURITY_ATTRIBUTES security{};
  security.nLength = sizeof(security);
  security.bInheritHandle = TRUE;

  HANDLE outRead = nullptr;
  HANDLE outWrite = nullptr;
  HANDLE errRead = nullptr;
  HANDLE errWrite = nullptr;
  if (!CreatePipe(&outRead, &outWrite, &security, 0) || !CreatePipe(&errRead, &errWrite, &security, 0)) {
    if (outRead) CloseHandle(outRead);
    if (outWrite) CloseHandle(outWrite);
    if (errRead) CloseHandle(errRead);
    if (errWrite) CloseHandle(errWrite);
    return result;
  }
  SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(errRead, HANDLE_FLAG_INHERIT, 0);
  HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr);

  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = nul == INVALID_HANDLE_VALUE ? nullptr : nul;
  startup.hStdOutput = outWrite;
  startup.hStdError = errWrite;
  PROCESS_INFORMATION process{};

  const BOOL started = CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                                      nullptr, nullptr, &startup, &process);
  if (outWrite) CloseHandle(outWrite);
  if (errWrite) CloseHandle(errWrite);
  if (nul != nullptr && nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
  if (!started) {
    CloseHandle(outRead);
    CloseHandle(errRead);
    return result;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    process_ = process.hProcess;
    if (cancel.cancelled()) TerminateProcess(process.hProcess, 1);
  }

  std::thread outThread(readPipe, outRead, std::ref(result.out));
  std::thread errThread(readPipe, errRead, std::ref(result.err));

  bool killed = false;
  while (WaitForSingleObject(process.hProcess, 100) == WAIT_TIMEOUT) {
    if (cancel.cancelled()) {
      TerminateProcess(process.hProcess, 1);
      killed = true;
      WaitForSingleObject(process.hProcess, 5000);
      break;
    }
  }

  DWORD code = 1;
  GetExitCodeProcess(process.hProcess, &code);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    process_ = nullptr;
  }
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  outThread.join();
  errThread.join();
  CloseHandle(outRead);
  CloseHandle(errRead);

  result.started = true;
  result.cancelled = killed || cancel.cancelled();
  result.exitCode = static_cast<int>(code);
  return result;
}

void ProcessRunner::terminate() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (process_ != nullptr) TerminateProcess(static_cast<HANDLE>(process_), 1);
}

}  // namespace skipper
