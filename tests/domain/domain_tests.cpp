#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "domain/audio_match.hpp"
#include "domain/black_frame.hpp"
#include "domain/chapter_picker.hpp"
#include "domain/edit_check.hpp"
#include "domain/keep_ranges.hpp"
#include "domain/output_name.hpp"
#include "domain/resolver.hpp"
#include "domain/window.hpp"

#include <vector>

using namespace skipper;

namespace {

DetectionLimits limits() { return {}; }

Fingerprint print(double start, double step, std::vector<std::uint32_t> points) {
  Fingerprint fingerprint;
  fingerprint.windowStartSec = start;
  fingerprint.secondsPerPoint = step;
  fingerprint.points = std::move(points);
  return fingerprint;
}

std::vector<std::uint32_t> sequence(std::uint32_t first, int count) {
  // LCG values stay far apart in Hamming distance, like real chromaprint points.
  std::vector<std::uint32_t> points;
  points.reserve(static_cast<std::size_t>(count));
  std::uint32_t x = first * 0x9E3779B9u + 0xA5A5A5A5u;
  for (int i = 0; i < count; ++i) {
    x = x * 1664525u + 1013904223u;
    points.push_back(x);
  }
  return points;
}

std::vector<VisualSample> band(double start, double end, double step, double black, double saturation) {
  std::vector<VisualSample> samples;
  for (double time = start; time <= end + 1e-9; time += step) {
    samples.push_back(VisualSample{time, black, saturation});
  }
  return samples;
}

EpisodeEvidence episode(std::string path, double duration) {
  EpisodeEvidence evidence;
  evidence.path = std::move(path);
  evidence.name = evidence.path;
  evidence.durationSec = duration;
  return evidence;
}

void expectRange(const std::optional<TimeRange>& range, double start, double end) {
  REQUIRE(range.has_value());
  CHECK(range->startSec == doctest::Approx(start));
  CHECK(range->endSec == doctest::Approx(end));
}

void expectSegment(const std::optional<Segment>& segment, double start, double end, SegmentSource source) {
  REQUIRE(segment.has_value());
  CHECK(segment->startSec == doctest::Approx(start));
  CHECK(segment->endSec == doctest::Approx(end));
  CHECK(segment->source == source);
}

}  // namespace

TEST_CASE("search windows") {
  const auto limits = ::limits();
  auto longForm = windowFor(2400, limits);
  CHECK(longForm.intro.startSec == doctest::Approx(0));
  CHECK(longForm.intro.endSec == doctest::Approx(600));
  CHECK(longForm.credits.startSec == doctest::Approx(1800));
  CHECK(longForm.credits.endSec == doctest::Approx(2400));

  auto medium = windowFor(1200, limits);
  CHECK(medium.intro.endSec == doctest::Approx(300));
  CHECK(medium.credits.startSec == doctest::Approx(600));
  CHECK(medium.credits.endSec == doctest::Approx(1200));

  auto shortForm = windowFor(60, limits);
  CHECK(shortForm.intro.endSec == doctest::Approx(15));
  CHECK(shortForm.credits.startSec == doctest::Approx(0));
  CHECK(shortForm.credits.endSec == doctest::Approx(60));

  Window intro{0, 75};
  auto widenedIntro = widen(intro, 200, 300, true);
  CHECK(widenedIntro.startSec == doctest::Approx(0));
  CHECK(widenedIntro.endSec == doctest::Approx(200));

  Window credits{1400, 2000};
  auto widenedCredits = widen(credits, 700, 2000, false);
  CHECK(widenedCredits.startSec == doctest::Approx(1300));
  CHECK(widenedCredits.endSec == doctest::Approx(2000));
}

TEST_CASE("output file name keeps the extension") {
  CHECK(outputFileName("ep01.mkv") == "ep01_trimmed.mkv");
  CHECK(outputFileName("file.MP4") == "file_trimmed.MP4");
  CHECK(outputFileName("archive.tar.mkv") == "archive.tar_trimmed.mkv");
  CHECK(fileNameOf("D:/show/ep01.mkv") == "ep01.mkv");
  CHECK(fileNameOf("D:\\show\\ep01.mkv") == "ep01.mkv");
}

