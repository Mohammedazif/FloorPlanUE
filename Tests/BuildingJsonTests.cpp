#include "Dxf/DxfParser.h"
#include "Dxf/DxfSource.h"
#include "Model/BuildingJson.h"
#include "Model/FloorPlanCompiler.h"
#include "Model/RoomGraph.h"
#include "Model/StoreyLink.h"
#include "TestHarness.h"

#include <string>
#include <vector>

using FloorPlan::Diagnostic;
using FloorPlan::Dxf::DxfDocument;
using FloorPlan::Dxf::DxfParser;
using FloorPlan::Dxf::DxfSource;
using FloorPlan::Model::BuildingJson;
using FloorPlan::Model::BuildingModel;
using FloorPlan::Model::CompilerOptions;
using FloorPlan::Model::FloorPlanCompiler;
using FloorPlan::Model::Room;
using FloorPlan::Model::RoomGraph;
using FloorPlan::Testing::DataPath;

namespace
{
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

    bool Contains(const std::string& haystack, const std::string& needle)
    {
        return haystack.find(needle) != std::string::npos;
    }

    std::size_t Count(const std::string& haystack, const std::string& needle)
    {
        std::size_t total = 0;
        std::size_t at = haystack.find(needle);
        while (at != std::string::npos)
        {
            ++total;
            at = haystack.find(needle, at + needle.size());
        }
        return total;
    }

    std::string Export(const std::string& relative, bool& ok)
    {
        const BuildingModel model = Compile(relative, ok);
        if (!ok)
        {
            return std::string();
        }
        return BuildingJson::Storey(model, RoomGraph::Build(model), "Ground", 0.0);
    }
}

FLOORPLAN_TEST(BuildingJson, EveryElementIdAppearsInTheExport)
{
    bool ok = false;
    const BuildingModel model = Compile("Fixtures/two_rooms_shared_wall.dxf", ok);
    CHECK(ok);
    const std::string json = BuildingJson::Storey(model, RoomGraph::Build(model), "Ground", 0.0);

    for (const auto& room : model.Rooms)
    {
        CHECK_MESSAGE(Contains(json, room.Id), "room " + room.Id + " missing from the export");
    }
    for (const auto& wall : model.Walls)
    {
        CHECK_MESSAGE(Contains(json, wall.Id), "wall " + wall.Id + " missing from the export");
    }
}

FLOORPLAN_TEST(BuildingJson, AreasSurviveTheRoundTripToTextExactly)
{
    bool ok = false;
    const std::string json = Export("Fixtures/arc_wall.dxf", ok);
    CHECK(ok);

    CHECK_MESSAGE(Contains(json, "26283185.307179585"),
                  "the arc room's area must not be rounded on the way out");
}

FLOORPLAN_TEST(BuildingJson, WholeNumbersAreNotWrittenInExponentialForm)
{
    bool ok = false;
    const std::string json = Export("Fixtures/single_room.dxf", ok);
    CHECK(ok);

    CHECK(Contains(json, "20000000"));
    CHECK_MESSAGE(!Contains(json, "e+"), "plain magnitudes must stay readable");
}

FLOORPLAN_TEST(BuildingJson, LabelsWithQuotesAndNewlinesAreEscaped)
{
    BuildingModel model;
    model.UnassignedLabels.push_back("He said \"stop\"\nthen\tleft\\");

    const std::string json = BuildingJson::Storey(model, RoomGraph::Build(model), "Ground", 0.0);

    CHECK(Contains(json, "\\\"stop\\\""));
    CHECK(Contains(json, "\\n"));
    CHECK(Contains(json, "\\t"));
    CHECK(Contains(json, "\\\\"));
}

FLOORPLAN_TEST(BuildingJson, ControlCharactersBecomeUnicodeEscapes)
{
    BuildingModel model;
    model.UnassignedLabels.push_back(std::string("bell\x07here"));

    const std::string json = BuildingJson::Storey(model, RoomGraph::Build(model), "Ground", 0.0);

    CHECK(Contains(json, "\\u0007"));
}

FLOORPLAN_TEST(BuildingJson, AnEmptyPlanStillProducesEverySection)
{
    const BuildingModel model;
    const std::string json = BuildingJson::Storey(model, RoomGraph::Build(model), "Ground", 0.0);

    CHECK(Contains(json, "\"rooms\": []"));
    CHECK(Contains(json, "\"walls\": []"));
    CHECK(Contains(json, "\"openings\": []"));
    CHECK(Contains(json, "\"adjacency\": []"));
}

