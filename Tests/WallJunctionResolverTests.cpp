#include "Dxf/DxfParser.h"
#include "Dxf/DxfSource.h"
#include "Model/FloorPlanCompiler.h"
#include "TestHarness.h"
#include "Walls/WallJunctionResolver.h"

#include <algorithm>
#include <string>
#include <vector>

using FloorPlan::Diagnostic;
using FloorPlan::Dxf::DxfDocument;
using FloorPlan::Dxf::DxfParser;
using FloorPlan::Dxf::DxfSource;
using FloorPlan::Geometry::Vec2;
using FloorPlan::Model::BuildingModel;
using FloorPlan::Model::CompilerOptions;
using FloorPlan::Model::FloorPlanCompiler;
using FloorPlan::Testing::DataPath;
using FloorPlan::Walls::JunctionReport;
using FloorPlan::Walls::WallCandidate;
using FloorPlan::Walls::WallJunctionResolver;

namespace
{
    struct Compiled
    {
        BuildingModel Model;
        Diagnostic Failure;
        bool Ok = false;
    };

    Compiled Build(const std::string& relative, const CompilerOptions& options)
    {
        Compiled result;
        const DxfSource source = DxfSource::FromFile(DataPath(relative));
        if (!source.IsValid())
        {
            result.Failure = source.Failure();
            return result;
        }
        const auto reader = source.OpenReader();
        DxfDocument document;
        DxfParser parser(*reader);
        if (!parser.Parse(document))
        {
            result.Failure = parser.Failure();
            return result;
        }
        result.Ok = FloorPlanCompiler::Compile(document, options, result.Model, result.Failure);
        return result;
    }

    std::vector<double> SortedLengths(const BuildingModel& model)
    {
        std::vector<double> lengths;
        for (const auto& wall : model.Walls)
        {
            lengths.push_back((wall.End - wall.Start).Length());
        }
        std::sort(lengths.begin(), lengths.end());
        return lengths;
    }
}

FLOORPLAN_TEST(WallJunction, TheLongerWallOwnsTheCorner)
{
    std::vector<WallCandidate> walls;
    walls.push_back(WallCandidate{Vec2{0.0, -100.0}, Vec2{5000.0, -100.0}, 0.0, 200.0, 0, 0});
    walls.push_back(WallCandidate{Vec2{-100.0, 0.0}, Vec2{-100.0, 4000.0}, 0.0, 200.0, 0, 0});

    JunctionReport report;
    WallJunctionResolver::Close(walls, report);

    CHECK_NEAR(walls[0].Start.X, -200.0, 0.001);
    CHECK_NEAR(walls[0].Start.Y, -100.0, 0.001);
    CHECK_MESSAGE(FloorPlan::Testing::NearlyEqual(walls[1].Start.Y, 0.0, 0.001),
                  "the shorter wall must stop at the owner's face, not overlap it");
    CHECK_NEAR(walls[1].Start.X, -100.0, 0.001);
    CHECK_EQUAL(report.EndsExtended, std::size_t{1});
}

FLOORPLAN_TEST(WallJunction, CornerIsCoveredExactlyOnce)
{
    std::vector<WallCandidate> walls;
    walls.push_back(WallCandidate{Vec2{0.0, -100.0}, Vec2{5000.0, -100.0}, 0.0, 200.0, 0, 0});
    walls.push_back(WallCandidate{Vec2{-100.0, 0.0}, Vec2{-100.0, 4000.0}, 0.0, 200.0, 0, 0});

    JunctionReport report;
    WallJunctionResolver::Close(walls, report);

    const double ownerReach = walls[0].Start.X;
    const double otherStop = walls[1].Start.Y;
    CHECK_NEAR(ownerReach, -200.0, 0.001);
    CHECK_NEAR(otherStop, 0.0, 0.001);

    const double ownerFarFace = walls[0].Start.Y + walls[0].ThicknessMm * 0.5;
    CHECK_MESSAGE(FloorPlan::Testing::NearlyEqual(otherStop, ownerFarFace, 0.001),
                  "no gap and no overlap: the loser starts exactly at the owner's inner face");
}

FLOORPLAN_TEST(WallJunction, ParallelWallsAreNeverExtended)
{
    std::vector<WallCandidate> walls;
    walls.push_back(WallCandidate{Vec2{0.0, 0.0}, Vec2{5000.0, 0.0}, 0.0, 200.0, 0, 0});
    walls.push_back(WallCandidate{Vec2{0.0, 3000.0}, Vec2{5000.0, 3000.0}, 0.0, 200.0, 0, 0});

    JunctionReport report;
    WallJunctionResolver::Close(walls, report);

    CHECK_NEAR(walls[0].Start.X, 0.0, 0.001);
    CHECK_NEAR(walls[0].End.X, 5000.0, 0.001);
    CHECK_EQUAL(report.EndsExtended, std::size_t{0});
    CHECK_EQUAL(report.EndsLeftFree, std::size_t{4});
}