TEST_CASE("chapter titles") {
  const auto limits = ::limits();
  const Window window{0, 600};

  auto adopted = pickChapter({Chapter{"OP1", 10, 100}}, window, true, limits);
  expectRange(adopted, 10, 100);

  CHECK_FALSE(pickChapter({Chapter{"Intro End", 10, 100}}, window, true, limits));
  CHECK_FALSE(pickChapter({Chapter{"片头结束", 10, 100}}, window, true, limits));
  CHECK_FALSE(pickChapter({Chapter{"OP: End", 10, 100}}, window, true, limits));
  CHECK_FALSE(pickChapter({Chapter{"HOPE", 10, 100}}, window, true, limits));
  CHECK_FALSE(pickChapter({Chapter{"EDITED", 10, 100}}, window, false, limits));

  auto earlier = pickChapter({Chapter{"Opening", 80, 140}, Chapter{"Intro", 20, 90}}, window, true, limits);
  expectRange(earlier, 20, 90);

  const Window credits{1400, 2000};
  auto later = pickChapter({Chapter{"Ending", 1500, 1560}, Chapter{"Credits", 1600, 1700}}, credits, false, limits);
  expectRange(later, 1600, 1700);

  auto tie = pickChapter({Chapter{"ED", 1500, 1700}, Chapter{"Outro", 1620, 1700}}, credits, false, limits);
  expectRange(tie, 1620, 1700);

  CHECK_FALSE(pickChapter({Chapter{"Intro", 0, 10}}, window, true, limits));
  CHECK_FALSE(pickChapter({Chapter{"Intro", 0, 200}}, window, true, limits));
  CHECK(pickChapter({Chapter{"intro", 10, 40}}, window, true, limits));
}

TEST_CASE("fingerprint alignment") {
  const auto limits = ::limits();
  const auto shared = sequence(1, 3);
  auto match = matchFingerprints(print(0, 1, {shared[0], shared[1], shared[2], sequence(9, 1)[0]}),
                                 print(0, 1, {sequence(8, 1)[0], shared[0], shared[1], shared[2]}), limits);
  REQUIRE(match);
  CHECK(match->left.startSec == doctest::Approx(0));
  CHECK(match->left.endSec == doctest::Approx(3));
  CHECK(match->right.startSec == doctest::Approx(1));
  CHECK(match->right.endSec == doctest::Approx(4));

  const auto ends = sequence(2, 1);
  auto gapped = matchFingerprints(print(0, 1, {shared[0], sequence(9, 1)[0], ends[0]}),
                                  print(0, 1, {shared[0], sequence(8, 1)[0], ends[0]}), limits);
  REQUIRE(gapped);
  CHECK(gapped->left.endSec - gapped->left.startSec == doctest::Approx(3));

  std::vector<Window> full(2, Window{0, 100});
  auto tooMuch = bestPairwise(
      {print(0, 1, sequence(1, 90)), print(0, 1, sequence(1, 90))}, full, limits);
  CHECK_FALSE(tooMuch[0]);
  CHECK_FALSE(tooMuch[1]);

  auto kept = bestPairwise(
      {print(0, 1, sequence(1, 89)), print(0, 1, sequence(1, 89))}, full, limits);
  expectRange(kept[0], 0, 89);
  expectRange(kept[1], 0, 89);

  std::vector<Window> wide(3, Window{0, 200});
  auto ranked = bestPairwise(
      {print(0, 1, sequence(100, 40)), print(0, 1, sequence(100, 20)), print(0, 1, sequence(100, 40))},
      wide, limits);
  expectRange(ranked[0], 0, 40);
  expectRange(ranked[1], 0, 20);
  expectRange(ranked[2], 0, 40);

  CHECK_FALSE(bestPairwise({print(0, 1, sequence(1, 30))}, {Window{0, 100}}, limits)[0]);

  const auto templ = sequence(1, 10);
  std::vector<std::uint32_t> weakTarget(templ.begin(), templ.begin() + 7);
  auto weak = matchTemplate(print(0, 1, templ), print(0, 1, weakTarget), Window{0, 100}, limits);
  CHECK_FALSE(weak);

  std::vector<std::uint32_t> strongTarget = {sequence(50, 1)[0], sequence(51, 1)[0]};
  strongTarget.insert(strongTarget.end(), templ.begin(), templ.begin() + 8);
  auto strong = matchTemplate(print(0, 1, templ), print(2, 1, strongTarget), Window{0, 100}, limits);
  expectRange(strong, 4, 14);

  auto outside = matchTemplate(print(0, 1, sequence(1, 10)), print(95, 1, sequence(1, 10)), Window{0, 100}, limits);
  CHECK_FALSE(outside);

  // Near-equal chromaprint points (1 differing bit) should still align.
  std::vector<std::uint32_t> leftNear = sequence(0x1000, 40);
  std::vector<std::uint32_t> rightNear = leftNear;
  for (auto& point : rightNear) point ^= 0x80000000u;
  auto fuzzy = matchFingerprints(print(10, 1, leftNear), print(0, 1, rightNear), limits);
  REQUIRE(fuzzy);
  CHECK(fuzzy->left.endSec - fuzzy->left.startSec == doctest::Approx(40));
  CHECK(fuzzy->right.startSec == doctest::Approx(0));

  DetectionLimits exactOnly = limits;
  exactOnly.maxHammingBits = 0;
  CHECK_FALSE(matchFingerprints(print(10, 1, leftNear), print(0, 1, rightNear), exactOnly));
}

