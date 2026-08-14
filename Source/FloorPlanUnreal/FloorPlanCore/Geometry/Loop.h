#pragma once

#include "Geometry/Vec2.h"

#include <cstddef>
#include <vector>

namespace FloorPlan::Geometry
{
    struct LoopVertex
    {
        Vec2 Position;
        double Bulge = 0.0;
    };

    /// A closed boundary of straight and circular-arc edges, in millimetres.
    class Loop
    {
    public:
        Loop() = default;
        explicit Loop(std::vector<LoopVertex> vertices);

        const std::vector<LoopVertex>& Vertices() const { return Points; }

        std::size_t EdgeCount() const { return Points.size(); }

        /// Positive when the boundary runs counter-clockwise.
        double SignedArea() const { return Area; }

        double AbsoluteArea() const { return Area < 0.0 ? -Area : Area; }

        Vec2 Minimum() const { return Low; }
        Vec2 Maximum() const { return High; }

        bool BoundingBoxContains(const Loop& other) const;

        /// Winding-number test against the tessellated boundary.
        bool Contains(const Vec2& point) const;

        /// A point guaranteed to lie strictly inside, or false when none could be found.
        bool InteriorPoint(Vec2& point) const;

        const std::vector<Vec2>& Tessellated() const { return Chords; }

    private:
        void Rebuild();

        std::vector<LoopVertex> Points;
        std::vector<Vec2> Chords;
        double Area = 0.0;
        Vec2 Low;
        Vec2 High;
    };
}
