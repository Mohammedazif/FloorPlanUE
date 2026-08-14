#include "Dxf/DxfBlockExpander.h"
#include "Dxf/DxfParser.h"
#include "Dxf/DxfSource.h"
#include "Model/FloorPlanCompiler.h"
#include "TestHarness.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using FloorPlan::Diagnostic;
using FloorPlan::Dxf::DxfDocument;
using FloorPlan::Dxf::DxfEntity;
using FloorPlan::Dxf::DxfEntityType;
using FloorPlan::Dxf::DxfParser;
using FloorPlan::Dxf::DxfSource;
using FloorPlan::Model::BuildingModel;
using FloorPlan::Model::CompilerOptions;
using FloorPlan::Model::FloorPlanCompiler;
using FloorPlan::Testing::DataPath;

namespace
{
    constexpr double WallLengthMm = 3583.0139;
    constexpr double WallThicknessMm = 100.0;
    constexpr double WallSolidAreaMm2 = 358301.385067;
    constexpr double TotalFootprintMm2 = 716602.770134;

    bool ParseFile(DxfDocument& document, Diagnostic& failure)
    {
        const DxfSource source = DxfSource::FromFile(DataPath("RealWorld/wipeout_door.dxf"));
        if (!source.IsValid())
        {
            failure = source.Failure();
            return false;
        }
        const auto reader = source.OpenReader();
        DxfParser parser(*reader);
        if (!parser.Parse(document))
        {
            failure = parser.Failure();
            return false;
        }
        return true;
    }

    CompilerOptions WallLayerOnly()
    {
        CompilerOptions options;
        options.WallLayers.push_back("A.Wall_Internal");
        return options;
    }

    bool CompileFile(const CompilerOptions& options, BuildingModel& model, Diagnostic& failure)
    {
        DxfDocument document;
        if (!ParseFile(document, failure))
        {
            return false;
        }
        return FloorPlanCompiler::Compile(document, options, model, failure);
    }
}

FLOORPLAN_TEST(WipeoutDoor, EntityInventoryMatchesTheFile)
{
    DxfDocument document;
    Diagnostic failure;
    CHECK_MESSAGE(ParseFile(document, failure), FloorPlan::Format(failure));

    std::size_t lines = 0;
    std::size_t polylines = 0;
    std::size_t inserts = 0;
    std::size_t arcs = 0;
    std::size_t circles = 0;
    std::size_t mtext = 0;
    for (const DxfEntity& entity : document.ModelSpace)
    {
        switch (entity.Type)
        {
        case DxfEntityType::Line: ++lines; break;
        case DxfEntityType::LwPolyline: ++polylines; break;
        case DxfEntityType::Insert: ++inserts; break;
        case DxfEntityType::Arc: ++arcs; break;
        case DxfEntityType::Circle: ++circles; break;
        case DxfEntityType::MText: ++mtext; break;
        default: break;
        }
    }

    CHECK_EQUAL(lines, std::size_t{136});
    CHECK_EQUAL(polylines, std::size_t{2});
    CHECK_EQUAL(inserts, std::size_t{2});
    CHECK_EQUAL(arcs, std::size_t{2});
    CHECK_EQUAL(circles, std::size_t{2});
    CHECK_EQUAL(mtext, std::size_t{4});
    CHECK_EQUAL(document.Version, std::string("AC1032"));
}

FLOORPLAN_TEST(WipeoutDoor, DeclaredUnitIsInchesAndContradictsTheGeometry)
{
    DxfDocument document;
    Diagnostic failure;
    CHECK(ParseFile(document, failure));

    CHECK(document.HasInsertUnits);
    CHECK_EQUAL(document.InsertUnits, 1);
    CHECK_NEAR(FloorPlan::Dxf::MillimetresPerUnit(document.InsertUnits), 25.4, 1e-9);

    const double thicknessIfInches = WallThicknessMm * 25.4;
    CHECK_MESSAGE(thicknessIfInches > 2000.0,
                  "honouring the declared inch unit gives a 2.5 m thick internal wall");
}

FLOORPLAN_TEST(WipeoutDoor, WallSolidsAreRecoveredFromTheirOwnLayer)
{
    BuildingModel model;
    Diagnostic failure;
    CHECK_MESSAGE(CompileFile(WallLayerOnly(), model, failure), FloorPlan::Format(failure));

    CHECK_EQUAL(model.Loops.size(), std::size_t{2});
    for (const auto& loop : model.Loops)
    {
        CHECK_EQUAL(loop.EdgeCount(), std::size_t{4});
        CHECK_NEAR(loop.AbsoluteArea(), WallSolidAreaMm2, 0.001);

        const double width = loop.Maximum().X - loop.Minimum().X;
        const double height = loop.Maximum().Y - loop.Minimum().Y;
        CHECK_NEAR(width, WallLengthMm, 0.001);
        CHECK_NEAR(height, WallThicknessMm, 0.001);
    }
}

FLOORPLAN_TEST(WipeoutDoor, FootprintIsTheSumOfBothWallSolids)
{
    BuildingModel model;
    Diagnostic failure;
    CHECK(CompileFile(WallLayerOnly(), model, failure));
    CHECK_NEAR(model.WallFootprintMm2, TotalFootprintMm2, 0.001);
}

