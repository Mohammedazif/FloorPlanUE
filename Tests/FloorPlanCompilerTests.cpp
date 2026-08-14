#include "Dxf/DxfParser.h"
#include "Dxf/DxfSource.h"
#include "Geometry/Bulge.h"
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
using FloorPlan::Model::OpeningKind;
using FloorPlan::Testing::DataPath;

namespace
{
    struct Compiled
    {
        BuildingModel Model;
        Diagnostic Failure;
        bool Ok = false;
    };

    Compiled Build(const std::string& relative)
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
        result.Ok = FloorPlanCompiler::Compile(document, CompilerOptions{}, result.Model,
                                               result.Failure);
        return result;
    }

    std::vector<double> SortedRoomAreas(const BuildingModel& model)
    {
        std::vector<double> areas;
        areas.reserve(model.Rooms.size());
        for (const auto& room : model.Rooms)
        {
            areas.push_back(room.AreaMm2);
        }
        std::sort(areas.begin(), areas.end());
        return areas;
    }

    void CheckFixture(const char* relative, const std::vector<double>& expectedAreas,
                      double expectedFootprint)
    {
        const Compiled compiled = Build(relative);
        CHECK_MESSAGE(compiled.Ok,
                      std::string(relative) + ": " + FloorPlan::Format(compiled.Failure));
        if (!compiled.Ok)
        {
            return;
        }

        const std::vector<double> actual = SortedRoomAreas(compiled.Model);
        CHECK_MESSAGE(actual.size() == expectedAreas.size(),
                      std::string(relative) + " room count " +
                          std::to_string(actual.size()));
        for (std::size_t index = 0; index < expectedAreas.size() && index < actual.size();
             ++index)
        {
            CHECK_MESSAGE(FloorPlan::Testing::NearlyEqual(actual[index], expectedAreas[index],
                                                          0.001),
                          std::string(relative) + " room area " +
                              FloorPlan::Testing::Describe(actual[index]) + " expected " +
                              FloorPlan::Testing::Describe(expectedAreas[index]));
        }
        CHECK_MESSAGE(FloorPlan::Testing::NearlyEqual(compiled.Model.WallFootprintMm2,
                                                      expectedFootprint, 0.001),
                      std::string(relative) + " footprint " +
                          FloorPlan::Testing::Describe(compiled.Model.WallFootprintMm2) +
                          " expected " + FloorPlan::Testing::Describe(expectedFootprint));
    }
}

FLOORPLAN_TEST(Compiler, SingleRoomMatchesBothGoldenInvariants)
{
    CheckFixture("Fixtures/single_room.dxf", {20000000.0}, 3760000.0);
}

FLOORPLAN_TEST(Compiler, TwoRoomsSharedWallMatchesBothGoldenInvariants)
{
    CheckFixture("Fixtures/two_rooms_shared_wall.dxf", {9000000.0, 12000000.0}, 4670000.0);
}

FLOORPLAN_TEST(Compiler, LShapedRoomMatchesBothGoldenInvariants)
{
    CheckFixture("Fixtures/l_shaped_room.dxf", {26000000.0}, 4560000.0);
}

FLOORPLAN_TEST(Compiler, RoomInRoomMatchesBothGoldenInvariants)
{
    CheckFixture("Fixtures/room_in_room.dxf", {1440000.0, 45750000.0}, 6570000.0);
}

FLOORPLAN_TEST(Compiler, DoorAndWindowMatchesBothGoldenInvariants)
{
    CheckFixture("Fixtures/door_and_window.dxf", {20000000.0}, 3760000.0);
}

FLOORPLAN_TEST(Compiler, LabeledRoomsMatchesBothGoldenInvariants)
{
    CheckFixture("Fixtures/labeled_rooms.dxf", {9000000.0, 12000000.0}, 4670000.0);
}

FLOORPLAN_TEST(Compiler, VaryingThicknessMatchesBothGoldenInvariants)
{
    CheckFixture("Fixtures/varying_thickness.dxf", {20000000.0}, 4080000.0);
}

FLOORPLAN_TEST(Compiler, ArcWallMatchesBothGoldenInvariants)
{
    CheckFixture("Fixtures/arc_wall.dxf", {26283185.307179585}, 4199468.914507713);
}

FLOORPLAN_TEST(Compiler, ArcWallYieldsFourWallsOneOfThemCurved)
{
    const Compiled compiled = Build("Fixtures/arc_wall.dxf");
    CHECK(compiled.Ok);

    CHECK_EQUAL(compiled.Model.Walls.size(), std::size_t{4});

    std::size_t curved = 0;
    for (const auto& wall : compiled.Model.Walls)
    {
        if (!FloorPlan::Geometry::Bulge::IsStraight(wall.Bulge))
        {
            ++curved;
        }
    }
    CHECK_MESSAGE(curved == 1, "the semicircular wall must survive as a curved wall, not vanish");
}

