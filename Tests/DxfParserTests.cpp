#include "Dxf/DxfDocument.h"
#include "Dxf/DxfParser.h"
#include "Dxf/DxfSource.h"
#include "TestHarness.h"

#include <filesystem>
#include <string>

using FloorPlan::Diagnostic;
using FloorPlan::DiagnosticCode;
using FloorPlan::Dxf::DxfDocument;
using FloorPlan::Dxf::DxfEntity;
using FloorPlan::Dxf::DxfEntityType;
using FloorPlan::Dxf::DxfParser;
using FloorPlan::Dxf::DxfSource;
using FloorPlan::Testing::DataPath;

namespace
{
    struct Parsed
    {
        DxfDocument Document;
        Diagnostic Failure;
        bool Ok = false;
    };

    Parsed ParseFixture(const std::string& relative)
    {
        Parsed parsed;
        const DxfSource source = DxfSource::FromFile(DataPath(relative));
        if (!source.IsValid())
        {
            parsed.Failure = source.Failure();
            return parsed;
        }
        const auto reader = source.OpenReader();
        DxfParser parser(*reader);
        parsed.Ok = parser.Parse(parsed.Document);
        parsed.Failure = parser.Failure();
        return parsed;
    }

    std::size_t CountOfType(const DxfDocument& document, DxfEntityType type)
    {
        std::size_t count = 0;
        for (const DxfEntity& entity : document.ModelSpace)
        {
            if (entity.Type == type)
            {
                ++count;
            }
        }
        return count;
    }

    const DxfEntity* FirstOfType(const DxfDocument& document, DxfEntityType type)
    {
        for (const DxfEntity& entity : document.ModelSpace)
        {
            if (entity.Type == type)
            {
                return &entity;
            }
        }
        return nullptr;
    }
}

FLOORPLAN_TEST(DxfParser, SingleRoomHasTwoClosedWallLoops)
{
    const Parsed parsed = ParseFixture("Fixtures/single_room.dxf");
    CHECK_MESSAGE(parsed.Ok, FloorPlan::Format(parsed.Failure));
    CHECK_EQUAL(CountOfType(parsed.Document, DxfEntityType::LwPolyline), std::size_t{2});

    const DxfEntity* first = FirstOfType(parsed.Document, DxfEntityType::LwPolyline);
    CHECK(first != nullptr);
    if (first != nullptr)
    {
        CHECK_EQUAL(first->Vertices.size(), std::size_t{4});
        CHECK(first->Closed);
        CHECK_EQUAL(first->Layer, std::string("A-WALL"));
        CHECK_NEAR(first->Vertices[0].X, 0.0, 1e-9);
        CHECK_NEAR(first->Vertices[1].X, 5000.0, 1e-9);
        CHECK_NEAR(first->Vertices[2].Y, 4000.0, 1e-9);
    }
}

FLOORPLAN_TEST(DxfParser, HeaderCarriesVersionAndUnits)
{
    const Parsed parsed = ParseFixture("Fixtures/single_room.dxf");
    CHECK(parsed.Ok);
    CHECK_EQUAL(parsed.Document.Version, std::string("AC1024"));
    CHECK(parsed.Document.HasInsertUnits);
    CHECK_EQUAL(parsed.Document.InsertUnits, 4);
    CHECK_NEAR(FloorPlan::Dxf::MillimetresPerUnit(parsed.Document.InsertUnits), 1.0, 1e-12);
}

FLOORPLAN_TEST(DxfParser, ReleaseTwelveCarriesNoInsertUnits)
{
    const Parsed parsed = ParseFixture("Fixtures/single_room_r12.dxf");
    CHECK_MESSAGE(parsed.Ok, FloorPlan::Format(parsed.Failure));
    CHECK_EQUAL(parsed.Document.Version, std::string("AC1009"));
    CHECK_MESSAGE(!parsed.Document.HasInsertUnits, "R12 does not export $INSUNITS");
}

