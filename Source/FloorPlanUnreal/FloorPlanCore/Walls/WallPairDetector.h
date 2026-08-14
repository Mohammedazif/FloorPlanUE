#pragma once

#include "Geometry/LoopAssembler.h"
#include "Geometry/Vec2.h"

#include <cstddef>
#include <vector>

namespace FloorPlan::Walls
{
    struct WallCandidate
    {
        Geometry::Vec2 Start;
        Geometry::Vec2 End;
        double Bulge = 0.0;
        double ThicknessMm = 0.0;
        std::size_t FirstFace = 0;
        std::size_t SecondFace = 0;

        bool IsCurved() const;
    };

    struct PairingReport
    {
        std::size_t Considered = 0;
        std::size_t Paired = 0;
        std::size_t PairedCurved = 0;
        std::size_t RejectedByAngle = 0;
        std::size_t RejectedByGap = 0;
        std::size_t RejectedByOverlap = 0;
    };

    /// Recovers wall centrelines from the parallel face pairs a double-line drawing uses.
    class WallPairDetector
    {
    public:
        static void Detect(const std::vector<Geometry::Segment>& faces,
                           std::vector<WallCandidate>& walls, PairingReport& report);
    };
}
