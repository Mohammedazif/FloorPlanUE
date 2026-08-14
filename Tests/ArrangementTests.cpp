#include "Geometry/Arrangement.h"
#include "TestHarness.h"

#include <algorithm>
#include <cmath>
#include <vector>

using FloorPlan::Diagnostic;
using FloorPlan::Geometry::Arrangement;
using FloorPlan::Geometry::ArrangementReport;
using FloorPlan::Geometry::Segment;
using FloorPlan::Geometry::Vec2;

namespace
{
    void AddRectangle(std::vector<Segment>& segments, double x0, double y0, double x1,
                      double y1)
    {
        segments.push_back(Segment{Vec2{x0, y0}, Vec2{x1, y0}, 0.0});
        segments.push_back(Segment{Vec2{x1, y0}, Vec2{x1, y1}, 0.0});
        segments.push_back(Segment{Vec2{x1, y1}, Vec2{x0, y1}, 0.0});
        segments.push_back(Segment{Vec2{x0, y1}, Vec2{x0, y0}, 0.0});
    }

    std::vector<double> BoundedAreas(const Arrangement& arrangement)
    {
        std::vector<double> areas;
        for (const auto& face : arrangement.Faces())
        {
            if (face.Bounded)
            {
                areas.push_back(face.SignedArea);
            }
        }
        std::sort(areas.begin(), areas.end());
        return areas;
    }
}

FLOORPLAN_TEST(Arrangement, SingleSquareYieldsOneBoundedFace)
{
    std::vector<Segment> segments;
    AddRectangle(segments, 0.0, 0.0, 5000.0, 4000.0);

    Arrangement arrangement;
    ArrangementReport report;
    Diagnostic diagnostic;
    CHECK_MESSAGE(Arrangement::Build(segments, arrangement, report, diagnostic),
                  FloorPlan::Format(diagnostic));

    const std::vector<double> areas = BoundedAreas(arrangement);
    CHECK_EQUAL(areas.size(), std::size_t{1});
    if (!areas.empty())
    {
        CHECK_NEAR(areas[0], 20000000.0, 0.001);
    }
    CHECK_EQUAL(report.BoundedFaces, std::size_t{1});
}

FLOORPLAN_TEST(Arrangement, EveryFaceHasAnUnboundedCounterpart)
{
    std::vector<Segment> segments;
    AddRectangle(segments, 0.0, 0.0, 5000.0, 4000.0);

    Arrangement arrangement;
    ArrangementReport report;
    Diagnostic diagnostic;
    CHECK(Arrangement::Build(segments, arrangement, report, diagnostic));

    std::size_t unbounded = 0;
    for (const auto& face : arrangement.Faces())
    {
        if (!face.Bounded)
        {
            ++unbounded;
        }
    }
    CHECK_EQUAL(unbounded, std::size_t{1});
}

FLOORPLAN_TEST(Arrangement, SingleLineWallsSplitAtTeeJunctions)
{
    std::vector<Segment> segments;
    AddRectangle(segments, 0.0, 0.0, 8000.0, 5000.0);
    segments.push_back(Segment{Vec2{5000.0, 0.0}, Vec2{5000.0, 5000.0}, 0.0});

    Arrangement arrangement;
    ArrangementReport report;
    Diagnostic diagnostic;
    CHECK_MESSAGE(Arrangement::Build(segments, arrangement, report, diagnostic),
                  FloorPlan::Format(diagnostic));

    const std::vector<double> areas = BoundedAreas(arrangement);
    CHECK_MESSAGE(areas.size() == 2, "divider should split the plan into two rooms, got " +
                                         std::to_string(areas.size()));
    if (areas.size() == 2)
    {
        CHECK_NEAR(areas[0], 15000000.0, 0.001);
        CHECK_NEAR(areas[1], 25000000.0, 0.001);
    }
}

FLOORPLAN_TEST(Arrangement, BoundedAreasSumToTheOuterBoundary)
{
    std::vector<Segment> segments;
    AddRectangle(segments, 0.0, 0.0, 8000.0, 5000.0);
    segments.push_back(Segment{Vec2{5000.0, 0.0}, Vec2{5000.0, 5000.0}, 0.0});

    Arrangement arrangement;
    ArrangementReport report;
    Diagnostic diagnostic;
    CHECK(Arrangement::Build(segments, arrangement, report, diagnostic));

    double total = 0.0;
    for (const double area : BoundedAreas(arrangement))
    {
        total += area;
    }
    CHECK_NEAR(total, 40000000.0, 0.001);
}

FLOORPLAN_TEST(Arrangement, CrossingSegmentsSplitEachOther)
{
    std::vector<Segment> segments;
    AddRectangle(segments, 0.0, 0.0, 6000.0, 6000.0);
    segments.push_back(Segment{Vec2{3000.0, 0.0}, Vec2{3000.0, 6000.0}, 0.0});
    segments.push_back(Segment{Vec2{0.0, 3000.0}, Vec2{6000.0, 3000.0}, 0.0});

    Arrangement arrangement;
    ArrangementReport report;
    Diagnostic diagnostic;
    CHECK(Arrangement::Build(segments, arrangement, report, diagnostic));

    const std::vector<double> areas = BoundedAreas(arrangement);
    CHECK_MESSAGE(areas.size() == 4, "a cross should make four quadrants, got " +
                                         std::to_string(areas.size()));
    for (const double area : areas)
    {
        CHECK_NEAR(area, 9000000.0, 0.001);
    }
}

