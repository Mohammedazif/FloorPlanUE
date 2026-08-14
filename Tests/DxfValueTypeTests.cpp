#include "Dxf/DxfValueType.h"
#include "TestHarness.h"

using FloorPlan::Dxf::ClassifyGroupCode;
using FloorPlan::Dxf::DxfValueType;

FLOORPLAN_TEST(DxfValueType, CoordinatesAreReal)
{
    CHECK(ClassifyGroupCode(10) == DxfValueType::Real);
    CHECK(ClassifyGroupCode(20) == DxfValueType::Real);
    CHECK(ClassifyGroupCode(30) == DxfValueType::Real);
    CHECK(ClassifyGroupCode(40) == DxfValueType::Real);
    CHECK(ClassifyGroupCode(42) == DxfValueType::Real);
    CHECK(ClassifyGroupCode(50) == DxfValueType::Real);
    CHECK(ClassifyGroupCode(59) == DxfValueType::Real);
}

FLOORPLAN_TEST(DxfValueType, FlagsAndCountsAreIntegers)
{
    CHECK(ClassifyGroupCode(70) == DxfValueType::Int16);
    CHECK(ClassifyGroupCode(66) == DxfValueType::Int16);
    CHECK(ClassifyGroupCode(90) == DxfValueType::Int32);
    CHECK(ClassifyGroupCode(91) == DxfValueType::Int32);
    CHECK(ClassifyGroupCode(160) == DxfValueType::Int64);
    CHECK(ClassifyGroupCode(1071) == DxfValueType::Int32);
    CHECK(ClassifyGroupCode(1070) == DxfValueType::Int16);
}

FLOORPLAN_TEST(DxfValueType, NamesAndHandlesAreText)
{
    CHECK(ClassifyGroupCode(0) == DxfValueType::Text);
    CHECK(ClassifyGroupCode(1) == DxfValueType::Text);
    CHECK(ClassifyGroupCode(2) == DxfValueType::Text);
    CHECK(ClassifyGroupCode(3) == DxfValueType::Text);
    CHECK(ClassifyGroupCode(5) == DxfValueType::Text);
    CHECK(ClassifyGroupCode(8) == DxfValueType::Text);
    CHECK(ClassifyGroupCode(100) == DxfValueType::Text);
    CHECK(ClassifyGroupCode(330) == DxfValueType::Text);
    CHECK(ClassifyGroupCode(999) == DxfValueType::Text);
}

FLOORPLAN_TEST(DxfValueType, BooleanAndBinaryRanges)
{
    CHECK(ClassifyGroupCode(290) == DxfValueType::Boolean);
    CHECK(ClassifyGroupCode(299) == DxfValueType::Boolean);
    CHECK(ClassifyGroupCode(310) == DxfValueType::BinaryChunk);
    CHECK(ClassifyGroupCode(1004) == DxfValueType::BinaryChunk);
}

FLOORPLAN_TEST(DxfValueType, ClassificationIsConstexpr)
{
    static_assert(ClassifyGroupCode(10) == DxfValueType::Real, "coordinate");
    static_assert(ClassifyGroupCode(70) == DxfValueType::Int16, "flags");
    static_assert(ClassifyGroupCode(0) == DxfValueType::Text, "entity name");
    CHECK(true);
}
