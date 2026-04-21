#pragma once

#include <cstdint>
#include <limits>

using namespace std;
using NodeId = int32_t;
using EdgeId = int32_t;

constexpr NodeId INVALID_NODE = numeric_limits<NodeId>::max();
constexpr EdgeId INVALID_EDGE = numeric_limits<EdgeId>::max();