FLOORPLAN_TEST(Arrangement, TwoRoomsSharingOneWallEdge)
{
    std::vector<Segment> segments;
    AddRectangle(segments, 0.0, 0.0, 4000.0, 3000.0);
    AddRectangle(segments, 4000.0, 0.0, 7000.0, 3000.0);

    Arrangement arrangement;
    ArrangementReport report;
    Diagnostic diagnostic;
    CHECK(Arrangement::Build(segments, arrangement, report, diagnostic));

    const std::vector<double> areas = BoundedAreas(arrangement);
    CHECK_EQUAL(areas.size(), std::size_t{2});
    if (areas.size() == 2)
    {
        CHECK_NEAR(areas[0], 9000000.0, 0.001);
        CHECK_NEAR(areas[1], 12000000.0, 0.001);
    }
}

FLOORPLAN_TEST(Arrangement, DanglingSegmentDoesNotCreateAFace)
{
    std::vector<Segment> segments;
    AddRectangle(segments, 0.0, 0.0, 5000.0, 4000.0);
    segments.push_back(Segment{Vec2{2500.0, 2000.0}, Vec2{3500.0, 2000.0}, 0.0});

    Arrangement arrangement;
    ArrangementReport report;
    Diagnostic diagnostic;
    CHECK(Arrangement::Build(segments, arrangement, report, diagnostic));

    const std::vector<double> areas = BoundedAreas(arrangement);
    CHECK_EQUAL(areas.size(), std::size_t{1});
    if (!areas.empty())
    {
        CHECK_NEAR(areas[0], 20000000.0, 0.001);
    }
}

FLOORPLAN_TEST(Arrangement, ArcSegmentsAreTessellatedIntoTheSubdivision)
{
    std::vector<Segment> segments;
    segments.push_back(Segment{Vec2{0.0, 0.0}, Vec2{5000.0, 0.0}, 0.0});
    segments.push_back(Segment{Vec2{5000.0, 0.0}, Vec2{5000.0, 4000.0}, 1.0});
    segments.push_back(Segment{Vec2{5000.0, 4000.0}, Vec2{0.0, 4000.0}, 0.0});
    segments.push_back(Segment{Vec2{0.0, 4000.0}, Vec2{0.0, 0.0}, 0.0});

    Arrangement arrangement;
    ArrangementReport report;
    Diagnostic diagnostic;
    CHECK(Arrangement::Build(segments, arrangement, report, diagnostic));

    const std::vector<double> areas = BoundedAreas(arrangement);
    CHECK_EQUAL(areas.size(), std::size_t{1});
    if (areas.empty())
    {
        return;
    }
    CHECK_MESSAGE(areas[0] > 20000000.0, "the bulged edge must add area");

    const double truth = 26283185.307179585;
    const double radius = 2000.0;
    const double sweep = 3.14159265358979323846;
    const double chords = 100.0;
    const double predictedDeficit =
        radius * radius * sweep * sweep * sweep / (12.0 * chords * chords);

    CHECK_MESSAGE(areas[0] < truth, "an inscribed polygon must under-measure the arc");
    CHECK_NEAR(truth - areas[0], predictedDeficit, predictedDeficit * 0.05);
}

FLOORPLAN_TEST(Arrangement, TessellatedAreaIsWeakerThanTheAnalyticPath)
{
    std::vector<Segment> segments;
    segments.push_back(Segment{Vec2{0.0, 0.0}, Vec2{5000.0, 0.0}, 0.0});
    segments.push_back(Segment{Vec2{5000.0, 0.0}, Vec2{5000.0, 4000.0}, 1.0});
    segments.push_back(Segment{Vec2{5000.0, 4000.0}, Vec2{0.0, 4000.0}, 0.0});
    segments.push_back(Segment{Vec2{0.0, 4000.0}, Vec2{0.0, 0.0}, 0.0});

    Arrangement arrangement;
    ArrangementReport report;
    Diagnostic diagnostic;
    CHECK(Arrangement::Build(segments, arrangement, report, diagnostic));

    const std::vector<double> areas = BoundedAreas(arrangement);
    CHECK_EQUAL(areas.size(), std::size_t{1});
    if (areas.empty())
    {
        return;
    }

    const double error = std::fabs(26283185.307179585 - areas[0]);
    CHECK_MESSAGE(error > 1.0,
                  "the arrangement is a topology tool; measurement must use analytic areas");
}

FLOORPLAN_TEST(Arrangement, EmptyInputIsNotAFailure)
{
    Arrangement arrangement;
    ArrangementReport report;
    Diagnostic diagnostic;
    CHECK(Arrangement::Build({}, arrangement, report, diagnostic));
    CHECK_EQUAL(report.BoundedFaces, std::size_t{0});
}
