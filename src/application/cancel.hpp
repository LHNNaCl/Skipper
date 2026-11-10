#pragma once

#include <atomic>

namespace skipper {

class CancelToken {
 public:
  void cancel();
  void reset();
  bool cancelled() const;

 private:
  std::atomic<bool> flag_{false};
};

}  // namespace skipper
