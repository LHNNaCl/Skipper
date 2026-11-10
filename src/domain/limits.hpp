#pragma once

namespace skipper {

struct DetectionLimits {
  double minSegmentSec = 15;
  double maxSegmentSec = 180;
  double introFraction = 0.25;
  double introCapSec = 600;
  double creditsWindowSec = 600;
  double mergeGapSec = 3;
  double fullWindowRatio = 0.90;
  double templateCoverage = 0.80;
  int maxGapPoints = 1;
  int maxBucketSize = 50;
  // Chromaprint points rarely match bit-exact across files; allow this many
  // differing bits when extending a seeded offset into a continuous run.
  int maxHammingBits = 5;
  double blackPercent = 90;
  double darkPercent = 70;
  double maxSaturation = 20;
  double visualGapSec = 1.0;
};

}  // namespace skipper
