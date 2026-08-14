#include "Diagnostic.h"
#include "Dxf/DxfBinaryTagReader.h"
#include "Dxf/DxfSource.h"
#include "TestHarness.h"

#include <filesystem>
#include <string>
#include <vector>

using FloorPlan::Diagnostic;
using FloorPlan::DiagnosticCode;
using FloorPlan::Dxf::DxfSource;
using FloorPlan::Dxf::DxfTag;
using FloorPlan::Testing::DataPath;

namespace
{
    struct DrainResult
    {
        std::size_t TagCount = 0;
        Diagnostic Failure;
        bool SawEndOfFile = false;
        bool ConsumedAll = false;
        bool Opened = false;
    };

    DrainResult Drain(const std::string& path)
    {
        DrainResult result;
        const DxfSource source = DxfSource::FromFile(path);
        if (!source.IsValid())
        {
            result.Failure = source.Failure();
            return result;
        }
        const auto reader = source.OpenReader();
        if (!reader)
        {
            return result;
        }
        result.Opened = true;
        DxfTag tag;
        while (reader->Next(tag))
        {
            ++result.TagCount;
        }
        result.Failure = reader->Failure();
        result.SawEndOfFile = reader->SawEndOfFile();
        result.ConsumedAll = reader->ConsumedEntireInput();
        return result;
    }

    bool Resolved(const DrainResult& result)
    {
        return result.Failure.IsFailure() || result.SawEndOfFile;
    }

    std::vector<std::filesystem::path> DxfFilesIn(const std::string& relative)
    {
        std::vector<std::filesystem::path> files;
        std::error_code error;
        const std::filesystem::path root = DataPath(relative);
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
            if (extension == ".dxf")
            {
                files.push_back(entry.path());
            }
        }
        return files;
    }
}

FLOORPLAN_TEST(DxfTagReader, AsciiFixtureParsesToEndOfFile)
{
    const DrainResult result = Drain(DataPath("Fixtures/single_room.dxf"));
    CHECK(result.Opened);
    CHECK_MESSAGE(!result.Failure.IsFailure(), FloorPlan::Format(result.Failure));
    CHECK(result.SawEndOfFile);
    CHECK(result.ConsumedAll);
    CHECK(result.TagCount > 100);
}

FLOORPLAN_TEST(DxfTagReader, BinaryFixtureIsDetectedAndParses)
{
    const DxfSource source = DxfSource::FromFile(DataPath("Fixtures/single_room_binary.dxf"));
    CHECK(source.IsValid());
    CHECK(source.IsBinary());

    const DrainResult result = Drain(DataPath("Fixtures/single_room_binary.dxf"));
    CHECK_MESSAGE(!result.Failure.IsFailure(), FloorPlan::Format(result.Failure));
    CHECK(result.SawEndOfFile);
    CHECK(result.ConsumedAll);
}

FLOORPLAN_TEST(DxfTagReader, AsciiAndBinaryAgreeOnTagCount)
{
    const DrainResult ascii = Drain(DataPath("Fixtures/single_room.dxf"));
    const DrainResult binary = Drain(DataPath("Fixtures/single_room_binary.dxf"));
    CHECK(ascii.SawEndOfFile);
    CHECK(binary.SawEndOfFile);
    CHECK_EQUAL(ascii.TagCount, binary.TagCount);
}

FLOORPLAN_TEST(DxfTagReader, GroupCodeWidthSwitchesAtRelease13)
{
    struct Expectation
    {
        const char* File;
        int Width;
    };
    const Expectation expectations[] = {
        {"Versions/bin_dxf_r12.dxf", 1},
        {"Versions/diamond-bin.dxf", 1},
        {"Versions/bin_dxf_r13.dxf", 2},
        {"Versions/bin_dxf_r14.dxf", 2},
        {"Versions/bin_dxf_r2000.dxf", 2},
        {"Fixtures/single_room_binary.dxf", 2},
    };

    for (const Expectation& expectation : expectations)
    {
        const DxfSource source = DxfSource::FromFile(DataPath(expectation.File));
        CHECK_MESSAGE(source.IsValid(), expectation.File);
        CHECK_MESSAGE(source.IsBinary(), expectation.File);
        const auto reader = source.OpenReader();
        const auto* binary =
            dynamic_cast<const FloorPlan::Dxf::DxfBinaryTagReader*>(reader.get());
        CHECK_MESSAGE(binary != nullptr, expectation.File);
        if (binary != nullptr)
        {
            CHECK_MESSAGE(binary->GroupCodeWidth() == expectation.Width, expectation.File);
        }
    }
}

FLOORPLAN_TEST(DxfTagReader, EmptyFileIsRejectedBeforeAnyAllocation)
{
    const DxfSource source = DxfSource::FromFile(DataPath("Malformed/Generated/empty.dxf"));
    CHECK(!source.IsValid());
    CHECK(source.Failure().Code == DiagnosticCode::FileEmpty);
}