TEST_CASE("black frames become one segment") {
  const auto limits = ::limits();
  const Window window{0, 600};
  expectRange(pickBlackFrame(band(100, 120, 0.5, 95, 0), window, 1000, limits), 100, 120);
  expectRange(pickBlackFrame(band(100, 120, 0.5, 75, 10), window, 1000, limits), 100, 120);
  CHECK_FALSE(pickBlackFrame(band(100, 120, 0.5, 75, 40), window, 1000, limits));

  auto merged = band(0, 20, 0.5, 95, 0);
  auto second = band(23, 43, 0.5, 95, 0);
  merged.insert(merged.end(), second.begin(), second.end());
  expectRange(pickBlackFrame(merged, window, 1000, limits), 0, 43);

  auto split = band(0, 20, 0.5, 95, 0);
  auto later = band(23.1, 43.1, 0.5, 95, 0);
  split.insert(split.end(), later.begin(), later.end());
  expectRange(pickBlackFrame(split, window, 1000, limits), 23.1, 43.1);
}

TEST_CASE("resolve picks one segment per side") {
  const auto limits = ::limits();

  EpisodeEvidence only = episode("a.mkv", 2000);
  only.creditsVisual = band(1800, 1820, 0.5, 95, 0);
  auto blackOnly = resolve({only}, std::nullopt, limits);
  expectSegment(blackOnly[0].credits, 1800, 1820, SegmentSource::BlackFrame);

  EpisodeEvidence left = episode("a.mkv", 2000);
  EpisodeEvidence right = episode("b.mkv", 2000);
  left.creditsVisual = band(1800, 1860, 0.5, 95, 0);
  left.creditsWindow = print(1862, 1, sequence(10, 38));
  right.creditsWindow = print(1862, 1, sequence(10, 38));
  auto merged = resolve({left, right}, std::nullopt, limits);
  expectSegment(merged[0].credits, 1800, 1900, SegmentSource::BlackFrameAndAudio);

  EpisodeEvidence longBlack = episode("a.mkv", 2000);
  EpisodeEvidence partner = episode("b.mkv", 2000);
  longBlack.creditsVisual = band(1500, 1680, 0.5, 95, 0);
  longBlack.creditsWindow = print(1682, 1, sequence(50, 20));
  partner.creditsWindow = print(1682, 1, sequence(50, 20));
  auto tooLong = resolve({longBlack, partner}, std::nullopt, limits);
  expectSegment(tooLong[0].credits, 1500, 1680, SegmentSource::BlackFrame);

  EpisodeEvidence chaptered = episode("a.mkv", 2000);
  chaptered.chapters.push_back(Chapter{"Ending", 1500, 1580});
  chaptered.creditsVisual = band(1700, 1760, 0.5, 95, 0);
  chaptered.creditsWindow = print(1800, 1, sequence(3, 40));
  EpisodeEvidence other = episode("b.mkv", 2000);
  other.creditsWindow = print(1800, 1, sequence(3, 40));
  auto chapterWins = resolve({chaptered, other}, std::nullopt, limits);
  expectSegment(chapterWins[0].credits, 1500, 1580, SegmentSource::Chapter);

  EpisodeEvidence standardEp = episode("s.mkv", 400);
  EpisodeEvidence good = episode("good.mkv", 400);
  EpisodeEvidence bad = episode("bad.mkv", 400);
  const auto introTempl = sequence(1, 10);
  std::vector<std::uint32_t> goodIntro(introTempl.begin(), introTempl.begin() + 8);
  goodIntro.push_back(sequence(90, 1)[0]);
  goodIntro.push_back(sequence(91, 1)[0]);
  std::vector<std::uint32_t> badIntro(introTempl.begin(), introTempl.begin() + 7);
  badIntro.push_back(sequence(92, 1)[0]);
  badIntro.push_back(sequence(93, 1)[0]);
  badIntro.push_back(sequence(94, 1)[0]);
  good.introWindow = print(0, 1, goodIntro);
  bad.introWindow = print(0, 1, badIntro);
  bad.chapters.push_back(Chapter{"Intro", 30, 80});
  StandardEvidence mark;
  mark.path = "s.mkv";
  mark.intro = TimeRange{0, 10};
  mark.introTemplate = print(0, 1, introTempl);
  auto marked = resolve({standardEp, good, bad}, mark, limits);
  expectSegment(marked[0].intro, 0, 10, SegmentSource::Standard);
  expectSegment(marked[1].intro, 0, 10, SegmentSource::Standard);
  expectSegment(marked[2].intro, 30, 80, SegmentSource::Chapter);

  DetectionMethods noChapter;
  noChapter.chapter = false;
  noChapter.audio = false;
  auto withoutChapter = resolve({chaptered, other}, std::nullopt, limits, noChapter);
  expectSegment(withoutChapter[0].credits, 1700, 1760, SegmentSource::BlackFrame);
  CHECK_FALSE(withoutChapter[1].credits);

  DetectionMethods onlyAudio;
  onlyAudio.standard = false;
  onlyAudio.chapter = false;
  onlyAudio.blackFrame = false;
  auto audioOnly = resolve({chaptered, other}, std::nullopt, limits, onlyAudio);
  expectSegment(audioOnly[0].credits, 1800, 1840, SegmentSource::Audio);

  DetectionMethods none;
  none.standard = false;
  none.chapter = false;
  none.audio = false;
  none.blackFrame = false;
  auto ignored = resolve({standardEp, good, bad}, mark, limits, none);
  CHECK_FALSE(ignored[0].intro);
  CHECK_FALSE(ignored[2].intro);

  EpisodeEvidence host = episode("s.mkv", 300);
  EpisodeEvidence guest = episode("g.mkv", 300);
  guest.introWindow = print(0, 10, sequence(1, 20));
  StandardEvidence wide;
  wide.path = "s.mkv";
  wide.intro = TimeRange{0, 200};
  wide.introTemplate = print(0, 10, sequence(1, 20));
  auto widened = resolve({host, guest}, wide, limits);
  expectSegment(widened[1].intro, 0, 200, SegmentSource::Standard);
}

