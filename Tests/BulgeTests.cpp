#include "FloorPlanLimits.h"
#include "Geometry/Bulge.h"
#include "TestHarness.h"

#include <cmath>
#include <vector>

using FloorPlan::Geometry::Bulge;
using FloorPlan::Geometry::BulgeArc;
using FloorPlan::Geometry::Vec2;

namespace
{
    constexpr double Pi = 3.14159265358979323846;

    double ShoelaceWithBulges(const std::vector<Vec2>& points, const std::vector<double>& bulges)
    {
        double total = 0.0;
        for (std::size_t index = 0; index < points.size(); ++index)
        {
            const Vec2& current = points[index];
            const Vec2& next = points[(index + 1) % points.size()];
            total += current.X * next.Y - next.X * current.Y;
        }
        double area = 0.5 * total;
        for (std::size_t index = 0; index < points.size(); ++index)
        {
            const Vec2& current = points[index];
            const Vec2& next = points[(index + 1) % points.size()];
            area += Bulge::SegmentArea(current, next, bulges[index]);
        }
        return area;
    }
}

FLOORPLAN_TEST(Bulge, ZeroBulgeIsStraight)
{
    const BulgeArc arc = Bulge::Resolve(Vec2{0.0, 0.0}, Vec2{100.0, 0.0}, 0.0);
    CHECK(arc.IsStraight);
    CHECK_NEAR(Bulge::SegmentArea(Vec2{0.0, 0.0}, Vec2{100.0, 0.0}, 0.0), 0.0, 1e-12);
}

FLOORPLAN_TEST(Bulge, UnitBulgeIsASemicircle)
{
    const BulgeArc arc = Bulge::Resolve(Vec2{5000.0, 0.0}, Vec2{5000.0, 4000.0}, 1.0);
    CHECK(!arc.IsStraight);
    CHECK_NEAR(arc.Radius, 2000.0, 1e-9);
    CHECK_NEAR(arc.IncludedAngle, Pi, 1e-12);
    CHECK_NEAR(arc.Center.X, 5000.0, 1e-9);
    CHECK_NEAR(arc.Center.Y, 2000.0, 1e-9);
}

FLOORPLAN_TEST(Bulge, PositiveBulgeBowsRightOfTheChord)
{
    const BulgeArc arc = Bulge::Resolve(Vec2{5000.0, 0.0}, Vec2{5000.0, 4000.0}, 1.0);
    const double apexAngle = arc.StartAngle + arc.IncludedAngle * 0.5;
    const double apexX = arc.Center.X + arc.Radius * std::cos(apexAngle);
    const double apexY = arc.Center.Y + arc.Radius * std::sin(apexAngle);
    CHECK_NEAR(apexX, 7000.0, 1e-9);
    CHECK_NEAR(apexY, 2000.0, 1e-9);
}

FLOORPLAN_TEST(Bulge, SemicircleSegmentAreaIsHalfACircle)
{
    const double area = Bulge::SegmentArea(Vec2{5000.0, 0.0}, Vec2{5000.0, 4000.0}, 1.0);
    CHECK_NEAR(area, Pi * 2000.0 * 2000.0 / 2.0, 1e-6);
}

FLOORPLAN_TEST(Bulge, ArcWallInteriorMatchesTheGoldenArea)
{
    const std::vector<Vec2> points{
        Vec2{0.0, 0.0}, Vec2{5000.0, 0.0}, Vec2{5000.0, 4000.0}, Vec2{0.0, 4000.0}};
    const std::vector<double> bulges{0.0, 1.0, 0.0, 0.0};
    CHECK_NEAR(ShoelaceWithBulges(points, bulges), 26283185.307179585, 1e-6);
}

FLOORPLAN_TEST(Bulge, ArcWallExteriorMatchesTheGoldenArea)
{
    const std::vector<Vec2> points{Vec2{-200.0, -200.0}, Vec2{5000.0, -200.0},
                                   Vec2{5000.0, 4200.0}, Vec2{-200.0, 4200.0}};
    const std::vector<double> bulges{0.0, 1.0, 0.0, 0.0};
    CHECK_NEAR(ShoelaceWithBulges(points, bulges), 30482654.221687298, 1e-6);
}