FLOORPLAN_TEST(DxfTagReader, NonNumericGroupCodeIsRejectedWithLocation)
{
    const DrainResult result =
        Drain(DataPath("Malformed/Generated/non_numeric_group_code.dxf"));
    CHECK(result.Failure.Code == DiagnosticCode::InvalidGroupCode);
    CHECK(result.Failure.LineNumber > 0);
    CHECK(!result.SawEndOfFile);
}

FLOORPLAN_TEST(DxfTagReader, NonFiniteCoordinateIsRejectedAtPointOfRead)
{
    const DrainResult nan = Drain(DataPath("Malformed/Generated/nan_coordinate.dxf"));
    CHECK(nan.Failure.Code == DiagnosticCode::NonFiniteValue);
    CHECK(!nan.SawEndOfFile);

    const DrainResult mid =
        Drain(DataPath("Malformed/Generated/nan_coordinate_second_vertex.dxf"));
    CHECK(mid.Failure.Code == DiagnosticCode::NonFiniteValue);

    const DrainResult infinite = Drain(DataPath("Malformed/Generated/infinite_coordinate.dxf"));
    CHECK_MESSAGE(infinite.Failure.IsFailure(), FloorPlan::Format(infinite.Failure));
}

FLOORPLAN_TEST(DxfTagReader, OversizedStringIsRejectedByLineBound)
{
    const DrainResult result =
        Drain(DataPath("Malformed/Generated/enormous_string_value.dxf"));
    CHECK(result.Failure.IsFailure());
    CHECK(result.Failure.Code == DiagnosticCode::LineTooLong);
}

FLOORPLAN_TEST(DxfTagReader, MissingEndOfFileMarkerIsDetected)
{
    const DrainResult result = Drain(DataPath("Malformed/Generated/no_eof.dxf"));
    CHECK(result.Failure.Code == DiagnosticCode::MissingEndOfFileMarker);
    CHECK(!result.SawEndOfFile);
}

FLOORPLAN_TEST(DxfTagReader, TruncationNeverParsesClean)
{
    const char* truncated[] = {
        "Malformed/Generated/truncate_at_20_bytes.dxf",
        "Malformed/Generated/truncate_half.dxf",
        "Malformed/Generated/truncate_mid_entity.dxf",
        "Malformed/Generated/binary_truncated.dxf",
    };
    for (const char* relative : truncated)
    {
        const DrainResult result = Drain(DataPath(relative));
        CHECK_MESSAGE(result.Failure.IsFailure(), relative);
        CHECK_MESSAGE(!result.SawEndOfFile, relative);
    }
}

FLOORPLAN_TEST(DxfTagReader, PlainTextIsRejected)
{
    const DrainResult result = Drain(DataPath("Malformed/Generated/not_dxf.dxf"));
    CHECK(result.Failure.Code == DiagnosticCode::InvalidGroupCode);
}

FLOORPLAN_TEST(DxfTagReader, EveryGeneratedAdversarialFileReachesADefinedState)
{
    const auto files = DxfFilesIn("Malformed/Generated");
    CHECK(files.size() >= 18);
    for (const auto& file : files)
    {
        const DrainResult result = Drain(file.string());
        CHECK_MESSAGE(Resolved(result) || !result.Opened, file.filename().string());
    }
}

FLOORPLAN_TEST(DxfTagReader, EveryRealWorldBrokenFileReachesADefinedState)
{
    const auto files = DxfFilesIn("Malformed");
    CHECK(files.size() >= 17);
    for (const auto& file : files)
    {
        const DrainResult result = Drain(file.string());
        CHECK_MESSAGE(Resolved(result) || !result.Opened, file.filename().string());
    }
}

FLOORPLAN_TEST(DxfTagReader, EveryVersionFileReachesADefinedState)
{
    const auto files = DxfFilesIn("Versions");
    CHECK(files.size() >= 12);
    for (const auto& file : files)
    {
        const DrainResult result = Drain(file.string());
        CHECK_MESSAGE(Resolved(result) || !result.Opened, file.filename().string());
    }
}

FLOORPLAN_TEST(DxfTagReader, EveryRealWorldFileParsesToEndOfFile)
{
    const auto files = DxfFilesIn("RealWorld");
    CHECK(files.size() >= 14);
    for (const auto& file : files)
    {
        const DrainResult result = Drain(file.string());
        CHECK_MESSAGE(result.SawEndOfFile,
                      file.filename().string() + ": " + FloorPlan::Format(result.Failure));
    }
}

FLOORPLAN_TEST(DxfTagReader, EveryFixtureParsesToEndOfFile)
{
    const auto files = DxfFilesIn("Fixtures");
    CHECK(files.size() >= 13);
    for (const auto& file : files)
    {
        const DrainResult result = Drain(file.string());
        CHECK_MESSAGE(result.SawEndOfFile,
                      file.filename().string() + ": " + FloorPlan::Format(result.Failure));
        CHECK_MESSAGE(result.ConsumedAll, file.filename().string());
    }
}
