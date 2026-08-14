#pragma once

#include "Geometry/Vec2.h"

#include <vector>

namespace FloorPlan::Geometry
{
    /// An ellipse as DXF states one: a centre, a vector to the major axis end, and a ratio.
    class EllipticArc
    {
    public:
        /// The parameter is the ellipse's own angle, not the angle subtended at the centre.
        static Vec2 Evaluate(const Vec2& centre, const Vec2& majorAxis, double minorRatio,
                             double parameterRadians);

        /// Samples from start to end inclusive; a curve has no exact bulge so it is chorded.
        static void Tessellate(const Vec2& centre, const Vec2& majorAxis, double minorRatio,
                               double startRadians, double endRadians, bool counterClockwise,
                               double sagittaTolerance, std::vector<Vec2>& points);
    };
}