TEST_CASE("manual edits") {
  const std::unordered_map<std::string, double> durations{{"a.mkv", 1000}, {"b.mkv", 1000}};

  ConfirmedEpisode half;
  half.path = "a.mkv";
  half.intro.startSec = 10;
  auto halfErrors = validate({half}, durations);
  REQUIRE(halfErrors.size() == 1);
  CHECK(halfErrors[0].field == Field::Intro);
  CHECK(halfErrors[0].code == FieldCode::HalfEmpty);

  ConfirmedEpisode order;
  order.path = "a.mkv";
  order.intro.startSec = 40;
  order.intro.endSec = 10;
  auto orderErrors = validate({order}, durations);
  REQUIRE(orderErrors.size() == 1);
  CHECK(orderErrors[0].field == Field::IntroStart);
  CHECK(orderErrors[0].code == FieldCode::Order);

  ConfirmedEpisode out;
  out.path = "a.mkv";
  out.intro.startSec = 0;
  out.intro.endSec = 1001;
  auto outErrors = validate({out}, durations);
  REQUIRE(outErrors.size() == 1);
  CHECK(outErrors[0].field == Field::IntroEnd);
  CHECK(outErrors[0].code == FieldCode::OutOfRange);

  ConfirmedEpisode overlap;
  overlap.path = "a.mkv";
  overlap.intro = Endpoint{0, 100};
  overlap.credits = Endpoint{90, 120};
  auto overlapErrors = validate({overlap}, durations);
  REQUIRE(overlapErrors.size() == 1);
  CHECK(overlapErrors[0].field == Field::Pair);
  CHECK(overlapErrors[0].code == FieldCode::Overlap);

  ConfirmedEpisode longIntro;
  longIntro.path = "a.mkv";
  longIntro.intro = Endpoint{0, 200};
  CHECK(validate({longIntro}, durations).empty());

  ConfirmedEpisode edge;
  edge.path = "a.mkv";
  edge.intro = Endpoint{0, 100};
  edge.credits = Endpoint{100, 1000};
  CHECK(validate({edge}, durations).empty());

  ConfirmedEpisode excluded = order;
  excluded.excluded = true;
  CHECK(validate({excluded}, durations).empty());

  ConfirmedEpisode unknown;
  unknown.path = "missing.mkv";
  unknown.intro = Endpoint{0, 10};
  auto unknownErrors = validate({unknown}, durations);
  REQUIRE(unknownErrors.size() == 1);
  CHECK(unknownErrors[0].field == Field::Intro);
  CHECK(unknownErrors[0].code == FieldCode::OutOfRange);
}

