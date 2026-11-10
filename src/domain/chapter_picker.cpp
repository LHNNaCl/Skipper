#include "domain/chapter_picker.hpp"

#include "domain/time.hpp"
#include "domain/window.hpp"

#include <cstddef>

namespace skipper {
namespace {

std::string lowerAscii(std::string text) {
  for (char& ch : text) {
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
  }
  return text;
}

bool isLetter(unsigned char ch) {
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

bool isAlnum(unsigned char ch) {
  return isLetter(ch) || (ch >= '0' && ch <= '9');
}

bool isAsciiWord(const std::string& word) {
  if (word.empty()) return false;
  for (unsigned char ch : word) {
    if (ch > 127 || !isLetter(ch)) return false;
  }
  return true;
}

bool isGap(const std::string& text, std::size_t index, std::size_t& length) {
  if (index >= text.size()) return false;
  const unsigned char ch = static_cast<unsigned char>(text[index]);
  if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == ':' || ch == '-') {
    length = 1;
    return true;
  }
  if (text.compare(index, 3, "：") == 0 || text.compare(index, 3, "—") == 0) {
    length = 3;
    return true;
  }
  return false;
}

bool followedByEnd(const std::string& text, std::size_t index) {
  std::size_t gap = 0;
  while (isGap(text, index, gap)) index += gap;
  if (text.compare(index, 3, "end") == 0) {
    const std::size_t after = index + 3;
    if (after >= text.size() || !isLetter(static_cast<unsigned char>(text[after]))) return true;
  }
  if (text.compare(index, std::string("结束").size(), "结束") == 0) return true;
  return false;
}

bool leftBoundary(const std::string& text, std::size_t index) {
  if (index == 0) return true;
  return !isAlnum(static_cast<unsigned char>(text[index - 1]));
}

bool rightBoundary(const std::string& text, std::size_t after) {
  if (after >= text.size()) return true;
  return !isLetter(static_cast<unsigned char>(text[after]));
}

bool titleMatches(const std::string& title, const std::vector<std::string>& words) {
  if (title.empty()) return false;
  const std::string text = lowerAscii(title);
  for (const std::string& word : words) {
    const bool ascii = isAsciiWord(word);
    std::size_t from = 0;
    while (from < text.size()) {
      const auto found = text.find(word, from);
      if (found == std::string::npos) break;
      const bool boundary = !ascii || (leftBoundary(text, found) && rightBoundary(text, found + word.size()));
      if (boundary && !followedByEnd(text, found + word.size())) return true;
      from = found + 1;
    }
  }
  return false;
}

const std::vector<std::string>& introWords() {
  static const std::vector<std::string> words = {
      "introduction", "opening", "intro", "op", "片头曲", "片头"};
  return words;
}

const std::vector<std::string>& creditsWords() {
  static const std::vector<std::string> words = {
      "credits", "credit", "ending", "outro", "ed", "片尾曲", "片尾"};
  return words;
}

bool betterIntro(const TimeRange& candidate, const TimeRange& current) {
  if (nearlyLess(candidate.startSec, current.startSec)) return true;
  if (nearlyEqual(candidate.startSec, current.startSec) && nearlyLess(candidate.endSec, current.endSec))
    return true;
  return false;
}

bool betterCredits(const TimeRange& candidate, const TimeRange& current) {
  if (nearlyLess(current.endSec, candidate.endSec)) return true;
  if (nearlyEqual(candidate.endSec, current.endSec) && nearlyLess(current.startSec, candidate.startSec))
    return true;
  return false;
}

}  // namespace

std::optional<TimeRange> pickChapter(const std::vector<Chapter>& chapters,
                                     const Window& window,
                                     bool intro,
                                     const DetectionLimits& limits) {
  const auto& words = intro ? introWords() : creditsWords();
  std::optional<TimeRange> best;
  for (const Chapter& chapter : chapters) {
    if (!titleMatches(chapter.title, words)) continue;
    const TimeRange range{chapter.startSec, chapter.endSec};
    if (!accepted(range, window, limits)) continue;
    if (!best || (intro ? betterIntro(range, *best) : betterCredits(range, *best))) best = range;
  }
  return best;
}

}  // namespace skipper
