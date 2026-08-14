#include "Dxf/DxfParser.h"
#include "Dxf/DxfSource.h"
#include "Model/FloorPlanCompiler.h"
#include "TestHarness.h"

#include <string>
#include <vector>

using FloorPlan::Diagnostic;
using FloorPlan::Dxf::DxfDocument;
using FloorPlan::Dxf::DxfParser;
using FloorPlan::Dxf::DxfSource;
using FloorPlan::Model::BuildingModel;
using FloorPlan::Model::CompilerOptions;
using FloorPlan::Model::Dimension;
using FloorPlan::Model::DimensionKind;
using FloorPlan::Model::FloorPlanCompiler;
using FloorPlan::Testing::DataPath;

namespace
{
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

    bool HasMeasurement(const BuildingModel& model, double expected)
    {
        for (const Dimension& dimension : model.Dimensions)
        {
            if (FloorPlan::Testing::NearlyEqual(dimension.MeasurementMm, expected, 0.001))
            {
                return true;
            }
        }
        return false;
    }
}

FLOORPLAN_TEST(Annotation, DimensionsCarryTheValueTheDrawingAsserts)
{
    bool ok = false;
    const BuildingModel model = Plan("Fixtures/annotated_room.dxf", ok);
    CHECK(ok);

    CHECK_EQUAL(model.Dimensions.size(), std::size_t{2});
    CHECK_MESSAGE(HasMeasurement(model, 5000.0), "the 5000 mm run must be read");
    CHECK_MESSAGE(HasMeasurement(model, 3800.0), "the stated 3800 must be read as written");
}

FLOORPLAN_TEST(Annotation, AStatedMeasurementIsCheckedAgainstTheGeometryNotBelieved)
{
    bool ok = false;
    const BuildingModel model = Plan("Fixtures/annotated_room.dxf", ok);
    CHECK(ok);

    std::size_t agreeing = 0;
    std::size_t disagreeing = 0;
    for (const Dimension& dimension : model.Dimensions)
    {
        CHECK(dimension.MeasurementWasStated);
        CHECK(dimension.Kind == DimensionKind::Linear ||
              dimension.Kind == DimensionKind::Aligned);
        if (dimension.AgreesWithGeometry)
        {
            ++agreeing;
        }
        else
        {
            ++disagreeing;
            CHECK_NEAR(dimension.MeasurementMm, 3800.0, 0.001);
            CHECK_NEAR(dimension.GeometryMm, 4000.0, 0.001);
        }
    }
    CHECK_EQUAL(agreeing, std::size_t{1});
    CHECK_MESSAGE(disagreeing == 1,
                  "a dimension contradicting the geometry it spans must be reported, not trusted");
}

FLOORPLAN_TEST(Annotation, AnAbsentMeasurementFallsBackToTheExtensionLines)
{
    bool ok = false;
    const BuildingModel model = Plan("Fixtures/two_storey_ground.dxf", ok);
    CHECK(ok);

    for (const Dimension& dimension : model.Dimensions)
    {
        if (!dimension.MeasurementWasStated)
        {
            CHECK_NEAR(dimension.MeasurementMm, dimension.GeometryMm, 0.001);
            CHECK(dimension.AgreesWithGeometry);
        }
    }
}

FLOORPLAN_TEST(Annotation, FurnitureIsListedWithItsBlockNameAndTransform)
{
    bool ok = false;
    const BuildingModel model = Plan("Fixtures/annotated_room.dxf", ok);
    CHECK(ok);

    CHECK_EQUAL(model.BlockInstances.size(), std::size_t{1});
    if (model.BlockInstances.empty())
    {
        return;
    }
    const auto& sofa = model.BlockInstances[0];
    CHECK(sofa.BlockName == "FURN_SOFA");
    CHECK(sofa.Layer == "A-FURN");
    CHECK_NEAR(sofa.Position.X, 3000.0, 0.001);
    CHECK_NEAR(sofa.Position.Y, 3000.0, 0.001);
    CHECK_NEAR(sofa.RotationDegrees, 45.0, 0.001);
}

FLOORPLAN_TEST(Annotation, DoorsAndWindowsAreNotCountedAsFurniture)
{
    bool ok = false;
    const BuildingModel model = Plan("Fixtures/door_and_window.dxf", ok);
    CHECK(ok);

    CHECK_EQUAL(model.Openings.size(), std::size_t{2});
    CHECK_MESSAGE(model.BlockInstances.empty(),
                  "a block already understood as an opening must not appear twice");
}

FLOORPLAN_TEST(Annotation, AColumnProfileIsMeasuredButNeverBecomesARoom)
{
    bool ok = false;
    const BuildingModel model = Plan("Fixtures/annotated_room.dxf", ok);
    CHECK(ok);

    CHECK_EQUAL(model.Columns.size(), std::size_t{1});
    CHECK_EQUAL(model.Rooms.size(), std::size_t{1});
    if (model.Columns.empty())
    {
        return;
    }
    const auto& column = model.Columns[0];
    CHECK(column.HasProfile);
    CHECK_NEAR(column.WidthMm, 400.0, 0.001);
    CHECK_NEAR(column.DepthMm, 400.0, 0.001);
    CHECK_NEAR(column.Centre.X, 1200.0, 0.001);
    CHECK_NEAR(column.Centre.Y, 1200.0, 0.001);
    CHECK_NEAR(model.Rooms[0].AreaMm2, 20000000.0, 0.001);
}

FLOORPLAN_TEST(Annotation, GridLinesTakeTheLabelOfTheirNearestBubble)
{
    bool ok = false;
    const BuildingModel model = Plan("Fixtures/annotated_room.dxf", ok);
    CHECK(ok);

    CHECK_EQUAL(model.GridLines.size(), std::size_t{1});
    if (model.GridLines.empty())
    {
        return;
    }
    CHECK(model.GridLines[0].Label == "A");
    CHECK_NEAR(model.GridLines[0].Start.Y, 2000.0, 0.001);
}

FLOORPLAN_TEST(Annotation, APlainPlanReportsASingleElevation)
{
    bool ok = false;
    const BuildingModel model = Plan("Fixtures/single_room.dxf", ok);
    CHECK(ok);

    CHECK_MESSAGE(model.Elevation.IsPlanar(), "a 2D plan sits at one level");
    CHECK_NEAR(model.Elevation.MinimumMm, 0.0, 0.001);
    CHECK_NEAR(model.Elevation.MaximumMm, 0.0, 0.001);
}

FLOORPLAN_TEST(Annotation, AnnotationNeverChangesTheGoldenAreas)
{
    bool ok = false;
    const BuildingModel model = Plan("Fixtures/annotated_room.dxf", ok);
    CHECK(ok);

    CHECK_NEAR(model.TotalRoomAreaMm2(), 20000000.0, 0.001);
    CHECK_NEAR(model.WallFootprintMm2, 3760000.0, 0.001);
    CHECK_EQUAL(model.Walls.size(), std::size_t{4});
}

FLOORPLAN_TEST(Annotation, NothingIsExtractedFromAPlanThatCarriesNone)
{
    bool ok = false;
    const BuildingModel model = Plan("Fixtures/single_room.dxf", ok);
    CHECK(ok);

    CHECK(model.Dimensions.empty());
    CHECK(model.Columns.empty());
    CHECK(model.GridLines.empty());
    CHECK(model.BlockInstances.empty());
    CHECK_EQUAL(model.SkippedHatchPaths, std::size_t{0});
}