TEST_CASE("keep ranges snap to keyframes") {
  std::optional<TimeRange> intro = TimeRange{0, 12};
  std::optional<TimeRange> credits = TimeRange{28, 40};
  auto snapped = planKeepRanges(40, intro, credits, {0, 10, 20, 30});
  REQUIRE(snapped.size() == 1);
  CHECK(snapped[0].startSec == doctest::Approx(10));
  CHECK(snapped[0].endSec == doctest::Approx(30));

  auto joined = planKeepRanges(30, TimeRange{8, 12}, std::nullopt, {0, 10, 20, 30});
  REQUIRE(joined.size() == 1);
  CHECK(joined[0].startSec == doctest::Approx(0));
  CHECK(joined[0].endSec == doctest::Approx(30));

  auto dropped = planKeepRanges(10, TimeRange{20, 22}, TimeRange{25, 30}, {});
  REQUIRE(dropped.size() == 1);
  CHECK(dropped[0].startSec == doctest::Approx(0));
  CHECK(dropped[0].endSec == doctest::Approx(10));
  for (const auto& range : dropped) CHECK(range.startSec < range.endSec);

  auto empty = planKeepRanges(100, TimeRange{0, 40}, TimeRange{40, 100}, {0, 50, 100});
  CHECK(empty.empty());
}
