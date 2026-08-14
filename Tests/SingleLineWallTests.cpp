#include "Dxf/DxfParser.h"
#include "Dxf/DxfSource.h"
#include "Model/FloorPlanCompiler.h"
#include "TestHarness.h"

#include <algorithm>
#include <string>
#include <vector>

using FloorPlan::Diagnostic;
using FloorPlan::Dxf::DxfDocument;
using FloorPlan::Dxf::DxfParser;
using FloorPlan::Dxf::DxfSource;
using FloorPlan::Model::BuildingModel;
using FloorPlan::Model::CompilerOptions;
using FloorPlan::Model::FloorPlanCompiler;
using FloorPlan::Model::WallConvention;
using FloorPlan::Testing::DataPath;

namespace
{
    struct Compiled
    {
        BuildingModel Model;
        Diagnostic Failure;
        bool Ok = false;
    };

    Compiled Build(const std::string& relative, WallConvention convention)
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
        CompilerOptions options;
        options.Convention = convention;
        options.WallLayers.push_back("A-WALL");
        result.Ok = FloorPlanCompiler::Compile(document, options, result.Model, result.Failure);
        return result;
    }

    std::vector<double> SortedAreas(const BuildingModel& model)
    {
        std::vector<double> areas;
        for (const auto& room : model.Rooms)
        {
            areas.push_back(room.AreaMm2);
        }
        std::sort(areas.begin(), areas.end());
        return areas;
    }
}

FLOORPLAN_TEST(SingleLineWall, ArrangementSeparatesTheTwoRooms)
{
    const Compiled compiled =
        Build("Fixtures/single_line_two_rooms.dxf", WallConvention::SingleLine);
    CHECK_MESSAGE(compiled.Ok, FloorPlan::Format(compiled.Failure));

    const std::vector<double> areas = SortedAreas(compiled.Model);
    CHECK_MESSAGE(areas.size() == 2,
                  "expected two rooms, got " + std::to_string(areas.size()));
    if (areas.size() == 2)
    {
        CHECK_NEAR(areas[0], 15000000.0, 0.001);
        CHECK_NEAR(areas[1], 25000000.0, 0.001);
    }
    CHECK_EQUAL(compiled.Model.ArrangementFaces, std::size_t{2});
}

FLOORPLAN_TEST(SingleLineWall, ContainmentConventionCannotSeparateThem)
{
    const Compiled compiled =
        Build("Fixtures/single_line_two_rooms.dxf", WallConvention::DoubleLine);
    CHECK(compiled.Ok);
    CHECK_MESSAGE(compiled.Model.Rooms.size() != 2,
                  "double-line nesting must not accidentally succeed here");
}

FLOORPLAN_TEST(SingleLineWall, RoomLabelsAttachAcrossTheDivider)
{
    const Compiled compiled =
        Build("Fixtures/single_line_two_rooms.dxf", WallConvention::SingleLine);
    CHECK(compiled.Ok);

    std::string living;
    std::string kitchen;
    for (const auto& room : compiled.Model.Rooms)
    {
        if (FloorPlan::Testing::NearlyEqual(room.AreaMm2, 25000000.0, 0.001))
        {
            living = room.Name;
        }
        if (FloorPlan::Testing::NearlyEqual(room.AreaMm2, 15000000.0, 0.001))
        {
            kitchen = room.Name;
        }
    }
    CHECK_EQUAL(living, std::string("Living"));
    CHECK_EQUAL(kitchen, std::string("Kitchen"));
}

FLOORPLAN_TEST(SingleLineWall, DividerSplitsOuterEdgesIntoWallSegments)
{
    const Compiled compiled =
        Build("Fixtures/single_line_two_rooms.dxf", WallConvention::SingleLine);
    CHECK(compiled.Ok);
    CHECK_MESSAGE(compiled.Model.Walls.size() >= 6,
                  "the two T-junctions must split the top and bottom edges, got " +
                      std::to_string(compiled.Model.Walls.size()));
}

FLOORPLAN_TEST(SingleLineWall, IdentityRemainsStableAcrossReloads)
{
    const Compiled first =
        Build("Fixtures/single_line_two_rooms.dxf", WallConvention::SingleLine);
    const Compiled second =
        Build("Fixtures/single_line_two_rooms.dxf", WallConvention::SingleLine);
    CHECK(first.Ok);
    CHECK(second.Ok);
    CHECK_EQUAL(first.Model.Rooms.size(), second.Model.Rooms.size());
    for (std::size_t index = 0;
         index < first.Model.Rooms.size() && index < second.Model.Rooms.size(); ++index)
    {
        CHECK_EQUAL(first.Model.Rooms[index].Id, second.Model.Rooms[index].Id);
    }
}

FLOORPLAN_TEST(SingleLineWall, DoubleLineFixturesAreUnaffectedByTheNewPath)
{
    const Compiled compiled = Build("Fixtures/single_room.dxf", WallConvention::DoubleLine);
    CHECK(compiled.Ok);
    CHECK_EQUAL(compiled.Model.Rooms.size(), std::size_t{1});
    if (!compiled.Model.Rooms.empty())
    {
        CHECK_NEAR(compiled.Model.Rooms[0].AreaMm2, 20000000.0, 0.001);
    }
    CHECK_NEAR(compiled.Model.WallFootprintMm2, 3760000.0, 0.001);
}
