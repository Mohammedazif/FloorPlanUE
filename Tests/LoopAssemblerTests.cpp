#include "Dxf/DxfParser.h"
#include "Dxf/DxfSource.h"
#include "Geometry/LoopAssembler.h"
#include "Model/FloorPlanCompiler.h"
#include "TestHarness.h"

#include <algorithm>
#include <string>
#include <vector>

using FloorPlan::Diagnostic;
using FloorPlan::Dxf::DxfDocument;
using FloorPlan::Dxf::DxfParser;
using FloorPlan::Dxf::DxfSource;
using FloorPlan::Geometry::AssemblyReport;
using FloorPlan::Geometry::Loop;
using FloorPlan::Geometry::LoopAssembler;
using FloorPlan::Geometry::Segment;
using FloorPlan::Geometry::Vec2;
using FloorPlan::Model::BuildingModel;
using FloorPlan::Model::CompilerOptions;
using FloorPlan::Model::FloorPlanCompiler;
using FloorPlan::Testing::DataPath;

namespace
{
    std::vector<Segment> Rectangle(double x0, double y0, double x1, double y1)
    {
        return {Segment{Vec2{x0, y0}, Vec2{x1, y0}, 0.0},
                Segment{Vec2{x1, y0}, Vec2{x1, y1}, 0.0},
                Segment{Vec2{x1, y1}, Vec2{x0, y1}, 0.0},
                Segment{Vec2{x0, y1}, Vec2{x0, y0}, 0.0}};
    }

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
}

FLOORPLAN_TEST(LoopAssembler, FourSegmentsFormOneClosedLoop)
{
    std::vector<Loop> loops;
    AssemblyReport report;
    Diagnostic diagnostic;
    CHECK(LoopAssembler::Assemble(Rectangle(0.0, 0.0, 5000.0, 4000.0), loops, report,
                                  diagnostic));
    CHECK_EQUAL(loops.size(), std::size_t{1});
    CHECK_EQUAL(report.ClosedLoops, std::size_t{1});
    if (!loops.empty())
    {
        CHECK_NEAR(loops[0].AbsoluteArea(), 20000000.0, 0.001);
    }
}

FLOORPLAN_TEST(LoopAssembler, SegmentOrderDoesNotMatter)
{
    std::vector<Segment> shuffled = Rectangle(0.0, 0.0, 5000.0, 4000.0);
    std::swap(shuffled[0], shuffled[2]);
    std::swap(shuffled[1], shuffled[3]);

    std::vector<Loop> loops;
    AssemblyReport report;
    Diagnostic diagnostic;
    CHECK(LoopAssembler::Assemble(shuffled, loops, report, diagnostic));
    CHECK_EQUAL(loops.size(), std::size_t{1});
    if (!loops.empty())
    {
        CHECK_NEAR(loops[0].AbsoluteArea(), 20000000.0, 0.001);
    }
}

FLOORPLAN_TEST(LoopAssembler, ReversedSegmentsStillChain)
{
    std::vector<Segment> mixed = Rectangle(0.0, 0.0, 5000.0, 4000.0);
    std::swap(mixed[1].Start, mixed[1].End);
    std::swap(mixed[3].Start, mixed[3].End);

    std::vector<Loop> loops;
    AssemblyReport report;
    Diagnostic diagnostic;
    CHECK(LoopAssembler::Assemble(mixed, loops, report, diagnostic));
    CHECK_EQUAL(loops.size(), std::size_t{1});
    if (!loops.empty())
    {
        CHECK_NEAR(loops[0].AbsoluteArea(), 20000000.0, 0.001);
    }
}

FLOORPLAN_TEST(LoopAssembler, OpenChainIsDiscardedNotGuessedClosed)
{
    std::vector<Segment> open = Rectangle(0.0, 0.0, 5000.0, 4000.0);
    open.pop_back();

    std::vector<Loop> loops;
    AssemblyReport report;
    Diagnostic diagnostic;
    CHECK(LoopAssembler::Assemble(open, loops, report, diagnostic));
    CHECK_EQUAL(loops.size(), std::size_t{0});
    CHECK(report.DiscardedSegments > 0);
}

FLOORPLAN_TEST(LoopAssembler, TwoDisjointRingsBecomeTwoLoops)
{
    std::vector<Segment> both = Rectangle(0.0, 0.0, 5000.0, 4000.0);
    const std::vector<Segment> outer = Rectangle(-200.0, -200.0, 5200.0, 4200.0);
    both.insert(both.end(), outer.begin(), outer.end());

    std::vector<Loop> loops;
    AssemblyReport report;
    Diagnostic diagnostic;
    CHECK(LoopAssembler::Assemble(both, loops, report, diagnostic));
    CHECK_EQUAL(loops.size(), std::size_t{2});
}

