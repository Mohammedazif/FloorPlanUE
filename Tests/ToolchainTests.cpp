#include "Diagnostic.h"
#include "FloorPlanLimits.h"
#include "TestHarness.h"

#include <cstdint>
#include <filesystem>
#include <limits>

FLOORPLAN_TEST(Toolchain, DiagnosticFormatsCodeAndLocation)
{
    FloorPlan::Diagnostic diagnostic;
    diagnostic.Code = FloorPlan::DiagnosticCode::InvalidGroupCode;
    diagnostic.LineNumber = 23;
    diagnostic.Message = "NOTACODE";

    CHECK(diagnostic.IsFailure());
    CHECK_EQUAL(FloorPlan::Format(diagnostic), std::string("InvalidGroupCode at line 23: NOTACODE"));
}

FLOORPLAN_TEST(Toolchain, DefaultDiagnosticIsNotAFailure)
{
    const FloorPlan::Diagnostic diagnostic;
    CHECK(!diagnostic.IsFailure());
}

FLOORPLAN_TEST(Toolchain, SixtyFourBitArithmeticIsExactForOrientationBudget)
{
    const std::int64_t extent =
        static_cast<std::int64_t>(FloorPlan::Limits::MaxCoordinateMm /
                                  FloorPlan::Limits::CoordinateQuantumMm);
    const std::int64_t span = 2 * extent;
    const std::int64_t product = span * span;

    CHECK(extent == 256'000'000);
    CHECK(product > 0);
    CHECK(product < (std::numeric_limits<std::int64_t>::max)() / 8);
}

FLOORPLAN_TEST(Toolchain, TestDataIsReachable)
{
    const std::string fixture = FloorPlan::Testing::DataPath("Fixtures/single_room.dxf");
    CHECK_MESSAGE(std::filesystem::exists(fixture), fixture);
}
