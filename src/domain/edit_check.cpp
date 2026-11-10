#include "domain/edit_check.hpp"

#include "domain/time.hpp"

namespace skipper {
namespace {

bool checkSide(const Endpoint& endpoint,
               double duration,
               Field side,
               Field startField,
               Field endField,
               const std::string& path,
               std::vector<FieldError>& errors,
               std::optional<TimeRange>& out) {
  const bool hasStart = endpoint.startSec.has_value();
  const bool hasEnd = endpoint.endSec.has_value();
  if (!hasStart && !hasEnd) {
    out.reset();
    return true;
  }
  if (hasStart != hasEnd) {
    errors.push_back(FieldError{path, side, FieldCode::HalfEmpty});
    return false;
  }
  const double start = *endpoint.startSec;
  const double end = *endpoint.endSec;
  if (!nearlyLess(start, end)) {
    errors.push_back(FieldError{path, startField, FieldCode::Order});
    return false;
  }
  bool ok = true;
  if (nearlyLess(start, 0)) {
    errors.push_back(FieldError{path, startField, FieldCode::OutOfRange});
    ok = false;
  }
  if (nearlyLess(duration, end)) {
    errors.push_back(FieldError{path, endField, FieldCode::OutOfRange});
    ok = false;
  }
  if (!ok) return false;
  out = TimeRange{start, end};
  return true;
}

}  // namespace

std::vector<FieldError> validate(const std::vector<ConfirmedEpisode>& episodes,
                                 const std::unordered_map<std::string, double>& durations) {
  std::vector<FieldError> errors;
  for (const ConfirmedEpisode& episode : episodes) {
    if (episode.excluded) continue;
    const auto found = durations.find(episode.path);
    if (found == durations.end()) {
      errors.push_back(FieldError{episode.path, Field::Intro, FieldCode::OutOfRange});
      continue;
    }
    std::optional<TimeRange> intro;
    std::optional<TimeRange> credits;
    const bool introOk = checkSide(episode.intro, found->second, Field::Intro, Field::IntroStart,
                                   Field::IntroEnd, episode.path, errors, intro);
    const bool creditsOk = checkSide(episode.credits, found->second, Field::Credits, Field::CreditsStart,
                                     Field::CreditsEnd, episode.path, errors, credits);
    if (introOk && creditsOk && intro && credits && nearlyLess(credits->startSec, intro->endSec)) {
      errors.push_back(FieldError{episode.path, Field::Pair, FieldCode::Overlap});
    }
  }
  return errors;
}

bool sameEndpoint(const Endpoint& a, const Endpoint& b) {
  const bool aEmpty = !a.startSec && !a.endSec;
  const bool bEmpty = !b.startSec && !b.endSec;
  if (aEmpty && bEmpty) return true;
  if (!a.startSec || !a.endSec || !b.startSec || !b.endSec) return false;
  return nearlyEqual(*a.startSec, *b.startSec) && nearlyEqual(*a.endSec, *b.endSec);
}

std::optional<SegmentSource> sourceAfterEdit(const std::optional<Segment>& original,
                                             const Endpoint& edited) {
  Endpoint previous;
  if (original) {
    previous.startSec = original->startSec;
    previous.endSec = original->endSec;
  }
  if (sameEndpoint(previous, edited)) {
    if (!original) return std::nullopt;
    return original->source;
  }
  return SegmentSource::Manual;
}

}  // namespace skipper
