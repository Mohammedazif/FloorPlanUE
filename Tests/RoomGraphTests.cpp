#include "Dxf/DxfParser.h"
#include "Dxf/DxfSource.h"
#include "Model/FloorPlanCompiler.h"
#include "Model/RoomGraph.h"
#include "TestHarness.h"

#include <string>
#include <vector>

using FloorPlan::Diagnostic;
using FloorPlan::Dxf::DxfDocument;
using FloorPlan::Dxf::DxfParser;
using FloorPlan::Dxf::DxfSource;
using FloorPlan::Geometry::Loop;
using FloorPlan::Geometry::LoopVertex;
using FloorPlan::Geometry::Vec2;
using FloorPlan::Model::BuildingModel;
using FloorPlan::Model::CompilerOptions;
using FloorPlan::Model::FloorPlanCompiler;
using FloorPlan::Model::Opening;
using FloorPlan::Model::Room;
using FloorPlan::Model::RoomGraph;
using FloorPlan::Model::RoomLink;
using FloorPlan::Model::Wall;
using FloorPlan::Testing::DataPath;

namespace
{
    Loop Rectangle(double x0, double y0, double x1, double y1)
    {
        std::vector<LoopVertex> vertices;
        vertices.push_back(LoopVertex{Vec2{x0, y0}, 0.0});
        vertices.push_back(LoopVertex{Vec2{x1, y0}, 0.0});
        vertices.push_back(LoopVertex{Vec2{x1, y1}, 0.0});
        vertices.push_back(LoopVertex{Vec2{x0, y1}, 0.0});
        return Loop(std::move(vertices));
    }

    void AddRoom(BuildingModel& model, const std::string& id, const Loop& boundary)
    {
        model.Loops.push_back(boundary);
        Room room;
        room.Id = id;
        room.LoopIndex = model.Loops.size() - 1;
        room.AreaMm2 = boundary.AbsoluteArea();
        model.Rooms.push_back(std::move(room));
    }

    void AddWall(BuildingModel& model, const std::string& id, const Vec2& start, const Vec2& end)
    {
        Wall wall;
        wall.Id = id;
        wall.Start = start;
        wall.End = end;
        wall.ThicknessMm = 200.0;
        wall.HeightMm = 2700.0;
        model.Walls.push_back(std::move(wall));
    }

    /// Two rooms either side of one 200 mm partition, plus an exterior wall on the far left.
    BuildingModel TwoRoomModel()
    {
        BuildingModel model;
        AddRoom(model, "roomA", Rectangle(0.0, 0.0, 3000.0, 4000.0));
        AddRoom(model, "roomB", Rectangle(3200.0, 0.0, 6200.0, 4000.0));
        AddWall(model, "wallShared", Vec2{3100.0, 0.0}, Vec2{3100.0, 4000.0});
        AddWall(model, "wallOuter", Vec2{-100.0, 0.0}, Vec2{-100.0, 4000.0});
        return model;
    }

    BuildingModel Compile(const std::string& relative, bool& ok)
    {
        BuildingModel model;
        Diagnostic failure;
        ok = false;
        const DxfSource source = DxfSource::FromFile(DataPath(relative));
        if (!source.IsValid())
        {
            return model;
        }
        const auto reader = source.OpenReader();
        DxfDocument document;
        DxfParser parser(*reader);
        if (!parser.Parse(document))
        {
            return model;
        }
        ok = FloorPlanCompiler::Compile(document, CompilerOptions{}, model, failure);
        return model;
    }

    std::size_t InteriorLinkCount(const RoomGraph& graph)
    {
        std::size_t count = 0;
        for (const RoomLink& link : graph.Links())
        {
            if (!link.IsExterior())
            {
                ++count;
            }
        }
        return count;
    }
}

FLOORPLAN_TEST(RoomGraph, APartitionLinksTheRoomsEitherSideOfIt)
{
    const BuildingModel model = TwoRoomModel();
    const RoomGraph graph = RoomGraph::Build(model);

    CHECK_EQUAL(graph.Links().size(), std::size_t{2});
    CHECK_EQUAL(InteriorLinkCount(graph), std::size_t{1});
    CHECK_EQUAL(graph.ExteriorLinkCount(), std::size_t{1});
}

