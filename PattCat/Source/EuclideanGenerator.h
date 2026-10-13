#pragma once

#include <vector>

namespace pattcat
{
/** Euclidean rhythms via Bjorklund's algorithm (pulses distributed as evenly as possible). */
class EuclideanGenerator
{
public:
    /** Returns `steps` booleans with `pulses` hits, rotated right by `rotation` steps. */
    static std::vector<bool> generate (int pulses, int steps, int rotation = 0);

    /** Plain Bjorklund sequence (first step is always a pulse when pulses > 0). */
    static std::vector<bool> bjorklund (int pulses, int steps);
};

} // namespace pattcat
