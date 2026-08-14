#pragma once

#include "Geometry/Vec2.h"

#include <vector>

namespace FloorPlan::Geometry
{
    struct BulgeArc
    {
        Vec2 Center;
        double Radius = 0.0;
        double IncludedAngle = 0.0;
        double StartAngle = 0.0;
        bool IsStraight = true;
    };

    /// Geometry of the arc a DXF bulge describes between two consecutive vertices.
    class Bulge
    {
    public:
        /// Positive bulge sweeps counter-clockwise and bows to the right of start->end.
        static BulgeArc Resolve(const Vec2& start, const Vec2& end, double bulge);

        /// Signed area between the chord and its arc; add directly to a signed shoelace sum.
        static double SegmentArea(const Vec2& start, const Vec2& end, double bulge);

        /// Number of equal chords that keep every chord within the sagitta tolerance of the arc.
        static std::size_t SegmentCount(const BulgeArc& arc, double sagittaTolerance);

        /// Interior points only; callers emit the stored endpoints to stay watertight.
        static void Tessellate(const Vec2& start, const Vec2& end, double bulge,
                               double sagittaTolerance, std::vector<Vec2>& interior);

        static bool IsStraight(double bulge);
    };
}
