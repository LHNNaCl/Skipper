#pragma once

#include "domain/types.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace skipper {

std::vector<FieldError> validate(const std::vector<ConfirmedEpisode>& episodes,
                                 const std::unordered_map<std::string, double>& durations);

bool sameEndpoint(const Endpoint& a, const Endpoint& b);

std::optional<SegmentSource> sourceAfterEdit(const std::optional<Segment>& original,
                                             const Endpoint& edited);

}  // namespace skipper
