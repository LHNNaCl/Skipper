#include "ui/time_format.hpp"

#include "domain/time.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

namespace skipper {
namespace {

std::string trim(std::string text) {
  auto space = [](unsigned char ch) { return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r'; };
  std::size_t begin = 0;
  while (begin < text.size() && space(static_cast<unsigned char>(text[begin]))) ++begin;
  std::size_t end = text.size();
  while (end > begin && space(static_cast<unsigned char>(text[end - 1]))) --end;
  return text.substr(begin, end - begin);
}

bool parseUnsigned(const std::string& text, double& value, bool allowFraction) {
  if (text.empty()) return false;
  const auto dot = text.find('.');
  if (dot == std::string::npos) {
    for (char ch : text) {
      if (ch < '0' || ch > '9') return false;
    }
    value = std::stod(text);
    return true;
  }
  if (!allowFraction || dot == text.size() - 1) return false;
  const std::string whole = text.substr(0, dot);
  const std::string fraction = text.substr(dot + 1);
  if (whole.empty() || fraction.empty()) return false;
  for (char ch : whole) {
    if (ch < '0' || ch > '9') return false;
  }
  for (char ch : fraction) {
    if (ch < '0' || ch > '9') return false;
  }
  value = std::stod(whole) + std::stod(fraction) / std::pow(10.0, static_cast<double>(fraction.size()));
  return true;
}

std::vector<std::string> split(const std::string& text, char separator) {
  std::vector<std::string> parts;
  std::size_t from = 0;
  while (from <= text.size()) {
    const auto pos = text.find(separator, from);
    if (pos == std::string::npos) {
      parts.push_back(text.substr(from));
      break;
    }
    parts.push_back(text.substr(from, pos - from));
    from = pos + 1;
  }
  return parts;
}

}  // namespace

TimeParse parseTime(const std::string& text) {
  const std::string trimmed = trim(text);
  TimeParse parsed;
  if (trimmed.empty()) {
    parsed.empty = true;
    parsed.ok = true;
    return parsed;
  }
  const auto parts = split(trimmed, ':');
  if (parts.size() > 3) return parsed;
  double seconds = 0;
  if (parts.size() == 1) {
    if (!parseUnsigned(parts[0], seconds, true)) return parsed;
  } else {
    double hours = 0;
    double minutes = 0;
    double secs = 0;
    if (parts.size() == 3) {
      if (!parseUnsigned(parts[0], hours, false) || !parseUnsigned(parts[1], minutes, false) ||
          !parseUnsigned(parts[2], secs, true)) {
        return parsed;
      }
    } else if (!parseUnsigned(parts[0], minutes, false) || !parseUnsigned(parts[1], secs, true)) {
      return parsed;
    }
    if (minutes >= 60 || secs >= 60) return parsed;
    seconds = hours * 3600.0 + minutes * 60.0 + secs;
  }
  parsed.ok = true;
  parsed.seconds = seconds;
  return parsed;
}

std::string formatTime(double seconds) {
  if (seconds < 0) seconds = 0;
  double rounded = std::round(seconds * 1000.0) / 1000.0;
  int whole = static_cast<int>(std::floor(rounded + 1e-9));
  int millis = static_cast<int>(std::round((rounded - whole) * 1000.0));
  if (millis >= 1000) {
    millis = 0;
    ++whole;
  }
  if (millis < 0) millis = 0;
  const int hours = whole / 3600;
  const int minutes = (whole % 3600) / 60;
  const int secs = whole % 60;
  char buffer[64];
  if (millis > 0) {
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d.%03d", hours, minutes, secs, millis);
  } else {
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hours, minutes, secs);
  }
  return buffer;
}

}  // namespace skipper
