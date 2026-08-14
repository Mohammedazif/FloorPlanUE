#include "Dxf/DxfParser.h"
#include "Dxf/DxfSource.h"
#include "Model/FloorPlanCompiler.h"
#include "Model/StairPlanner.h"
#include "Model/StoreyLink.h"
#include "TestHarness.h"

#include <string>
#include <vector>

using FloorPlan::Diagnostic;
using FloorPlan::Dxf::DxfDocument;
using FloorPlan::Dxf::DxfParser;
using FloorPlan::Dxf::DxfSource;
using FloorPlan::Geometry::Vec2;
using FloorPlan::Model::BuildingModel;
using FloorPlan::Model::CirculationKind;
using FloorPlan::Model::CirculationRegion;
using FloorPlan::Model::CompilerOptions;
using FloorPlan::Model::FloorPlanCompiler;
using FloorPlan::Model::StairFlight;
using FloorPlan::Model::StairPlanner;
using FloorPlan::Model::StoreyConnection;
using FloorPlan::Model::StoreyLink;
using FloorPlan::Testing::DataPath;

namespace
{
    constexpr double StoreyRiseMm = 3000.0;

    BuildingModel Plan(const std::string& relative, bool& ok)
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
        CompilerOptions options;
        options.WallLayers.push_back("A-WALL");
        ok = FloorPlanCompiler::Compile(document, options, model, failure);
        return model;
    }

    CirculationRegion StraightRun(double lengthMm, std::size_t drawnTreads)
    {
        CirculationRegion region;
        region.Kind = CirculationKind::Stair;
        region.Start = Vec2{0.0, 0.0};
        region.End = Vec2{0.0, lengthMm};
        region.WidthMm = 1200.0;
        region.DrawnTreads = drawnTreads;
        return region;
    }
}

FLOORPLAN_TEST(VerticalCirculation, ALabelledStairRoomIsFoundOnBothStoreys)
{
    bool ok = false;
    const BuildingModel ground = Plan("Fixtures/two_storey_ground.dxf", ok);
    CHECK(ok);
    const BuildingModel first = Plan("Fixtures/two_storey_first.dxf", ok);
    CHECK(ok);

    CHECK_EQUAL(ground.Circulation.size(), std::size_t{1});
    CHECK_EQUAL(first.Circulation.size(), std::size_t{1});
    if (ground.Circulation.empty())
    {
        return;
    }
    CHECK(ground.Circulation[0].Kind == CirculationKind::Stair);
    CHECK_EQUAL(ground.Circulation[0].DrawnTreads, std::size_t{17});
}

FLOORPLAN_TEST(VerticalCirculation, TheRunFollowsTheTreadsNotTheRoomOutline)
{
    bool ok = false;
    const BuildingModel ground = Plan("Fixtures/two_storey_ground.dxf", ok);
    CHECK(ok);
    if (ground.Circulation.empty())
    {
        return;
    }

    const CirculationRegion& stair = ground.Circulation[0];
    CHECK_NEAR(stair.Start.X, 6600.0, 0.001);
    CHECK_NEAR(stair.Start.Y, 0.0, 0.001);
    CHECK_NEAR(stair.End.X, 6600.0, 0.001);
    CHECK_NEAR(stair.End.Y, 4000.0, 0.001);
    CHECK_NEAR(stair.WidthMm, 2800.0, 0.001);
}

FLOORPLAN_TEST(VerticalCirculation, StackedStairsLinkTheTwoStoreys)
{
    bool ok = false;
    const BuildingModel ground = Plan("Fixtures/two_storey_ground.dxf", ok);
    CHECK(ok);
    const BuildingModel first = Plan("Fixtures/two_storey_first.dxf", ok);
    CHECK(ok);

    const std::vector<StoreyConnection> links = StoreyLink::Between(ground, first);
    CHECK_EQUAL(links.size(), std::size_t{1});
    if (links.empty())
    {
        return;
    }
    CHECK(links[0].Kind == CirculationKind::Stair);
    CHECK_NEAR(links[0].OverlapFraction, 1.0, 0.001);
}

FLOORPLAN_TEST(VerticalCirculation, APlanWithNoStairLinksToNothingAbove)
{
    bool ok = false;
    const BuildingModel ground = Plan("Fixtures/two_storey_ground.dxf", ok);
    CHECK(ok);
    const BuildingModel plain = Plan("Fixtures/single_room.dxf", ok);
    CHECK(ok);

    CHECK(plain.Circulation.empty());
    CHECK(StoreyLink::Between(ground, plain).empty());
}

FLOORPLAN_TEST(VerticalCirculation, DrawnTreadsSetTheStepCount)
{
    StairFlight flight;
    CHECK(StairPlanner::Plan(StraightRun(4000.0, 17), StoreyRiseMm, flight));

    CHECK_EQUAL(flight.StepCount, std::size_t{17});
    CHECK(flight.bFromDrawnTreads);
    CHECK_NEAR(flight.RiserHeightMm, StoreyRiseMm / 17.0, 1.0e-9);
    CHECK_NEAR(flight.TreadDepthMm, 4000.0 / 17.0, 1.0e-9);
}

FLOORPLAN_TEST(VerticalCirculation, TheStepsAlwaysAddUpToTheStoreyHeight)
{
    StairFlight flight;
    CHECK(StairPlanner::Plan(StraightRun(4000.0, 17), StoreyRiseMm, flight));

    const double climbed = flight.RiserHeightMm * static_cast<double>(flight.StepCount);
    CHECK_NEAR(climbed, StoreyRiseMm, 1.0e-9);
    CHECK_NEAR(flight.TreadDepthMm * static_cast<double>(flight.StepCount), 4000.0, 1.0e-9);
}

FLOORPLAN_TEST(VerticalCirculation, WithoutTreadsTheRunIsDividedByRiserHeight)
{
    StairFlight flight;
    CHECK(StairPlanner::Plan(StraightRun(4000.0, 0), StoreyRiseMm, flight));

    CHECK(!flight.bFromDrawnTreads);
    CHECK_EQUAL(flight.StepCount, std::size_t{18});
    CHECK_MESSAGE(flight.RiserHeightMm > 100.0 && flight.RiserHeightMm < 250.0,
                  "a derived riser must land in the range a person can climb");
}

FLOORPLAN_TEST(VerticalCirculation, HatchingIsNotMistakenForOneLinePerStep)
{
    StairFlight flight;
    CHECK(StairPlanner::Plan(StraightRun(4000.0, 120), StoreyRiseMm, flight));

    CHECK_MESSAGE(!flight.bFromDrawnTreads,
                  "120 lines over a 3 m rise is 25 mm a step, so it is not treads");
    CHECK_EQUAL(flight.StepCount, std::size_t{18});
}

FLOORPLAN_TEST(VerticalCirculation, ALiftIsNeverGivenAFlightOfSteps)
{
    CirculationRegion lift = StraightRun(2000.0, 0);
    lift.Kind = CirculationKind::Lift;

    StairFlight flight;
    CHECK(!StairPlanner::Plan(lift, StoreyRiseMm, flight));
}

FLOORPLAN_TEST(VerticalCirculation, StairsAreExcludedFromTheGoldenAreas)
{
    bool ok = false;
    const BuildingModel ground = Plan("Fixtures/two_storey_ground.dxf", ok);
    CHECK(ok);

    CHECK_EQUAL(ground.Rooms.size(), std::size_t{2});
    CHECK_NEAR(ground.TotalRoomAreaMm2(), 31200000.0, 0.001);
    CHECK_NEAR(ground.WallFootprintMm2, 5760000.0, 0.001);
}
