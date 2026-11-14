#include "EuclideanGenerator.h"
#include <algorithm>

namespace pattcat
{
std::vector<bool> EuclideanGenerator::bjorklund (int pulses, int steps)
{
    if (steps <= 0)
        return {};

    pulses = std::clamp (pulses, 0, steps);
    if (pulses == 0)     return std::vector<bool> ((size_t) steps, false);
    if (pulses == steps) return std::vector<bool> ((size_t) steps, true);

    // Groups start as [1] x pulses and [0] x remainder; repeatedly pair them up
    // (Bjorklund / Euclid's algorithm on the group counts) until one remainder is left.
    std::vector<std::vector<bool>> a ((size_t) pulses, std::vector<bool> { true });
    std::vector<std::vector<bool>> b ((size_t) (steps - pulses), std::vector<bool> { false });

    while (b.size() > 1)
    {
        const size_t n = std::min (a.size(), b.size());
        std::vector<std::vector<bool>> paired;
        paired.reserve (n);

        for (size_t i = 0; i < n; ++i)
        {
            auto g = a[i];
            g.insert (g.end(), b[i].begin(), b[i].end());
            paired.push_back (std::move (g));
        }

        std::vector<std::vector<bool>> remainder;
        if (a.size() > n) remainder.assign (a.begin() + (std::ptrdiff_t) n, a.end());
        else              remainder.assign (b.begin() + (std::ptrdiff_t) n, b.end());

        a = std::move (paired);
        b = std::move (remainder);
    }

    std::vector<bool> result;
    result.reserve ((size_t) steps);
    for (const auto& g : a) result.insert (result.end(), g.begin(), g.end());
    for (const auto& g : b) result.insert (result.end(), g.begin(), g.end());
    return result;
}

std::vector<bool> EuclideanGenerator::generate (int pulses, int steps, int rotation)
{
    auto pattern = bjorklund (pulses, steps);
    if (pattern.empty())
        return pattern;

    const int n = (int) pattern.size();
    const int r = ((rotation % n) + n) % n;
    std::rotate (pattern.begin(), pattern.begin() + (n - r) % n, pattern.end());
    return pattern;
}

} // namespace pattcat
