#include "Dxf/DxfBlockExpander.h"
#include "Dxf/DxfParser.h"
#include "Dxf/DxfSource.h"
#include "TestHarness.h"

#include <cmath>
#include <string>
#include <vector>

using FloorPlan::Diagnostic;
using FloorPlan::DiagnosticCode;
using FloorPlan::Dxf::DxfBlockExpander;
using FloorPlan::Dxf::DxfDocument;
using FloorPlan::Dxf::DxfEntity;
using FloorPlan::Dxf::DxfEntityType;
using FloorPlan::Dxf::DxfParser;
using FloorPlan::Dxf::DxfSource;
using FloorPlan::Testing::DataPath;

namespace
{
    struct Expanded
    {
        DxfDocument Document;
        std::vector<DxfEntity> Entities;
        Diagnostic Failure;
        bool ParsedOk = false;
        bool ExpandedOk = false;
    };

    Expanded Run(const std::string& relative)
    {
        Expanded result;
        const DxfSource source = DxfSource::FromFile(DataPath(relative));
        if (!source.IsValid())
        {
            result.Failure = source.Failure();
            return result;
        }
        const auto reader = source.OpenReader();
        DxfParser parser(*reader);
        result.ParsedOk = parser.Parse(result.Document);
        if (!result.ParsedOk)
        {
            result.Failure = parser.Failure();
            return result;
        }
        result.ExpandedOk =
            DxfBlockExpander::Expand(result.Document, result.Entities, result.Failure);
        return result;
    }
}

FLOORPLAN_TEST(DxfBlockExpander, SelfReferencingBlockIsRefused)
{
    const Expanded result = Run("Malformed/Generated/self_referencing_block.dxf");
    CHECK(result.ParsedOk);
    CHECK(!result.ExpandedOk);
    CHECK(result.Failure.Code == DiagnosticCode::BlockReferenceCycle);
}

FLOORPLAN_TEST(DxfBlockExpander, MutualBlockCycleIsRefused)
{
    const Expanded result = Run("Malformed/Generated/mutual_block_cycle.dxf");
    CHECK(result.ParsedOk);
    CHECK(!result.ExpandedOk);
    CHECK(result.Failure.Code == DiagnosticCode::BlockReferenceCycle);
}

FLOORPLAN_TEST(DxfBlockExpander, DanglingReferenceIsSkippedNotFatal)
{
    const Expanded result = Run("Malformed/Generated/dangling_block_reference.dxf");
    CHECK(result.ParsedOk);
    CHECK_MESSAGE(result.ExpandedOk, FloorPlan::Format(result.Failure));
    CHECK_EQUAL(DxfBlockExpander::UnresolvedReferenceCount(result.Document), std::size_t{1});

    std::size_t polylines = 0;
    for (const DxfEntity& entity : result.Entities)
    {
        if (entity.Type == DxfEntityType::LwPolyline)
        {
            ++polylines;
        }
    }
    CHECK_EQUAL(polylines, std::size_t{2});
}

FLOORPLAN_TEST(DxfBlockExpander, DoorBlockLandsAtItsInsertionPoint)
{
    const Expanded result = Run("Fixtures/door_and_window.dxf");
    CHECK(result.ParsedOk);
    CHECK_MESSAGE(result.ExpandedOk, FloorPlan::Format(result.Failure));

    bool sawDoorLeaf = false;
    bool sawDoorSwing = false;
    for (const DxfEntity& entity : result.Entities)
    {
        if (entity.Type == DxfEntityType::Line && entity.Layer == "0")
        {
            const bool atInsert = std::fabs(entity.StartX - 1500.0) < 1e-6 &&
                                  std::fabs(entity.StartY + 100.0) < 1e-6;
            if (atInsert && std::fabs(entity.EndX - 2400.0) < 1e-6)
            {
                sawDoorLeaf = true;
            }
        }
        if (entity.Type == DxfEntityType::Arc && std::fabs(entity.Radius - 900.0) < 1e-6 &&
            std::fabs(entity.CenterX - 1500.0) < 1e-6)
        {
            sawDoorSwing = true;
        }
    }
    CHECK(sawDoorLeaf);
    CHECK(sawDoorSwing);
}

FLOORPLAN_TEST(DxfBlockExpander, RotatedWindowBlockIsRotatedIntoPlace)
{
    const Expanded result = Run("Fixtures/door_and_window.dxf");
    CHECK(result.ExpandedOk);

    bool sawRotatedPane = false;
    for (const DxfEntity& entity : result.Entities)
    {
        if (entity.Type != DxfEntityType::Line)
        {
            continue;
        }
        const bool startsAtInsert = std::fabs(entity.StartX - 5100.0) < 1e-6 &&
                                    std::fabs(entity.StartY - 1400.0) < 1e-6;
        const bool endsRotated = std::fabs(entity.EndX - 5100.0) < 1e-6 &&
                                 std::fabs(entity.EndY - 2600.0) < 1e-6;
        if (startsAtInsert && endsRotated)
        {
            sawRotatedPane = true;
        }
    }
    CHECK_MESSAGE(sawRotatedPane, "WIN_1200 rotated 90 degrees should run along +Y");
}

FLOORPLAN_TEST(DxfBlockExpander, WallGeometrySurvivesExpansionUnchanged)
{
    const Expanded result = Run("Fixtures/single_room.dxf");
    CHECK(result.ExpandedOk);
    CHECK_EQUAL(result.Entities.size(), std::size_t{2});
    for (const DxfEntity& entity : result.Entities)
    {
        CHECK(entity.Type == DxfEntityType::LwPolyline);
        CHECK_EQUAL(entity.Vertices.size(), std::size_t{4});
    }
}

FLOORPLAN_TEST(DxfBlockExpander, EveryRealWorldFileExpandsWithoutCycles)
{
    const char* files[] = {
        "RealWorld/dimension_in_nested_blocks.dxf",
        "RealWorld/insert_bricscad_level_1.dxf",
        "RealWorld/multi_insert_with_attribs.dxf",
        "RealWorld/custom_blocks.dxf",
        "RealWorld/wipeout_door.dxf",
    };
    for (const char* relative : files)
    {
        const Expanded result = Run(relative);
        CHECK_MESSAGE(result.ParsedOk, relative);
        CHECK_MESSAGE(result.ExpandedOk,
                      std::string(relative) + ": " + FloorPlan::Format(result.Failure));
    }
}