FLOORPLAN_TEST(RoomGraph, EveryWallOfOnePairCollapsesIntoASingleLink)
{
    BuildingModel model = TwoRoomModel();
    AddWall(model, "wallSharedUpper", Vec2{3100.0, 1000.0}, Vec2{3100.0, 3000.0});
    const RoomGraph graph = RoomGraph::Build(model);

    CHECK_EQUAL(InteriorLinkCount(graph), std::size_t{1});
    for (const RoomLink& link : graph.Links())
    {
        if (!link.IsExterior())
        {
            CHECK_EQUAL(link.WallIndices.size(), std::size_t{2});
        }
    }
}

FLOORPLAN_TEST(RoomGraph, APartitionWithoutAnOpeningIsNotTraversable)
{
    const BuildingModel model = TwoRoomModel();
    const RoomGraph graph = RoomGraph::Build(model);

    for (const RoomLink& link : graph.Links())
    {
        CHECK_MESSAGE(!link.IsTraversable(), "a solid partition must not connect two rooms");
    }
    CHECK(graph.Neighbours(0).empty());
}

FLOORPLAN_TEST(RoomGraph, ADoorInThePartitionConnectsTheRooms)
{
    BuildingModel model = TwoRoomModel();
    Opening door;
    door.Id = "door1";
    door.HostWallId = "wallShared";
    door.WidthMm = 900.0;
    door.Position = Vec2{3100.0, 2000.0};
    model.Openings.push_back(std::move(door));

    const RoomGraph graph = RoomGraph::Build(model);
    const std::vector<std::size_t> neighbours = graph.Neighbours(0);

    CHECK_EQUAL(neighbours.size(), std::size_t{1});
    if (!neighbours.empty())
    {
        CHECK_EQUAL(neighbours[0], std::size_t{1});
    }
    CHECK_EQUAL(graph.Neighbours(1).size(), std::size_t{1});
}

FLOORPLAN_TEST(RoomGraph, ADoorInAnExteriorWallDoesNotJoinTwoRooms)
{
    BuildingModel model = TwoRoomModel();
    Opening entrance;
    entrance.Id = "door2";
    entrance.HostWallId = "wallOuter";
    entrance.WidthMm = 900.0;
    entrance.Position = Vec2{-100.0, 2000.0};
    model.Openings.push_back(std::move(entrance));

    const RoomGraph graph = RoomGraph::Build(model);

    CHECK(graph.Neighbours(1).empty());
    CHECK_EQUAL(graph.Neighbours(0).size(), std::size_t{1});
    CHECK_MESSAGE(graph.Neighbours(0)[0] == RoomGraph::Outside,
                  "the only way out of room A is to the street");
}

FLOORPLAN_TEST(RoomGraph, SharedWallFixtureJoinsItsTwoRooms)
{
    bool ok = false;
    const BuildingModel model = Compile("Fixtures/two_rooms_shared_wall.dxf", ok);
    CHECK(ok);

    const RoomGraph graph = RoomGraph::Build(model);
    CHECK_EQUAL(model.Rooms.size(), std::size_t{2});
    CHECK_MESSAGE(InteriorLinkCount(graph) == 1,
                  "the two rooms of the fixture are separated by exactly one partition");
    CHECK_EQUAL(graph.ExteriorLinkCount(), std::size_t{2});
}

FLOORPLAN_TEST(RoomGraph, ASingleRoomOnlyEverTouchesTheOutside)
{
    bool ok = false;
    const BuildingModel model = Compile("Fixtures/single_room.dxf", ok);
    CHECK(ok);

    const RoomGraph graph = RoomGraph::Build(model);
    CHECK_EQUAL(graph.Links().size(), std::size_t{1});
    CHECK_EQUAL(InteriorLinkCount(graph), std::size_t{0});
    CHECK_EQUAL(graph.Links()[0].WallIndices.size(), model.Walls.size());
}

FLOORPLAN_TEST(RoomGraph, ANestedRoomIsBoundedByTheRoomThatContainsIt)
{
    bool ok = false;
    const BuildingModel model = Compile("Fixtures/room_in_room.dxf", ok);
    CHECK(ok);

    const RoomGraph graph = RoomGraph::Build(model);
    CHECK_MESSAGE(InteriorLinkCount(graph) >= 1,
                  "the inner room must be linked to the room it sits inside");
}
