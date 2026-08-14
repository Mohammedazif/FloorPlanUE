#include "Geometry/Bulge.h"
#include "TestHarness.h"
#include "Walls/WallPairDetector.h"

#include <algorithm>
#include <cmath>
#include <vector>

using FloorPlan::Geometry::Segment;
using FloorPlan::Geometry::Vec2;
using FloorPlan::Walls::PairingReport;
using FloorPlan::Walls::WallCandidate;
using FloorPlan::Walls::WallPairDetector;

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

    std::vector<double> Thicknesses(const std::vector<WallCandidate>& walls)
    {
        std::vector<double> values;
        for (const WallCandidate& wall : walls)
        {
            values.push_back(wall.ThicknessMm);
        }
        std::sort(values.begin(), values.end());
        return values;
    }
}

FLOORPLAN_TEST(WallPairDetector, UniformDoubleLineRoomYieldsFourWalls)
{
    std::vector<Segment> faces;
    AddRectangle(faces, 0.0, 0.0, 5000.0, 4000.0);
    AddRectangle(faces, -200.0, -200.0, 5200.0, 4200.0);

    std::vector<WallCandidate> walls;
    PairingReport report;
    WallPairDetector::Detect(faces, walls, report);

    CHECK_EQUAL(walls.size(), std::size_t{4});
    CHECK_EQUAL(report.Paired, std::size_t{4});
    for (const WallCandidate& wall : walls)
    {
        CHECK_NEAR(wall.ThicknessMm, 200.0, 0.001);
    }
}

FLOORPLAN_TEST(WallPairDetector, VaryingThicknessIsRecoveredPerWall)
{
    std::vector<Segment> faces;
    AddRectangle(faces, 0.0, 0.0, 5000.0, 4000.0);
    AddRectangle(faces, -300.0, -150.0, 5300.0, 4150.0);

    std::vector<WallCandidate> walls;
    PairingReport report;
    WallPairDetector::Detect(faces, walls, report);

    const std::vector<double> values = Thicknesses(walls);
    CHECK_EQUAL(values.size(), std::size_t{4});
    if (values.size() == 4)
    {
        CHECK_NEAR(values[0], 150.0, 0.001);
        CHECK_NEAR(values[1], 150.0, 0.001);
        CHECK_NEAR(values[2], 300.0, 0.001);
        CHECK_NEAR(values[3], 300.0, 0.001);
    }
}

FLOORPLAN_TEST(WallPairDetector, PerpendicularFacesAreRejectedByAngle)
{
    std::vector<Segment> faces;
    faces.push_back(Segment{Vec2{0.0, 0.0}, Vec2{5000.0, 0.0}, 0.0});
    faces.push_back(Segment{Vec2{0.0, 0.0}, Vec2{0.0, 4000.0}, 0.0});

    std::vector<WallCandidate> walls;
    PairingReport report;
    WallPairDetector::Detect(faces, walls, report);

    CHECK_EQUAL(walls.size(), std::size_t{0});
    CHECK_EQUAL(report.RejectedByAngle, std::size_t{1});
}

FLOORPLAN_TEST(WallPairDetector, FacesTooFarApartAreNotAWall)
{
    std::vector<Segment> faces;
    faces.push_back(Segment{Vec2{0.0, 0.0}, Vec2{5000.0, 0.0}, 0.0});
    faces.push_back(Segment{Vec2{0.0, 4000.0}, Vec2{5000.0, 4000.0}, 0.0});

    std::vector<WallCandidate> walls;
    PairingReport report;
    WallPairDetector::Detect(faces, walls, report);

    CHECK_EQUAL(walls.size(), std::size_t{0});
    CHECK_EQUAL(report.RejectedByGap, std::size_t{1});
}

