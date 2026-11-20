// Port of src/sim/math/rng.ts (mulberry32). The state is an int32 like JS `| 0`.
// The order and number of draws is part of the contract: never reorder calls.
#pragma once
#include <cstdint>

namespace sim {

struct RngState {
    int32_t s = 0;
};

inline double rngNext(RngState& r) {
    r.s = static_cast<int32_t>(static_cast<uint32_t>(r.s) + 0x6d2b79f5u);
    uint32_t t = static_cast<uint32_t>(r.s);
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return static_cast<double>(t ^ (t >> 14)) / 4294967296.0;
}

inline double rngRange(RngState& r, double lo, double hi) { return lo + (hi - lo) * rngNext(r); }

}  // namespace sim