FLOORPLAN_TEST(LoopAssembler, ZeroLengthSegmentsAreDropped)
{
    std::vector<Segment> withStub = Rectangle(0.0, 0.0, 5000.0, 4000.0);
    withStub.push_back(Segment{Vec2{100.0, 100.0}, Vec2{100.0, 100.0}, 0.0});

    std::vector<Loop> loops;
    AssemblyReport report;
    Diagnostic diagnostic;
    CHECK(LoopAssembler::Assemble(withStub, loops, report, diagnostic));
    CHECK_EQUAL(loops.size(), std::size_t{1});
    CHECK(report.DiscardedSegments >= 1);
}

FLOORPLAN_TEST(LoopAssembler, LinePairRoomMatchesTheGoldenInvariants)
{
    const Compiled compiled = Build("Fixtures/line_pair_room.dxf", CompilerOptions{});
    CHECK_MESSAGE(compiled.Ok, FloorPlan::Format(compiled.Failure));
    CHECK_EQUAL(compiled.Model.AssembledLoops, std::size_t{2});
    CHECK_EQUAL(compiled.Model.Rooms.size(), std::size_t{1});
    if (!compiled.Model.Rooms.empty())
    {
        CHECK_NEAR(compiled.Model.Rooms[0].AreaMm2, 20000000.0, 0.001);
    }
    CHECK_NEAR(compiled.Model.WallFootprintMm2, 3760000.0, 0.001);
}

FLOORPLAN_TEST(LoopAssembler, LinePairWallsCarryMeasuredThickness)
{
    const Compiled compiled = Build("Fixtures/line_pair_room.dxf", CompilerOptions{});
    CHECK(compiled.Ok);
    CHECK_EQUAL(compiled.Model.Walls.size(), std::size_t{4});
    for (const auto& wall : compiled.Model.Walls)
    {
        CHECK_NEAR(wall.ThicknessMm, 200.0, 0.001);
    }
}

FLOORPLAN_TEST(LoopAssembler, LinePairAndPolylineFormsAgreeExactly)
{
    const Compiled fromLines = Build("Fixtures/line_pair_room.dxf", CompilerOptions{});
    const Compiled fromPolylines = Build("Fixtures/single_room.dxf", CompilerOptions{});
    CHECK(fromLines.Ok);
    CHECK(fromPolylines.Ok);
    CHECK_EQUAL(fromLines.Model.Rooms.size(), fromPolylines.Model.Rooms.size());
    CHECK_NEAR(fromLines.Model.WallFootprintMm2, fromPolylines.Model.WallFootprintMm2, 0.001);
    if (!fromLines.Model.Rooms.empty() && !fromPolylines.Model.Rooms.empty())
    {
        CHECK_NEAR(fromLines.Model.Rooms[0].AreaMm2, fromPolylines.Model.Rooms[0].AreaMm2,
                   0.001);
    }
}

FLOORPLAN_TEST(LoopAssembler, UnfilteredNoiseSilentlyCorruptsAreas)
{
    const Compiled compiled =
        Build("Fixtures/line_pair_room_with_noise.dxf", CompilerOptions{});
    CHECK(compiled.Ok);
    CHECK_EQUAL(compiled.Model.Rooms.size(), std::size_t{1});
    if (compiled.Model.Rooms.empty())
    {
        return;
    }

    const double furniture = 800.0 * 600.0;
    CHECK_MESSAGE(
        !FloorPlan::Testing::NearlyEqual(compiled.Model.Rooms[0].AreaMm2, 20000000.0, 0.001),
        "furniture nests one level deeper and is charged against the room");
    CHECK_NEAR(compiled.Model.Rooms[0].AreaMm2, 20000000.0 - furniture, 0.001);
    CHECK_NEAR(compiled.Model.WallFootprintMm2, 3760000.0 + furniture, 0.001);
}

FLOORPLAN_TEST(LoopAssembler, LayerFilterRemovesNoiseAndKeepsTheRoom)
{
    CompilerOptions options;
    options.WallLayers.push_back("A-WALL");

    const Compiled compiled = Build("Fixtures/line_pair_room_with_noise.dxf", options);
    CHECK_MESSAGE(compiled.Ok, FloorPlan::Format(compiled.Failure));
    CHECK_EQUAL(compiled.Model.Rooms.size(), std::size_t{1});
    if (!compiled.Model.Rooms.empty())
    {
        CHECK_NEAR(compiled.Model.Rooms[0].AreaMm2, 20000000.0, 0.001);
        CHECK_EQUAL(compiled.Model.Rooms[0].Name, std::string("Studio"));
    }
    CHECK_NEAR(compiled.Model.WallFootprintMm2, 3760000.0, 0.001);
}

FLOORPLAN_TEST(LoopAssembler, LayerFilterIsCaseInsensitive)
{
    CompilerOptions options;
    options.WallLayers.push_back("a-wall");

    const Compiled compiled = Build("Fixtures/line_pair_room_with_noise.dxf", options);
    CHECK(compiled.Ok);
    CHECK_EQUAL(compiled.Model.Rooms.size(), std::size_t{1});
}