FLOORPLAN_TEST(DxfParser, ReleaseTwelvePolylineCarriesItsVertices)
{
    const Parsed parsed = ParseFixture("Fixtures/single_room_r12.dxf");
    CHECK(parsed.Ok);
    CHECK_EQUAL(CountOfType(parsed.Document, DxfEntityType::Polyline), std::size_t{2});

    const DxfEntity* first = FirstOfType(parsed.Document, DxfEntityType::Polyline);
    CHECK(first != nullptr);
    if (first != nullptr)
    {
        CHECK_EQUAL(first->Vertices.size(), std::size_t{4});
        CHECK(first->Closed);
        CHECK_NEAR(first->Vertices[1].X, 5000.0, 1e-9);
        CHECK_NEAR(first->Vertices[2].Y, 4000.0, 1e-9);
    }
}

FLOORPLAN_TEST(DxfParser, BulgeIsReadFromTheVertexThatCarriesIt)
{
    const Parsed parsed = ParseFixture("Fixtures/arc_wall.dxf");
    CHECK_MESSAGE(parsed.Ok, FloorPlan::Format(parsed.Failure));

    const DxfEntity* first = FirstOfType(parsed.Document, DxfEntityType::LwPolyline);
    CHECK(first != nullptr);
    if (first != nullptr)
    {
        CHECK_EQUAL(first->Vertices.size(), std::size_t{4});
        CHECK_NEAR(first->Vertices[0].Bulge, 0.0, 1e-12);
        CHECK_NEAR(first->Vertices[1].Bulge, 1.0, 1e-12);
        CHECK_NEAR(first->Vertices[2].Bulge, 0.0, 1e-12);
        CHECK_NEAR(first->Vertices[3].Bulge, 0.0, 1e-12);
        CHECK_NEAR(first->Vertices[1].X, 5000.0, 1e-9);
        CHECK_NEAR(first->Vertices[1].Y, 0.0, 1e-9);
    }
}

FLOORPLAN_TEST(DxfParser, BlocksAndInsertsAreRecovered)
{
    const Parsed parsed = ParseFixture("Fixtures/door_and_window.dxf");
    CHECK_MESSAGE(parsed.Ok, FloorPlan::Format(parsed.Failure));
    CHECK_EQUAL(CountOfType(parsed.Document, DxfEntityType::Insert), std::size_t{2});

    CHECK(parsed.Document.FindBlock("DOOR_900") != nullptr);
    CHECK(parsed.Document.FindBlock("WIN_1200") != nullptr);

    for (const DxfEntity& entity : parsed.Document.ModelSpace)
    {
        if (entity.Type != DxfEntityType::Insert)
        {
            continue;
        }
        if (entity.BlockName == "DOOR_900")
        {
            CHECK_NEAR(entity.InsertX, 1500.0, 1e-9);
            CHECK_NEAR(entity.InsertY, -100.0, 1e-9);
            CHECK_NEAR(entity.RotationDegrees, 0.0, 1e-9);
            CHECK_EQUAL(entity.Layer, std::string("A-DOOR"));
        }
        else if (entity.BlockName == "WIN_1200")
        {
            CHECK_NEAR(entity.InsertX, 5100.0, 1e-9);
            CHECK_NEAR(entity.InsertY, 1400.0, 1e-9);
            CHECK_NEAR(entity.RotationDegrees, 90.0, 1e-9);
            CHECK_EQUAL(entity.Layer, std::string("A-GLAZ"));
        }
    }
}