FLOORPLAN_TEST(Bulge, ArcWallFootprintMatchesTheGoldenInvariant)
{
    const std::vector<Vec2> interior{Vec2{0.0, 0.0}, Vec2{5000.0, 0.0}, Vec2{5000.0, 4000.0},
                                     Vec2{0.0, 4000.0}};
    const std::vector<Vec2> exterior{Vec2{-200.0, -200.0}, Vec2{5000.0, -200.0},
                                     Vec2{5000.0, 4200.0}, Vec2{-200.0, 4200.0}};
    const std::vector<double> bulges{0.0, 1.0, 0.0, 0.0};
    const double footprint =
        ShoelaceWithBulges(exterior, bulges) - ShoelaceWithBulges(interior, bulges);
    CHECK_NEAR(footprint, 4199468.914507713, 1e-6);
}

FLOORPLAN_TEST(Bulge, NegativeBulgeSubtractsWhereItsPositivePeerAdds)
{
    const std::vector<Vec2> points{Vec2{0.0, 0.0}, Vec2{5000.0, 0.0}, Vec2{5000.0, 4000.0},
                                   Vec2{0.0, 4000.0}};
    const double outward = ShoelaceWithBulges(points, {0.0, 1.0, 0.0, 0.0});
    const double inward = ShoelaceWithBulges(points, {0.0, -1.0, 0.0, 0.0});
    CHECK_NEAR(outward, 20000000.0 + Pi * 2000.0 * 2000.0 / 2.0, 1e-6);
    CHECK_NEAR(inward, 20000000.0 - Pi * 2000.0 * 2000.0 / 2.0, 1e-6);
}

FLOORPLAN_TEST(Bulge, MajorArcCentreCrossesToTheApexSide)
{
    const double bulge = std::tan(3.0 * Pi / 8.0);
    const BulgeArc arc = Bulge::Resolve(Vec2{1000.0, 0.0}, Vec2{0.0, -1000.0}, bulge);
    CHECK_NEAR(arc.Center.X, 0.0, 1e-6);
    CHECK_NEAR(arc.Center.Y, 0.0, 1e-6);
    CHECK_NEAR(arc.Radius, 1000.0, 1e-9);
    CHECK_NEAR(std::fabs(arc.IncludedAngle), 3.0 * Pi / 2.0, 1e-9);
}

FLOORPLAN_TEST(Bulge, TwoVertexFullCircleAreaComesEntirelyFromBulges)
{
    const std::vector<Vec2> points{Vec2{-1000.0, 0.0}, Vec2{1000.0, 0.0}};
    const std::vector<double> bulges{1.0, 1.0};
    CHECK_NEAR(ShoelaceWithBulges(points, bulges), Pi * 1000.0 * 1000.0, 1e-6);
}

FLOORPLAN_TEST(Bulge, TessellationHonoursTheSagittaTolerance)
{
    const BulgeArc arc = Bulge::Resolve(Vec2{5000.0, 0.0}, Vec2{5000.0, 4000.0}, 1.0);
    const double tolerance = FloorPlan::Limits::ArcTessellationSagittaMm;
    const std::size_t segments = Bulge::SegmentCount(arc, tolerance);
    CHECK(segments >= 2);

    std::vector<Vec2> interior;
    Bulge::Tessellate(Vec2{5000.0, 0.0}, Vec2{5000.0, 4000.0}, 1.0, tolerance, interior);
    CHECK_EQUAL(interior.size(), segments - 1);

    for (const Vec2& point : interior)
    {
        const double distance = std::hypot(point.X - arc.Center.X, point.Y - arc.Center.Y);
        CHECK_NEAR(distance, arc.Radius, 1e-6);
    }

    const double step = std::fabs(arc.IncludedAngle) / static_cast<double>(segments);
    const double sagitta = arc.Radius * (1.0 - std::cos(step * 0.5));
    CHECK(sagitta <= tolerance);
}

FLOORPLAN_TEST(Bulge, CoincidentEndpointsProduceNoArc)
{
    const BulgeArc arc = Bulge::Resolve(Vec2{100.0, 100.0}, Vec2{100.0, 100.0}, 1.0);
    CHECK(arc.IsStraight);
    CHECK_NEAR(Bulge::SegmentArea(Vec2{100.0, 100.0}, Vec2{100.0, 100.0}, 1.0), 0.0, 1e-12);
}