FLOORPLAN_TEST(Compiler, TheCurvedWallFollowsTheRingItWasDrawnAs)
{
    const Compiled compiled = Build("Fixtures/arc_wall.dxf");
    CHECK(compiled.Ok);

    for (const auto& wall : compiled.Model.Walls)
    {
        if (FloorPlan::Geometry::Bulge::IsStraight(wall.Bulge))
        {
            continue;
        }
        const FloorPlan::Geometry::BulgeArc arc =
            FloorPlan::Geometry::Bulge::Resolve(wall.Start, wall.End, wall.Bulge);
        CHECK_NEAR(arc.Center.X, 5000.0, 0.001);
        CHECK_NEAR(arc.Center.Y, 2000.0, 0.001);
        CHECK_NEAR(arc.Radius, 2100.0, 0.001);
        CHECK_NEAR(wall.ThicknessMm, 200.0, 0.001);
    }
}

FLOORPLAN_TEST(Compiler, StraightWallsMeetTheCurvedWallWithoutAGap)
{
    const Compiled compiled = Build("Fixtures/arc_wall.dxf");
    CHECK(compiled.Ok);

    for (const auto& wall : compiled.Model.Walls)
    {
        if (FloorPlan::Geometry::Bulge::IsStraight(wall.Bulge))
        {
            continue;
        }
        double nearestToStart = 1.0e9;
        double nearestToEnd = 1.0e9;
        for (const auto& other : compiled.Model.Walls)
        {
            if (other.Id == wall.Id)
            {
                continue;
            }
            for (const auto& end : {other.Start, other.End})
            {
                nearestToStart = std::min(nearestToStart, (end - wall.Start).Length());
                nearestToEnd = std::min(nearestToEnd, (end - wall.End).Length());
            }
        }
        CHECK_NEAR(nearestToStart, 0.0, 0.001);
        CHECK_NEAR(nearestToEnd, 0.0, 0.001);
    }
}

FLOORPLAN_TEST(Compiler, ReleaseTwelveMatchesBothGoldenInvariants)
{
    CheckFixture("Fixtures/single_room_r12.dxf", {20000000.0}, 3760000.0);
}

FLOORPLAN_TEST(Compiler, BinaryMatchesBothGoldenInvariants)
{
    CheckFixture("Fixtures/single_room_binary.dxf", {20000000.0}, 3760000.0);
}

FLOORPLAN_TEST(Compiler, RoomLabelsAttachToTheirEnclosingRoom)
{
    const Compiled compiled = Build("Fixtures/labeled_rooms.dxf");
    CHECK(compiled.Ok);

    std::string bedroom;
    std::string bathroom;
    for (const auto& room : compiled.Model.Rooms)
    {
        if (FloorPlan::Testing::NearlyEqual(room.AreaMm2, 12000000.0, 0.001))
        {
            bedroom = room.Name;
        }
        if (FloorPlan::Testing::NearlyEqual(room.AreaMm2, 9000000.0, 0.001))
        {
            bathroom = room.Name;
        }
    }
    CHECK_EQUAL(bedroom, std::string("Bedroom 1"));
    CHECK_EQUAL(bathroom, std::string("Bathroom"));
}

FLOORPLAN_TEST(Compiler, LabelOutsideEveryRoomStaysUnassigned)
{
    const Compiled compiled = Build("Fixtures/labeled_rooms.dxf");
    CHECK(compiled.Ok);
    CHECK_EQUAL(compiled.Model.UnassignedLabels.size(), std::size_t{1});
    if (!compiled.Model.UnassignedLabels.empty())
    {
        CHECK_EQUAL(compiled.Model.UnassignedLabels[0], std::string("NORTH ELEVATION"));
    }
}

FLOORPLAN_TEST(Compiler, VaryingThicknessProducesBothWallThicknesses)
{
    const Compiled compiled = Build("Fixtures/varying_thickness.dxf");
    CHECK(compiled.Ok);
    CHECK_EQUAL(compiled.Model.Walls.size(), std::size_t{4});

    std::vector<double> thicknesses;
    for (const auto& wall : compiled.Model.Walls)
    {
        thicknesses.push_back(wall.ThicknessMm);
    }
    std::sort(thicknesses.begin(), thicknesses.end());
    CHECK_EQUAL(thicknesses.size(), std::size_t{4});
    if (thicknesses.size() == 4)
    {
        CHECK_NEAR(thicknesses[0], 150.0, 0.001);
        CHECK_NEAR(thicknesses[1], 150.0, 0.001);
        CHECK_NEAR(thicknesses[2], 300.0, 0.001);
        CHECK_NEAR(thicknesses[3], 300.0, 0.001);
    }
}

FLOORPLAN_TEST(Compiler, UniformWallThicknessIsMeasuredAsTwoHundred)
{
    const Compiled compiled = Build("Fixtures/single_room.dxf");
    CHECK(compiled.Ok);
    CHECK_EQUAL(compiled.Model.Walls.size(), std::size_t{4});
    for (const auto& wall : compiled.Model.Walls)
    {
        CHECK_NEAR(wall.ThicknessMm, 200.0, 0.001);
    }
}

