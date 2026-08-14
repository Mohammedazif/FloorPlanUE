#include "Dxf/DxfParser.h"
#include "Dxf/DxfSource.h"
#include "Geometry/BSpline.h"
#include "Geometry/EllipticArc.h"
#include "Geometry/Loop.h"
#include "Model/FloorPlanCompiler.h"
#include "TestHarness.h"

#include <cmath>
#include <string>
#include <vector>

using FloorPlan::Diagnostic;
using FloorPlan::Dxf::DxfDocument;
using FloorPlan::Dxf::DxfEntity;
using FloorPlan::Dxf::DxfEntityType;
using FloorPlan::Dxf::DxfParser;
using FloorPlan::Dxf::DxfSource;
using FloorPlan::Geometry::BSpline;
using FloorPlan::Geometry::EllipticArc;
using FloorPlan::Geometry::Loop;
using FloorPlan::Geometry::LoopVertex;
using FloorPlan::Geometry::SplineCurve;
using FloorPlan::Geometry::Vec2;
using FloorPlan::Model::BuildingModel;
using FloorPlan::Model::CompilerOptions;
using FloorPlan::Model::FloorPlanCompiler;
using FloorPlan::Testing::DataPath;

namespace
{
    constexpr double Pi = 3.14159265358979323846;

    DxfDocument Parse(const std::string& relative, bool& ok)
    {
        DxfDocument document;
        ok = false;
        const DxfSource source = DxfSource::FromFile(DataPath(relative));
        if (!source.IsValid())
        {
            return document;
        }
        const auto reader = source.OpenReader();
        DxfParser parser(*reader);
        ok = parser.Parse(document);
        return document;
    }

    BuildingModel Plan(const std::string& relative, bool& ok)
    {
        BuildingModel model;
        DxfDocument document = Parse(relative, ok);
        if (!ok)
        {
            return model;
        }
        Diagnostic failure;
        ok = FloorPlanCompiler::Compile(document, CompilerOptions{}, model, failure);
        return model;
    }

    const DxfEntity* FirstHatch(const DxfDocument& document)
    {
        for (const DxfEntity& entity : document.ModelSpace)
        {
            if (entity.Type == DxfEntityType::Hatch)
            {
                return &entity;
            }
        }
        return nullptr;
    }

    Loop LoopOf(const DxfEntity& hatch, std::size_t path)
    {
        std::vector<LoopVertex> vertices;
        for (const auto& vertex : hatch.BoundaryLoops[path].Vertices)
        {
            vertices.push_back(LoopVertex{Vec2{vertex.X, vertex.Y}, vertex.Bulge});
        }
        return Loop(std::move(vertices));
    }
}

FLOORPLAN_TEST(HatchBoundary, AnEdgePathOfLinesBecomesItsRectangle)
{
    bool ok = false;
    const DxfDocument document = Parse("Fixtures/hatched_room.dxf", ok);
    CHECK(ok);
    const DxfEntity* hatch = FirstHatch(document);
    CHECK(hatch != nullptr);
    if (hatch == nullptr)
    {
        return;
    }

    CHECK_EQUAL(hatch->BoundaryLoops.size(), std::size_t{3});
    CHECK_EQUAL(hatch->SkippedBoundaryPaths, std::size_t{0});
    CHECK(hatch->BoundaryLoops[0].IsOutermost);
    CHECK_NEAR(LoopOf(*hatch, 0).AbsoluteArea(), 36000000.0, 0.001);
}

FLOORPLAN_TEST(HatchBoundary, InnerPathsAreReadAsWellAsTheOuterOne)
{
    bool ok = false;
    const DxfDocument document = Parse("Fixtures/hatched_room.dxf", ok);
    CHECK(ok);
    const DxfEntity* hatch = FirstHatch(document);
    if (hatch == nullptr || hatch->BoundaryLoops.size() < 3)
    {
        CHECK_MESSAGE(false, "both inner polyline paths must survive");
        return;
    }

    CHECK(!hatch->BoundaryLoops[1].IsOutermost);
    CHECK_NEAR(LoopOf(*hatch, 1).AbsoluteArea(), 31360000.0, 0.001);
    CHECK_NEAR(LoopOf(*hatch, 2).AbsoluteArea(), 810000.0, 0.001);
}

FLOORPLAN_TEST(HatchBoundary, PocheDrawnAsOneHatchYieldsTheRoomItEncloses)
{
    bool ok = false;
    const BuildingModel model = Plan("Fixtures/hatched_room.dxf", ok);
    CHECK(ok);

    CHECK_EQUAL(model.Rooms.size(), std::size_t{1});
    if (model.Rooms.empty())
    {
        return;
    }
    CHECK_MESSAGE(FloorPlan::Testing::NearlyEqual(model.Rooms[0].AreaMm2, 30550000.0, 0.001),
                  "the 5600 room less the 900 column standing in it");
    CHECK_NEAR(model.WallFootprintMm2, 5450000.0, 0.001);
}

FLOORPLAN_TEST(HatchBoundary, HatchFacesPairIntoWallsLikeAnyOtherLoop)
{
    bool ok = false;
    const BuildingModel model = Plan("Fixtures/hatched_room.dxf", ok);
    CHECK(ok);

    CHECK_MESSAGE(model.Walls.size() == 4,
                  "the outer and inner paths are parallel faces 200 apart");
    for (const auto& wall : model.Walls)
    {
        CHECK_NEAR(wall.ThicknessMm, 200.0, 0.001);
    }
}