FLOORPLAN_TEST(WallPairDetector, FacesTooCloseAreNotAWall)
{
    std::vector<Segment> faces;
    faces.push_back(Segment{Vec2{0.0, 0.0}, Vec2{5000.0, 0.0}, 0.0});
    faces.push_back(Segment{Vec2{0.0, 5.0}, Vec2{5000.0, 5.0}, 0.0});

    std::vector<WallCandidate> walls;
    PairingReport report;
    WallPairDetector::Detect(faces, walls, report);

    CHECK_EQUAL(walls.size(), std::size_t{0});
    CHECK_EQUAL(report.RejectedByGap, std::size_t{1});
}

FLOORPLAN_TEST(WallPairDetector, ParallelFacesThatDoNotOverlapAreRejected)
{
    std::vector<Segment> faces;
    faces.push_back(Segment{Vec2{0.0, 0.0}, Vec2{1000.0, 0.0}, 0.0});
    faces.push_back(Segment{Vec2{5000.0, 200.0}, Vec2{6000.0, 200.0}, 0.0});

    std::vector<WallCandidate> walls;
    PairingReport report;
    WallPairDetector::Detect(faces, walls, report);

    CHECK_EQUAL(walls.size(), std::size_t{0});
    CHECK_EQUAL(report.RejectedByOverlap, std::size_t{1});
}

FLOORPLAN_TEST(WallPairDetector, CentrelineSitsHalfwayBetweenTheFaces)
{
    std::vector<Segment> faces;
    faces.push_back(Segment{Vec2{0.0, 0.0}, Vec2{5000.0, 0.0}, 0.0});
    faces.push_back(Segment{Vec2{0.0, 200.0}, Vec2{5000.0, 200.0}, 0.0});

    std::vector<WallCandidate> walls;
    PairingReport report;
    WallPairDetector::Detect(faces, walls, report);

    CHECK_EQUAL(walls.size(), std::size_t{1});
    if (!walls.empty())
    {
        CHECK_NEAR(walls[0].ThicknessMm, 200.0, 0.001);
        CHECK_NEAR(walls[0].Start.Y, 100.0, 0.001);
        CHECK_NEAR(walls[0].End.Y, 100.0, 0.001);
        CHECK_NEAR(walls[0].Start.X, 0.0, 0.001);
        CHECK_NEAR(walls[0].End.X, 5000.0, 0.001);
    }
}

FLOORPLAN_TEST(WallPairDetector, PartialOverlapTrimsTheCentreline)
{
    std::vector<Segment> faces;
    faces.push_back(Segment{Vec2{0.0, 0.0}, Vec2{5000.0, 0.0}, 0.0});
    faces.push_back(Segment{Vec2{2000.0, 200.0}, Vec2{8000.0, 200.0}, 0.0});

    std::vector<WallCandidate> walls;
    PairingReport report;
    WallPairDetector::Detect(faces, walls, report);

    CHECK_EQUAL(walls.size(), std::size_t{1});
    if (!walls.empty())
    {
        CHECK_NEAR(walls[0].Start.X, 2000.0, 0.001);
        CHECK_NEAR(walls[0].End.X, 5000.0, 0.001);
    }
}

FLOORPLAN_TEST(WallPairDetector, NearlyParallelFacesWithinToleranceStillPair)
{
    std::vector<Segment> faces;
    faces.push_back(Segment{Vec2{0.0, 0.0}, Vec2{5000.0, 0.0}, 0.0});
    faces.push_back(Segment{Vec2{0.0, 200.0}, Vec2{5000.0, 200.5}, 0.0});

    std::vector<WallCandidate> walls;
    PairingReport report;
    WallPairDetector::Detect(faces, walls, report);

    CHECK_EQUAL(walls.size(), std::size_t{1});
    if (!walls.empty())
    {
        CHECK_NEAR(walls[0].ThicknessMm, 200.25, 0.5);
    }
}