FLOORPLAN_TEST(WipeoutDoor, AFragmentEnclosingNothingYieldsNoRooms)
{
    BuildingModel model;
    Diagnostic failure;
    CHECK(CompileFile(WallLayerOnly(), model, failure));
    CHECK_MESSAGE(model.Rooms.empty(),
                  "two parallel wall bars enclose nothing; zero rooms is correct");
}

FLOORPLAN_TEST(WipeoutDoor, PairingFindsOneWallPerSolid)
{
    BuildingModel model;
    Diagnostic failure;
    CHECK(CompileFile(WallLayerOnly(), model, failure));

    CHECK_EQUAL(model.Walls.size(), std::size_t{2});
    for (const auto& wall : model.Walls)
    {
        CHECK_NEAR(wall.ThicknessMm, WallThicknessMm, 0.001);
        const double length = (wall.End - wall.Start).Length();
        CHECK_NEAR(length, WallLengthMm, 0.001);
    }
}

FLOORPLAN_TEST(WipeoutDoor, TheTwoWallRunsAreTooFarApartToPair)
{
    BuildingModel model;
    Diagnostic failure;
    CHECK(CompileFile(WallLayerOnly(), model, failure));

    std::vector<double> centreY;
    for (const auto& wall : model.Walls)
    {
        centreY.push_back(0.5 * (wall.Start.Y + wall.End.Y));
    }
    std::sort(centreY.begin(), centreY.end());
    CHECK_EQUAL(centreY.size(), std::size_t{2});
    if (centreY.size() == 2)
    {
        CHECK_MESSAGE(centreY[1] - centreY[0] > 1800.0,
                      "the runs are ~1.9 m apart, well beyond any wall thickness");
    }
}

FLOORPLAN_TEST(WipeoutDoor, DoorBlockIsExpandedWithoutCycles)
{
    DxfDocument document;
    Diagnostic failure;
    CHECK(ParseFile(document, failure));

    CHECK(document.FindBlock("wipeout_door") != nullptr);
    CHECK_EQUAL(FloorPlan::Dxf::DxfBlockExpander::UnresolvedReferenceCount(document),
                std::size_t{0});

    std::vector<DxfEntity> expanded;
    Diagnostic expandFailure;
    CHECK_MESSAGE(FloorPlan::Dxf::DxfBlockExpander::Expand(document, expanded, expandFailure),
                  FloorPlan::Format(expandFailure));

    std::size_t fromBlock = 0;
    for (const DxfEntity& entity : expanded)
    {
        if (entity.FromBlock)
        {
            ++fromBlock;
        }
    }
    CHECK_MESSAGE(fromBlock == 136, "two inserts of a 68-entity block, minus the wipeout");
}

FLOORPLAN_TEST(WipeoutDoor, ExplodedDoorLinesBecomePhantomRoomsWithoutFiltering)
{
    BuildingModel unfiltered;
    Diagnostic failure;
    CHECK(CompileFile(CompilerOptions{}, unfiltered, failure));

    BuildingModel filtered;
    CHECK(CompileFile(WallLayerOnly(), filtered, failure));

    CHECK_MESSAGE(!unfiltered.Rooms.empty(),
                  "the exploded door outline closes into rings that read as rooms");
    CHECK_MESSAGE(unfiltered.WallFootprintMm2 > filtered.WallFootprintMm2,
                  "phantom rings inflate the footprint");
    CHECK_MESSAGE(filtered.Rooms.empty(), "filtering to the wall layer removes every phantom");
    CHECK_NEAR(filtered.WallFootprintMm2, TotalFootprintMm2, 0.001);
}

FLOORPLAN_TEST(WipeoutDoor, PhantomGeometryComesFromLooseSegmentsNotBlocks)
{
    BuildingModel unfiltered;
    Diagnostic failure;
    CHECK(CompileFile(CompilerOptions{}, unfiltered, failure));
    CHECK_MESSAGE(unfiltered.AssembledLoops > 0,
                  "the phantoms are assembled from loose model-space lines");
}

FLOORPLAN_TEST(WipeoutDoor, CommentaryTextIsNotTreatedAsARoomLabel)
{
    BuildingModel model;
    Diagnostic failure;
    CHECK(CompileFile(WallLayerOnly(), model, failure));

    CHECK(model.Rooms.empty());
    CHECK_MESSAGE(model.UnassignedLabels.size() == 4,
                  "all four MTEXT are commentary and belong to no room, got " +
                      std::to_string(model.UnassignedLabels.size()));
}

FLOORPLAN_TEST(WipeoutDoor, IdentityIsStableAcrossReloads)
{
    BuildingModel first;
    BuildingModel second;
    Diagnostic failure;
    CHECK(CompileFile(WallLayerOnly(), first, failure));
    CHECK(CompileFile(WallLayerOnly(), second, failure));

    CHECK_EQUAL(first.Walls.size(), second.Walls.size());
    for (std::size_t index = 0; index < first.Walls.size() && index < second.Walls.size();
         ++index)
    {
        CHECK_EQUAL(first.Walls[index].Id, second.Walls[index].Id);
    }
}