FLOORPLAN_TEST(WallJunction, DistantWallsDoNotPullEndsAcrossThePlan)
{
    std::vector<WallCandidate> walls;
    walls.push_back(WallCandidate{Vec2{0.0, 0.0}, Vec2{1000.0, 0.0}, 0.0, 200.0, 0, 0});
    walls.push_back(WallCandidate{Vec2{50000.0, -5000.0}, Vec2{50000.0, 5000.0}, 0.0, 200.0, 0, 0});

    JunctionReport report;
    WallJunctionResolver::Close(walls, report);

    CHECK_NEAR(walls[0].End.X, 1000.0, 0.001);
    CHECK_EQUAL(report.EndsExtended, std::size_t{0});
}

FLOORPLAN_TEST(WallJunction, SingleRoomCornersClose)
{
    const Compiled compiled = Build("Fixtures/single_room.dxf", CompilerOptions{});
    CHECK_MESSAGE(compiled.Ok, FloorPlan::Format(compiled.Failure));
    CHECK_EQUAL(compiled.Model.Walls.size(), std::size_t{4});
    CHECK_EQUAL(compiled.Model.ExtendedWallEnds, std::size_t{4});

    const std::vector<double> lengths = SortedLengths(compiled.Model);
    CHECK_EQUAL(lengths.size(), std::size_t{4});
    if (lengths.size() == 4)
    {
        CHECK_NEAR(lengths[0], 4000.0, 0.001);
        CHECK_NEAR(lengths[1], 4000.0, 0.001);
        CHECK_NEAR(lengths[2], 5400.0, 0.001);
        CHECK_NEAR(lengths[3], 5400.0, 0.001);
    }
}

FLOORPLAN_TEST(WallJunction, ClosedWallsSpanTheFullOuterExtent)
{
    const Compiled compiled = Build("Fixtures/single_room.dxf", CompilerOptions{});
    CHECK(compiled.Ok);

    double minX = 1e30;
    double maxX = -1e30;
    double minY = 1e30;
    double maxY = -1e30;
    for (const auto& wall : compiled.Model.Walls)
    {
        const Vec2 span = wall.End - wall.Start;
        const double length = span.Length();
        if (length <= 0.0)
        {
            continue;
        }
        const Vec2 across{-span.Y / length * wall.ThicknessMm * 0.5,
                          span.X / length * wall.ThicknessMm * 0.5};
        for (const Vec2& point : {wall.Start, wall.End})
        {
            for (const double side : {-1.0, 1.0})
            {
                const double x = point.X + across.X * side;
                const double y = point.Y + across.Y * side;
                minX = std::min(minX, x);
                maxX = std::max(maxX, x);
                minY = std::min(minY, y);
                maxY = std::max(maxY, y);
            }
        }
    }
    CHECK_NEAR(minX, -200.0, 0.001);
    CHECK_NEAR(maxX, 5200.0, 0.001);
    CHECK_NEAR(minY, -200.0, 0.001);
    CHECK_NEAR(maxY, 4200.0, 0.001);
}

FLOORPLAN_TEST(WallJunction, VaryingThicknessExtendsByTheNeighboursHalfThickness)
{
    const Compiled compiled = Build("Fixtures/varying_thickness.dxf", CompilerOptions{});
    CHECK(compiled.Ok);
    CHECK_EQUAL(compiled.Model.Walls.size(), std::size_t{4});

    const std::vector<double> lengths = SortedLengths(compiled.Model);
    if (lengths.size() == 4)
    {
        CHECK_NEAR(lengths[0], 4000.0, 0.001);
        CHECK_NEAR(lengths[1], 4000.0, 0.001);
        CHECK_NEAR(lengths[2], 5600.0, 0.001);
        CHECK_NEAR(lengths[3], 5600.0, 0.001);
    }
}

namespace
{
    struct Box
    {
        double MinX = 1e30;
        double MinY = 1e30;
        double MaxX = -1e30;
        double MaxY = -1e30;
    };

    std::vector<Box> Footprints(const BuildingModel& model)
    {
        std::vector<Box> boxes;
        for (const auto& wall : model.Walls)
        {
            const Vec2 span = wall.End - wall.Start;
            const double length = span.Length();
            if (length <= 0.0)
            {
                continue;
            }
            const Vec2 across{-span.Y / length * wall.ThicknessMm * 0.5,
                              span.X / length * wall.ThicknessMm * 0.5};
            Box box;
            for (const Vec2& point : {wall.Start, wall.End})
            {
                for (const double side : {-1.0, 1.0})
                {
                    const double x = point.X + across.X * side;
                    const double y = point.Y + across.Y * side;
                    box.MinX = std::min(box.MinX, x);
                    box.MinY = std::min(box.MinY, y);
                    box.MaxX = std::max(box.MaxX, x);
                    box.MaxY = std::max(box.MaxY, y);
                }
            }
            boxes.push_back(box);
        }
        return boxes;
    }