FLOORPLAN_TEST(BuildingJson, CurvedWallsReportArcLengthNotChordLength)
{
    bool ok = false;
    const BuildingModel model = Compile("Fixtures/arc_wall.dxf", ok);
    CHECK(ok);
    const std::string json = BuildingJson::Storey(model, RoomGraph::Build(model), "Ground", 0.0);

    CHECK_MESSAGE(Contains(json, "6597.34"),
                  "the semicircular wall is 2100 mm radius over half a turn");
}

FLOORPLAN_TEST(BuildingJson, AStairIsReportedWithTheTreadsItWasDrawnWith)
{
    bool ok = false;
    const std::string json = Export("Fixtures/two_storey_ground.dxf", ok);
    CHECK(ok);

    CHECK(Contains(json, "\"kind\": \"stair\""));
    CHECK(Contains(json, "\"drawnTreads\": 17"));
}

FLOORPLAN_TEST(BuildingJson, VerticalConnectionsAreListedAtBuildingLevel)
{
    bool ok = false;
    const BuildingModel ground = Compile("Fixtures/two_storey_ground.dxf", ok);
    CHECK(ok);
    const BuildingModel first = Compile("Fixtures/two_storey_first.dxf", ok);
    CHECK(ok);

    const auto links = FloorPlan::Model::StoreyLink::Between(ground, first);
    CHECK_EQUAL(links.size(), std::size_t{1});
    if (links.empty())
    {
        return;
    }

    const std::string connection =
        BuildingJson::Connection(ground, first, "Ground", "First", links[0]);
    const std::string building = BuildingJson::Building({}, {connection});

    CHECK(Contains(building, "\"verticalConnections\""));
    CHECK(Contains(building, "\"fromStorey\": \"Ground\""));
    CHECK(Contains(building, "\"toStorey\": \"First\""));
    CHECK(Contains(building, ground.Rooms[links[0].LowerRoom].Id));
}

FLOORPLAN_TEST(BuildingJson, EverythingAnnotatedOnThePlanReachesTheExport)
{
    bool ok = false;
    const std::string json = Export("Fixtures/annotated_room.dxf", ok);
    CHECK(ok);

    CHECK(Contains(json, "\"dimensions\""));
    CHECK(Contains(json, "\"columns\""));
    CHECK(Contains(json, "\"grid\""));
    CHECK(Contains(json, "\"blocks\""));
    CHECK(Contains(json, "\"FURN_SOFA\""));
    CHECK(Contains(json, "\"label\": \"A\""));
    CHECK(Contains(json, "\"planar\": true"));
}

FLOORPLAN_TEST(BuildingJson, ADisagreeingDimensionIsExportedAsSuch)
{
    bool ok = false;
    const std::string json = Export("Fixtures/annotated_room.dxf", ok);
    CHECK(ok);

    CHECK(Contains(json, "\"agreesWithGeometry\": false"));
    CHECK_EQUAL(Count(json, "\"agreesWithGeometry\": true"), std::size_t{1});
}

FLOORPLAN_TEST(BuildingJson, StoreysAreWrappedInOneBuildingDocument)
{
    bool ok = false;
    const std::string ground = Export("Fixtures/single_room.dxf", ok);
    CHECK(ok);
    const std::string first = Export("Fixtures/two_rooms_shared_wall.dxf", ok);
    CHECK(ok);

    const std::string building = BuildingJson::Building({ground, first});

    CHECK(Contains(building, "\"storeys\""));
    CHECK_EQUAL(Count(building, "\"storey\":"), std::size_t{2});
}

FLOORPLAN_TEST(BuildingJson, TheSameStoreyKeyGivesTheSameIdsAndADifferentOneDoesNot)
{
    const DxfSource source = DxfSource::FromFile(DataPath("Fixtures/single_room.dxf"));
    CHECK(source.IsValid());

    const auto MakeIds = [](const std::string& key) {
        const DxfSource file = DxfSource::FromFile(DataPath("Fixtures/single_room.dxf"));
        const auto reader = file.OpenReader();
        DxfDocument document;
        DxfParser parser(*reader);
        parser.Parse(document);
        CompilerOptions options;
        options.StoreyKey = key;
        BuildingModel model;
        Diagnostic failure;
        FloorPlanCompiler::Compile(document, options, model, failure);
        std::vector<std::string> ids;
        for (const auto& room : model.Rooms)
        {
            ids.push_back(room.Id);
        }
        for (const auto& wall : model.Walls)
        {
            ids.push_back(wall.Id);
        }
        return ids;
    };

    const std::vector<std::string> ground = MakeIds("Ground");
    const std::vector<std::string> repeat = MakeIds("Ground");
    const std::vector<std::string> upper = MakeIds("First");

    CHECK(ground == repeat);
    CHECK_MESSAGE(ground != upper,
                  "identical storeys of one building must not collide on identity");
}
