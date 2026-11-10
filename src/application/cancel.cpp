#include "application/cancel.hpp"

namespace skipper {

void CancelToken::cancel() { flag_.store(true, std::memory_order_relaxed); }

void CancelToken::reset() { flag_.store(false, std::memory_order_relaxed); }

bool CancelToken::cancelled() const { return flag_.load(std::memory_order_relaxed); }

}  // namespace skipper