FLOORPLAN_TEST(HatchBoundary, ACircularEdgeKeepsItsCurveAsABulge)
{
    bool ok = false;
    const DxfDocument document = Parse("Fixtures/hatch_curved_edges.dxf", ok);
    CHECK(ok);
    const DxfEntity* hatch = FirstHatch(document);
    CHECK(hatch != nullptr);
    if (hatch == nullptr || hatch->BoundaryLoops.empty())
    {
        return;
    }

    bool bulged = false;
    for (const auto& vertex : hatch->BoundaryLoops[0].Vertices)
    {
        if (std::fabs(std::fabs(vertex.Bulge) - 1.0) < 0.001)
        {
            bulged = true;
        }
    }
    CHECK_MESSAGE(bulged, "a semicircular arc edge is exactly a bulge of one");
}

FLOORPLAN_TEST(HatchBoundary, EveryCurvedEdgeTypeIsBuilt)
{
    bool ok = false;
    const DxfDocument document = Parse("Fixtures/hatch_curved_edges.dxf", ok);
    CHECK(ok);
    const DxfEntity* hatch = FirstHatch(document);
    CHECK(hatch != nullptr);
    if (hatch == nullptr)
    {
        return;
    }

    CHECK_EQUAL(hatch->BoundaryLoops.size(), std::size_t{1});
    CHECK_EQUAL(hatch->SkippedBoundaryPaths, std::size_t{0});

    const double area = LoopOf(*hatch, 0).AbsoluteArea();
    const double expected = 4000.0 * 4000.0 + Pi * 2000.0 * 2000.0 / 2.0 +
                            Pi * 2000.0 * 1000.0 / 2.0;
    CHECK_MESSAGE(FloorPlan::Testing::NearlyEqual(area, expected, expected * 0.001),
                  "rectangle plus a semicircular bump plus a half ellipse");
}

FLOORPLAN_TEST(HatchBoundary, AnEllipseIsSampledOnItsOwnParameter)
{
    const Vec2 centre{0.0, 0.0};
    const Vec2 major{2000.0, 0.0};

    const Vec2 atZero = EllipticArc::Evaluate(centre, major, 0.5, 0.0);
    const Vec2 atQuarter = EllipticArc::Evaluate(centre, major, 0.5, Pi * 0.5);

    CHECK_NEAR(atZero.X, 2000.0, 0.001);
    CHECK_NEAR(atZero.Y, 0.0, 0.001);
    CHECK_NEAR(atQuarter.X, 0.0, 0.001);
    CHECK_MESSAGE(FloorPlan::Testing::NearlyEqual(atQuarter.Y, 1000.0, 0.001),
                  "the minor axis is the major turned a quarter and scaled by the ratio");
}

FLOORPLAN_TEST(HatchBoundary, ASplineWithAWrongKnotCountIsRejected)
{
    SplineCurve curve;
    curve.Degree = 3;
    curve.ControlPoints = {Vec2{0, 0}, Vec2{1, 2}, Vec2{3, 2}, Vec2{4, 0}};
    curve.Knots = {0, 0, 0, 1, 1, 1};

    CHECK(!BSpline::IsWellFormed(curve));
    Vec2 point;
    CHECK(!BSpline::Evaluate(curve, 0.5, point));
}

FLOORPLAN_TEST(HatchBoundary, ABezierSplinePassesThroughItsEndControlPoints)
{
    SplineCurve curve;
    curve.Degree = 3;
    curve.ControlPoints = {Vec2{0, 0}, Vec2{0, 3000}, Vec2{3000, 3000}, Vec2{3000, 0}};
    curve.Knots = {0, 0, 0, 0, 1, 1, 1, 1};
    CHECK(BSpline::IsWellFormed(curve));

    Vec2 start;
    Vec2 middle;
    Vec2 end;
    CHECK(BSpline::Evaluate(curve, 0.0, start));
    CHECK(BSpline::Evaluate(curve, 0.5, middle));
    CHECK(BSpline::Evaluate(curve, 1.0, end));

    CHECK_NEAR(start.X, 0.0, 0.001);
    CHECK_NEAR(start.Y, 0.0, 0.001);
    CHECK_NEAR(end.X, 3000.0, 0.001);
    CHECK_NEAR(end.Y, 0.0, 0.001);
    CHECK_NEAR(middle.X, 1500.0, 0.001);
    CHECK_MESSAGE(FloorPlan::Testing::NearlyEqual(middle.Y, 2250.0, 0.001),
                  "a cubic Bezier at its midpoint sits three quarters of the way up");
}

FLOORPLAN_TEST(HatchBoundary, ASplineTessellationStartsAndEndsOnTheCurve)
{
    SplineCurve curve;
    curve.Degree = 3;
    curve.ControlPoints = {Vec2{0, 0}, Vec2{0, 3000}, Vec2{3000, 3000}, Vec2{3000, 0}};
    curve.Knots = {0, 0, 0, 0, 1, 1, 1, 1};

    std::vector<Vec2> points;
    BSpline::Tessellate(curve, 16, points);

    CHECK_EQUAL(points.size(), std::size_t{17});
    if (points.size() < 2)
    {
        return;
    }
    CHECK_NEAR(points.front().X, 0.0, 0.001);
    CHECK_NEAR(points.back().X, 3000.0, 0.001);
}
