#include "Geometry/Bulge.h"

#include "FloorPlanLimits.h"

#include <algorithm>
#include <cmath>

namespace FloorPlan::Geometry
{
    bool Bulge::IsStraight(double bulge)
    {
        return !std::isfinite(bulge) || std::fabs(bulge) < Limits::ZeroBulgeMagnitude;
    }

    BulgeArc Bulge::Resolve(const Vec2& start, const Vec2& end, double bulge)
    {
        BulgeArc arc;
        const Vec2 chord = end - start;
        const double chordLength = chord.Length();
        if (IsStraight(bulge) || chordLength < Limits::ZeroChordLengthMm)
        {
            return arc;
        }

        arc.IsStraight = false;
        arc.IncludedAngle = 4.0 * std::atan(bulge);
        arc.Radius = chordLength * (1.0 + bulge * bulge) / (4.0 * std::fabs(bulge));

        const double offset = (1.0 - bulge * bulge) / (4.0 * bulge);
        arc.Center = Vec2{start.X + 0.5 * chord.X - offset * chord.Y,
                          start.Y + 0.5 * chord.Y + offset * chord.X};
        arc.StartAngle = std::atan2(start.Y - arc.Center.Y, start.X - arc.Center.X);
        return arc;
    }

    double Bulge::SegmentArea(const Vec2& start, const Vec2& end, double bulge)
    {
        const BulgeArc arc = Resolve(start, end, bulge);
        if (arc.IsStraight)
        {
            return 0.0;
        }
        return 0.5 * arc.Radius * arc.Radius *
               (arc.IncludedAngle - std::sin(arc.IncludedAngle));
    }

    std::size_t Bulge::SegmentCount(const BulgeArc& arc, double sagittaTolerance)
    {
        if (arc.IsStraight || arc.Radius <= 0.0 || sagittaTolerance <= 0.0)
        {
            return 1;
        }
        const double ratio = 1.0 - sagittaTolerance / arc.Radius;
        if (ratio <= -1.0)
        {
            return 1;
        }
        const double step = 2.0 * std::acos(std::min(1.0, ratio));
        if (!(step > 0.0))
        {
            return 1;
        }
        const double count = std::ceil(std::fabs(arc.IncludedAngle) / step);
        if (!std::isfinite(count) || count < 1.0)
        {
            return 1;
        }
        const double bounded = std::min(count, static_cast<double>(Limits::MaxVerticesPerPolyline));
        return static_cast<std::size_t>(bounded);
    }

    void Bulge::Tessellate(const Vec2& start, const Vec2& end, double bulge,
                            double sagittaTolerance, std::vector<Vec2>& interior)
    {
        interior.clear();
        const BulgeArc arc = Resolve(start, end, bulge);
        if (arc.IsStraight)
        {
            return;
        }
        const std::size_t segments = SegmentCount(arc, sagittaTolerance);
        if (segments < 2)
        {
            return;
        }
        interior.reserve(segments - 1);
        const double step = arc.IncludedAngle / static_cast<double>(segments);
        for (std::size_t index = 1; index < segments; ++index)
        {
            const double angle = arc.StartAngle + step * static_cast<double>(index);
            interior.push_back(Vec2{arc.Center.X + arc.Radius * std::cos(angle),
                                    arc.Center.Y + arc.Radius * std::sin(angle)});
        }
    }
}