FLOORPLAN_TEST(WallPairDetector, ConcentricArcFacesYieldACurvedWall)
{
    std::vector<Segment> faces;
    faces.push_back(Segment{Vec2{5000.0, 0.0}, Vec2{5000.0, 4000.0}, 1.0});
    faces.push_back(Segment{Vec2{5000.0, -200.0}, Vec2{5000.0, 4200.0}, 1.0});

    std::vector<WallCandidate> walls;
    PairingReport report;
    WallPairDetector::Detect(faces, walls, report);

    CHECK_EQUAL(walls.size(), std::size_t{1});
    CHECK_EQUAL(report.PairedCurved, std::size_t{1});
    if (walls.empty())
    {
        return;
    }
    CHECK(walls[0].IsCurved());
    CHECK_NEAR(walls[0].ThicknessMm, 200.0, 0.001);
    CHECK_NEAR(walls[0].Bulge, 1.0, 1.0e-9);
    CHECK_NEAR(walls[0].Start.X, 5000.0, 0.001);
    CHECK_NEAR(walls[0].Start.Y, -100.0, 0.001);
    CHECK_NEAR(walls[0].End.X, 5000.0, 0.001);
    CHECK_NEAR(walls[0].End.Y, 4100.0, 0.001);
}

FLOORPLAN_TEST(WallPairDetector, ConcentricArcCentrelineRunsMidwayBetweenTheRadii)
{
    std::vector<Segment> faces;
    faces.push_back(Segment{Vec2{5000.0, 0.0}, Vec2{5000.0, 4000.0}, 1.0});
    faces.push_back(Segment{Vec2{5000.0, -200.0}, Vec2{5000.0, 4200.0}, 1.0});

    std::vector<WallCandidate> walls;
    PairingReport report;
    WallPairDetector::Detect(faces, walls, report);

    CHECK_EQUAL(walls.size(), std::size_t{1});
    if (walls.empty())
    {
        return;
    }
    const FloorPlan::Geometry::BulgeArc arc =
        FloorPlan::Geometry::Bulge::Resolve(walls[0].Start, walls[0].End, walls[0].Bulge);
    CHECK_NEAR(arc.Radius, 2100.0, 0.001);
    CHECK_NEAR(arc.Center.X, 5000.0, 0.001);
    CHECK_NEAR(arc.Center.Y, 2000.0, 0.001);
}

FLOORPLAN_TEST(WallPairDetector, TheChordOfAnArcNeverPairsWithAStraightFace)
{
    std::vector<Segment> faces;
    faces.push_back(Segment{Vec2{0.0, 0.0}, Vec2{0.0, 4000.0}, 0.0});
    faces.push_back(Segment{Vec2{200.0, 0.0}, Vec2{200.0, 4000.0}, 0.2});

    std::vector<WallCandidate> walls;
    PairingReport report;
    WallPairDetector::Detect(faces, walls, report);

    CHECK_MESSAGE(walls.empty(), "a curved face and a flat face do not bound one wall");
    CHECK_EQUAL(report.Considered, std::size_t{0});
}

FLOORPLAN_TEST(WallPairDetector, ArcsAboutDifferentCentresAreNotAWall)
{
    std::vector<Segment> faces;
    faces.push_back(Segment{Vec2{5000.0, 0.0}, Vec2{5000.0, 4000.0}, 1.0});
    faces.push_back(Segment{Vec2{5000.0, 1000.0}, Vec2{5000.0, 4200.0}, 1.0});

    std::vector<WallCandidate> walls;
    PairingReport report;
    WallPairDetector::Detect(faces, walls, report);

    CHECK(walls.empty());
    CHECK_EQUAL(report.RejectedByAngle, std::size_t{1});
}

FLOORPLAN_TEST(WallPairDetector, ConcentricArcsThatShareNoAnglesAreRejected)
{
    std::vector<Segment> faces;
    faces.push_back(Segment{Vec2{2000.0, 0.0}, Vec2{-2000.0, 0.0}, 1.0});
    faces.push_back(Segment{Vec2{-2200.0, 0.0}, Vec2{2200.0, 0.0}, 1.0});

    std::vector<WallCandidate> walls;
    PairingReport report;
    WallPairDetector::Detect(faces, walls, report);

    CHECK_MESSAGE(walls.empty(), "opposite halves of one ring share no angular range");
    CHECK_EQUAL(report.RejectedByOverlap, std::size_t{1});
}