FLOORPLAN_TEST(DxfParser, TextLabelsCarryValueAndAnchor)
{
    const Parsed parsed = ParseFixture("Fixtures/labeled_rooms.dxf");
    CHECK_MESSAGE(parsed.Ok, FloorPlan::Format(parsed.Failure));
    CHECK_EQUAL(CountOfType(parsed.Document, DxfEntityType::Text), std::size_t{3});

    bool sawBedroom = false;
    bool sawOutside = false;
    for (const DxfEntity& entity : parsed.Document.ModelSpace)
    {
        if (entity.Type != DxfEntityType::Text)
        {
            continue;
        }
        CHECK_EQUAL(entity.Layer, std::string("A-ANNO"));
        if (entity.Text == "Bedroom 1")
        {
            sawBedroom = true;
            CHECK_NEAR(entity.AnchorX(), 2000.0, 1e-9);
            CHECK_NEAR(entity.AnchorY(), 1500.0, 1e-9);
            CHECK_NEAR(entity.TextHeight, 200.0, 1e-9);
        }
        else if (entity.Text == "NORTH ELEVATION")
        {
            sawOutside = true;
            CHECK_NEAR(entity.AnchorY(), -1500.0, 1e-9);
        }
    }
    CHECK(sawBedroom);
    CHECK(sawOutside);
}

FLOORPLAN_TEST(DxfParser, LayerTableIsRecovered)
{
    const Parsed parsed = ParseFixture("Fixtures/single_room.dxf");
    CHECK(parsed.Ok);
    CHECK(parsed.Document.Layers.size() >= 6);

    bool sawWall = false;
    for (const auto& layer : parsed.Document.Layers)
    {
        if (layer.Name == "A-WALL")
        {
            sawWall = true;
        }
    }
    CHECK(sawWall);
}

FLOORPLAN_TEST(DxfParser, RoomInRoomYieldsFourLoops)
{
    const Parsed parsed = ParseFixture("Fixtures/room_in_room.dxf");
    CHECK_MESSAGE(parsed.Ok, FloorPlan::Format(parsed.Failure));
    CHECK_EQUAL(CountOfType(parsed.Document, DxfEntityType::LwPolyline), std::size_t{4});
}

FLOORPLAN_TEST(DxfParser, BinaryAndAsciiProduceTheSameModelSpace)
{
    const Parsed ascii = ParseFixture("Fixtures/single_room.dxf");
    const Parsed binary = ParseFixture("Fixtures/single_room_binary.dxf");
    CHECK(ascii.Ok);
    CHECK_MESSAGE(binary.Ok, FloorPlan::Format(binary.Failure));
    CHECK_EQUAL(ascii.Document.ModelSpace.size(), binary.Document.ModelSpace.size());

    for (std::size_t index = 0; index < ascii.Document.ModelSpace.size() &&
                                index < binary.Document.ModelSpace.size();
         ++index)
    {
        const DxfEntity& left = ascii.Document.ModelSpace[index];
        const DxfEntity& right = binary.Document.ModelSpace[index];
        CHECK(left.Type == right.Type);
        CHECK_EQUAL(left.Layer, right.Layer);
        CHECK_EQUAL(left.Vertices.size(), right.Vertices.size());
        for (std::size_t vertex = 0;
             vertex < left.Vertices.size() && vertex < right.Vertices.size(); ++vertex)
        {
            CHECK_NEAR(left.Vertices[vertex].X, right.Vertices[vertex].X, 1e-12);
            CHECK_NEAR(left.Vertices[vertex].Y, right.Vertices[vertex].Y, 1e-12);
            CHECK_NEAR(left.Vertices[vertex].Bulge, right.Vertices[vertex].Bulge, 1e-12);
        }
    }
}

