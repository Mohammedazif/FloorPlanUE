#pragma once

#include "Walls/WallPairDetector.h"

#include <cstddef>
#include <vector>

namespace FloorPlan::Walls
{
    struct JunctionReport
    {
        std::size_t EndsExtended = 0;
        std::size_t EndsTrimmed = 0;
        std::size_t EndsLeftFree = 0;
        double LongestExtensionMm = 0.0;
    };

    /// Butt-joints wall ends so corners are neither open nor overlapping.
    class WallJunctionResolver
    {
    public:
        /// At each junction one wall owns the corner and the other stops at its face.
        /// An end with no non-parallel neighbour within reach is left exactly as it was.
        static void Close(std::vector<WallCandidate>& walls, JunctionReport& report);
    };
}