    double OverlapArea(const std::vector<Box>& boxes)
    {
        double total = 0.0;
        for (std::size_t left = 0; left < boxes.size(); ++left)
        {
            for (std::size_t right = left + 1; right < boxes.size(); ++right)
            {
                const double x = std::min(boxes[left].MaxX, boxes[right].MaxX) -
                                 std::max(boxes[left].MinX, boxes[right].MinX);
                const double y = std::min(boxes[left].MaxY, boxes[right].MaxY) -
                                 std::max(boxes[left].MinY, boxes[right].MinY);
                if (x > 0.001 && y > 0.001)
                {
                    total += x * y;
                }
            }
        }
        return total;
    }

    double SegmentArea(const BuildingModel& model)
    {
        double total = 0.0;
        for (const auto& wall : model.Walls)
        {
            total += (wall.End - wall.Start).Length() * wall.ThicknessMm;
        }
        return total;
    }
}

FLOORPLAN_TEST(WallJunction, SegmentsTileTheWallRingExactly)
{
    const char* fixtures[] = {"Fixtures/single_room.dxf", "Fixtures/varying_thickness.dxf",
                              "Fixtures/room_in_room.dxf", "Fixtures/l_shaped_room.dxf"};
    for (const char* relative : fixtures)
    {
        const Compiled compiled = Build(relative, CompilerOptions{});
        CHECK_MESSAGE(compiled.Ok, relative);
        if (!compiled.Ok)
        {
            continue;
        }
        const double segments = SegmentArea(compiled.Model);
        CHECK_MESSAGE(
            FloorPlan::Testing::NearlyEqual(segments, compiled.Model.WallFootprintMm2, 0.01),
            std::string(relative) + ": segments cover " +
                FloorPlan::Testing::Describe(segments) + " mm2 but the DXF ring is " +
                FloorPlan::Testing::Describe(compiled.Model.WallFootprintMm2));
    }
}

FLOORPLAN_TEST(WallJunction, NoFixtureProducesOverlappingWalls)
{
    const char* fixtures[] = {
        "Fixtures/single_room.dxf",       "Fixtures/varying_thickness.dxf",
        "Fixtures/room_in_room.dxf",      "Fixtures/l_shaped_room.dxf",
        "Fixtures/two_rooms_shared_wall.dxf", "Fixtures/door_and_window.dxf",
        "Fixtures/line_pair_room.dxf"};
    for (const char* relative : fixtures)
    {
        const Compiled compiled = Build(relative, CompilerOptions{});
        CHECK_MESSAGE(compiled.Ok, relative);
        if (!compiled.Ok)
        {
            continue;
        }
        const double overlap = OverlapArea(Footprints(compiled.Model));
        CHECK_MESSAGE(overlap <= 0.01,
                      std::string(relative) + ": walls overlap by " +
                          FloorPlan::Testing::Describe(overlap) + " mm2, which z-fights");
    }
}

FLOORPLAN_TEST(WallJunction, FreeStandingRunsAreLeftAlone)
{
    CompilerOptions options;
    options.WallLayers.push_back("A.Wall_Internal");

    const Compiled compiled = Build("RealWorld/wipeout_door.dxf", options);
    CHECK_MESSAGE(compiled.Ok, FloorPlan::Format(compiled.Failure));
    CHECK_EQUAL(compiled.Model.Walls.size(), std::size_t{2});
    CHECK_MESSAGE(compiled.Model.ExtendedWallEnds == 0,
                  "two parallel runs meet nothing and must keep their measured length");

    for (const auto& wall : compiled.Model.Walls)
    {
        CHECK_NEAR((wall.End - wall.Start).Length(), 3583.0139, 0.001);
    }
}

FLOORPLAN_TEST(WallJunction, GoldenInvariantsAreUnaffectedByClosing)
{
    const Compiled compiled = Build("Fixtures/room_in_room.dxf", CompilerOptions{});
    CHECK(compiled.Ok);

    std::vector<double> areas;
    for (const auto& room : compiled.Model.Rooms)
    {
        areas.push_back(room.AreaMm2);
    }
    std::sort(areas.begin(), areas.end());
    CHECK_EQUAL(areas.size(), std::size_t{2});
    if (areas.size() == 2)
    {
        CHECK_NEAR(areas[0], 1440000.0, 0.001);
        CHECK_NEAR(areas[1], 45750000.0, 0.001);
    }
    CHECK_NEAR(compiled.Model.WallFootprintMm2, 6570000.0, 0.001);
}