FLOORPLAN_TEST(DxfParser, ReleaseTwelveAndModernAgreeOnGeometry)
{
    const Parsed modern = ParseFixture("Fixtures/single_room.dxf");
    const Parsed legacy = ParseFixture("Fixtures/single_room_r12.dxf");
    CHECK(modern.Ok);
    CHECK(legacy.Ok);

    const DxfEntity* modernFirst = FirstOfType(modern.Document, DxfEntityType::LwPolyline);
    const DxfEntity* legacyFirst = FirstOfType(legacy.Document, DxfEntityType::Polyline);
    CHECK(modernFirst != nullptr);
    CHECK(legacyFirst != nullptr);
    if (modernFirst != nullptr && legacyFirst != nullptr)
    {
        CHECK_EQUAL(modernFirst->Vertices.size(), legacyFirst->Vertices.size());
        for (std::size_t index = 0; index < modernFirst->Vertices.size(); ++index)
        {
            CHECK_NEAR(modernFirst->Vertices[index].X, legacyFirst->Vertices[index].X, 1e-9);
            CHECK_NEAR(modernFirst->Vertices[index].Y, legacyFirst->Vertices[index].Y, 1e-9);
        }
    }
}

FLOORPLAN_TEST(DxfParser, AbsurdCoordinateIsRejectedByRangeBound)
{
    const Parsed parsed = ParseFixture("Malformed/Generated/absurd_coordinate.dxf");
    CHECK(!parsed.Ok);
    CHECK(parsed.Failure.Code == DiagnosticCode::ValueOutOfRange);
}

FLOORPLAN_TEST(DxfParser, DeclaredVertexCountIsNeverTrusted)
{
    const char* files[] = {
        "Malformed/Generated/huge_vertex_count.dxf",
        "Malformed/Generated/negative_vertex_count.dxf",
        "Malformed/Generated/understated_vertex_count.dxf",
    };
    for (const char* relative : files)
    {
        const Parsed parsed = ParseFixture(relative);
        CHECK_MESSAGE(parsed.Ok, std::string(relative) + ": " + FloorPlan::Format(parsed.Failure));
        const DxfEntity* first = FirstOfType(parsed.Document, DxfEntityType::LwPolyline);
        CHECK_MESSAGE(first != nullptr, relative);
        if (first != nullptr)
        {
            CHECK_MESSAGE(first->Vertices.size() == 4, relative);
        }
    }
}

FLOORPLAN_TEST(DxfParser, EveryFixtureParsesWithoutDiagnostic)
{
    const char* fixtures[] = {
        "Fixtures/single_room.dxf",         "Fixtures/two_rooms_shared_wall.dxf",
        "Fixtures/l_shaped_room.dxf",       "Fixtures/room_in_room.dxf",
        "Fixtures/door_and_window.dxf",     "Fixtures/labeled_rooms.dxf",
        "Fixtures/varying_thickness.dxf",   "Fixtures/arc_wall.dxf",
        "Fixtures/single_room_r12.dxf",     "Fixtures/single_room_binary.dxf",
    };
    for (const char* relative : fixtures)
    {
        const Parsed parsed = ParseFixture(relative);
        CHECK_MESSAGE(parsed.Ok, std::string(relative) + ": " + FloorPlan::Format(parsed.Failure));
        CHECK_MESSAGE(!parsed.Document.ModelSpace.empty(), relative);
    }
}

FLOORPLAN_TEST(DxfParser, EveryRealWorldFileParsesWithoutDiagnostic)
{
    std::error_code error;
    const std::filesystem::path root = DataPath("RealWorld");
    std::size_t seen = 0;
    for (const auto& entry : std::filesystem::directory_iterator(root, error))
    {
        if (!entry.is_regular_file(error))
        {
            continue;
        }
        std::string extension = entry.path().extension().string();
        for (char& character : extension)
        {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }
        if (extension != ".dxf")
        {
            continue;
        }
        ++seen;
        const DxfSource source = DxfSource::FromFile(entry.path().string());
        CHECK_MESSAGE(source.IsValid(), entry.path().filename().string());
        if (!source.IsValid())
        {
            continue;
        }
        const auto reader = source.OpenReader();
        DxfDocument document;
        DxfParser parser(*reader);
        const bool ok = parser.Parse(document);
        CHECK_MESSAGE(ok, entry.path().filename().string() + ": " +
                              FloorPlan::Format(parser.Failure()));
    }
    CHECK(seen >= 14);
}