FLOORPLAN_TEST(Compiler, OpeningsCarryKindWidthAndHostWall)
{
    const Compiled compiled = Build("Fixtures/door_and_window.dxf");
    CHECK(compiled.Ok);
    CHECK_EQUAL(compiled.Model.Openings.size(), std::size_t{2});

    bool sawDoor = false;
    bool sawWindow = false;
    for (const auto& opening : compiled.Model.Openings)
    {
        CHECK(!opening.HostWallId.empty());
        if (opening.Kind == OpeningKind::Door)
        {
            sawDoor = true;
            CHECK_EQUAL(opening.BlockName, std::string("DOOR_900"));
            CHECK_NEAR(opening.WidthMm, 900.0, 1e-9);
            CHECK_NEAR(opening.SillHeightMm, 0.0, 1e-9);
        }
        else
        {
            sawWindow = true;
            CHECK_EQUAL(opening.BlockName, std::string("WIN_1200"));
            CHECK_NEAR(opening.WidthMm, 1200.0, 1e-9);
            CHECK(opening.SillHeightMm > 0.0);
        }
    }
    CHECK(sawDoor);
    CHECK(sawWindow);
}

FLOORPLAN_TEST(Compiler, IdentityIsStableAcrossReloads)
{
    const Compiled first = Build("Fixtures/room_in_room.dxf");
    const Compiled second = Build("Fixtures/room_in_room.dxf");
    CHECK(first.Ok);
    CHECK(second.Ok);
    CHECK_EQUAL(first.Model.Rooms.size(), second.Model.Rooms.size());
    for (std::size_t index = 0;
         index < first.Model.Rooms.size() && index < second.Model.Rooms.size(); ++index)
    {
        CHECK_EQUAL(first.Model.Rooms[index].Id, second.Model.Rooms[index].Id);
    }
    CHECK_EQUAL(first.Model.Walls.size(), second.Model.Walls.size());
    for (std::size_t index = 0;
         index < first.Model.Walls.size() && index < second.Model.Walls.size(); ++index)
    {
        CHECK_EQUAL(first.Model.Walls[index].Id, second.Model.Walls[index].Id);
    }
}

FLOORPLAN_TEST(Compiler, IdentityIsIndependentOfFileEncoding)
{
    const Compiled ascii = Build("Fixtures/single_room.dxf");
    const Compiled binary = Build("Fixtures/single_room_binary.dxf");
    CHECK(ascii.Ok);
    CHECK(binary.Ok);
    CHECK_EQUAL(ascii.Model.Rooms.size(), binary.Model.Rooms.size());
    for (std::size_t index = 0;
         index < ascii.Model.Rooms.size() && index < binary.Model.Rooms.size(); ++index)
    {
        CHECK_EQUAL(ascii.Model.Rooms[index].Id, binary.Model.Rooms[index].Id);
    }
    for (std::size_t index = 0;
         index < ascii.Model.Walls.size() && index < binary.Model.Walls.size(); ++index)
    {
        CHECK_EQUAL(ascii.Model.Walls[index].Id, binary.Model.Walls[index].Id);
    }
}

FLOORPLAN_TEST(Compiler, IdentityIsIndependentOfDxfVersion)
{
    const Compiled modern = Build("Fixtures/single_room.dxf");
    const Compiled legacy = Build("Fixtures/single_room_r12.dxf");
    CHECK(modern.Ok);
    CHECK(legacy.Ok);
    CHECK_EQUAL(modern.Model.Rooms.size(), legacy.Model.Rooms.size());
    for (std::size_t index = 0;
         index < modern.Model.Rooms.size() && index < legacy.Model.Rooms.size(); ++index)
    {
        CHECK_EQUAL(modern.Model.Rooms[index].Id, legacy.Model.Rooms[index].Id);
    }
}

FLOORPLAN_TEST(Compiler, IdentifiersAreDistinctBetweenElements)
{
    const Compiled compiled = Build("Fixtures/room_in_room.dxf");
    CHECK(compiled.Ok);

    std::vector<std::string> ids;
    for (const auto& room : compiled.Model.Rooms)
    {
        ids.push_back(room.Id);
    }
    for (const auto& wall : compiled.Model.Walls)
    {
        ids.push_back(wall.Id);
    }
    const std::size_t total = ids.size();
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    CHECK_EQUAL(ids.size(), total);
}

FLOORPLAN_TEST(Compiler, ReleaseTwelveFallsBackToTheSuppliedUnit)
{
    const Compiled modern = Build("Fixtures/single_room.dxf");
    const Compiled legacy = Build("Fixtures/single_room_r12.dxf");
    CHECK(modern.Ok);
    CHECK(legacy.Ok);
    CHECK(modern.Model.UnitsWereDeclared);
    CHECK(!legacy.Model.UnitsWereDeclared);
    CHECK_NEAR(modern.Model.MillimetresPerUnit, 1.0, 1e-12);
    CHECK_NEAR(legacy.Model.MillimetresPerUnit, 1.0, 1e-12);
}
