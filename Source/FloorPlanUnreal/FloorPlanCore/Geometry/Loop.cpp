#include "Geometry/Loop.h"

#include "FloorPlanLimits.h"
#include "Geometry/Bulge.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace FloorPlan::Geometry
{
    Loop::Loop(std::vector<LoopVertex> vertices) : Points(std::move(vertices))
    {
        Rebuild();
    }

    void Loop::Rebuild()
    {
        Chords.clear();
        Area = 0.0;
        Low = Vec2{0.0, 0.0};
        High = Vec2{0.0, 0.0};
        if (Points.size() < 2)
        {
            return;
        }

        double shoelace = 0.0;
        double arcs = 0.0;
        std::vector<Vec2> interior;
        for (std::size_t index = 0; index < Points.size(); ++index)
        {
            const LoopVertex& current = Points[index];
            const LoopVertex& next = Points[(index + 1) % Points.size()];
            shoelace += current.Position.X * next.Position.Y -
                        next.Position.X * current.Position.Y;
            arcs += Bulge::SegmentArea(current.Position, next.Position, current.Bulge);

            Chords.push_back(current.Position);
            Bulge::Tessellate(current.Position, next.Position, current.Bulge,
                              Limits::ArcTessellationSagittaMm, interior);
            Chords.insert(Chords.end(), interior.begin(), interior.end());
        }
        Area = 0.5 * shoelace + arcs;

        double minX = std::numeric_limits<double>::max();
        double minY = std::numeric_limits<double>::max();
        double maxX = -std::numeric_limits<double>::max();
        double maxY = -std::numeric_limits<double>::max();
        for (const Vec2& point : Chords)
        {
            minX = std::min(minX, point.X);
            minY = std::min(minY, point.Y);
            maxX = std::max(maxX, point.X);
            maxY = std::max(maxY, point.Y);
        }
        Low = Vec2{minX, minY};
        High = Vec2{maxX, maxY};
    }

    bool Loop::BoundingBoxContains(const Loop& other) const
    {
        const double slack = Limits::VertexWeldToleranceMm;
        return other.Low.X >= Low.X - slack && other.Low.Y >= Low.Y - slack &&
               other.High.X <= High.X + slack && other.High.Y <= High.Y + slack;
    }

    bool Loop::Contains(const Vec2& point) const
    {
        if (Chords.size() < 3)
        {
            return false;
        }
        int winding = 0;
        for (std::size_t index = 0; index < Chords.size(); ++index)
        {
            const Vec2& start = Chords[index];
            const Vec2& end = Chords[(index + 1) % Chords.size()];
            const double side =
                (end.X - start.X) * (point.Y - start.Y) - (end.Y - start.Y) * (point.X - start.X);
            if (start.Y <= point.Y)
            {
                if (end.Y > point.Y && side > 0.0)
                {
                    ++winding;
                }
            }
            else if (end.Y <= point.Y && side < 0.0)
            {
                --winding;
            }
        }
        return winding != 0;
    }

    bool Loop::InteriorPoint(Vec2& point) const
    {
        if (Chords.size() < 3)
        {
            return false;
        }
        for (std::size_t index = 0; index < Chords.size(); ++index)
        {
            const Vec2& a = Chords[index];
            const Vec2& b = Chords[(index + 1) % Chords.size()];
            const Vec2& c = Chords[(index + 2) % Chords.size()];
            const Vec2 candidate{(a.X + b.X + c.X) / 3.0, (a.Y + b.Y + c.Y) / 3.0};
            if (Contains(candidate))
            {
                point = candidate;
                return true;
            }
        }
        const Vec2 centre{(Low.X + High.X) * 0.5, (Low.Y + High.Y) * 0.5};
        if (Contains(centre))
        {
            point = centre;
            return true;
        }
        return false;
    }
}
