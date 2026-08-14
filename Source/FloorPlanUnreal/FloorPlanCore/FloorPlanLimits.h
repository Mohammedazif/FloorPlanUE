#pragma once

#include <cstddef>
#include <cstdint>

namespace FloorPlan::Limits
{
    inline constexpr std::size_t MaxFileBytes = 512u * 1024u * 1024u;
    inline constexpr std::size_t MaxGroupCodeLineBytes = 4096;
    inline constexpr std::size_t MaxStringValueBytes = 8192;
    inline constexpr std::size_t MaxTagCount = 8'000'000;
    inline constexpr std::size_t MaxEntityCount = 2'000'000;
    inline constexpr std::size_t MaxVerticesPerPolyline = 1'048'576;
    inline constexpr std::size_t MaxBlockDefinitionCount = 65'536;
    inline constexpr std::size_t MaxBlockRecursionDepth = 32;
    inline constexpr std::size_t MaxLayerCount = 65'536;

    inline constexpr int LowestValidGroupCode = 0;
    inline constexpr int HighestValidGroupCode = 1071;

    inline constexpr std::size_t BinarySentinelBytes = 22;
    inline constexpr std::size_t BinaryChunkLengthMax = 127;
    inline constexpr std::size_t BinaryNarrowGroupCodeProbeBytes = 24;
    inline constexpr int BinaryExtendedDataEscape = 255;

    inline constexpr double MaxCoordinateMm = 1.0e6;
    inline constexpr double MaxRadiusMm = 1.0e6;
    inline constexpr double MaxBulgeMagnitude = 1.0e4;
    inline constexpr double MaxTextHeightMm = 1.0e5;
    inline constexpr double MaxInsertScaleMagnitude = 1.0e4;
    inline constexpr double MinInsertScaleMagnitude = 1.0e-6;

    inline constexpr std::size_t MaxCurveSegments = 4096;
    inline constexpr std::size_t MaxSplineDegree = 15;
    inline constexpr std::size_t SplineSamplesPerSpan = 12;
    inline constexpr std::size_t MaxHatchTagCount = 1'000'000;
    inline constexpr std::size_t MaxHatchBoundaryPaths = 4096;
    inline constexpr std::size_t MaxHatchEdgesPerPath = 65'536;

    inline constexpr double CoordinateQuantumMm = 1.0 / 256.0;
    inline constexpr double VertexWeldToleranceMm = 1.0;
    inline constexpr double EdgeProximityToleranceMm = 1.0;
    inline constexpr double WeldClusterDiameterMaxMm = 2.0;
    inline constexpr double MinEdgeLengthMm = 2.0;
    inline constexpr double CollinearDeviationMm = 0.25;
    inline constexpr double ArcTessellationSagittaMm = 0.25;
    inline constexpr double ZeroChordLengthMm = 1.0e-9;
    inline constexpr double ZeroBulgeMagnitude = 1.0e-12;
    inline constexpr double AreaEpsilonMm2 = 1.0e-6;

    inline constexpr std::size_t SnapFixpointPassesMax = 8;
    inline constexpr std::size_t ArrangementIntersectionsMax = 2'000'000;

    inline constexpr double DefaultSingleLineWallThicknessMm = 100.0;
    inline constexpr double MinWallThicknessMm = 40.0;
    inline constexpr double MaxWallThicknessMm = 800.0;
    inline constexpr double MinWallOverlapMm = 100.0;
    inline constexpr double MaxWallPairSineDeviation = 0.02;
    inline constexpr double MaxWallArcCentreOffsetMm = 2.0;
    inline constexpr double MaxWallArcSweepRadians = 6.0;

    inline constexpr double RoomProbeOffsetMm = 10.0;
    inline constexpr std::size_t RoomProbesPerWall = 5;

    inline constexpr double TargetRiserHeightMm = 175.0;
    inline constexpr double MinRiserHeightMm = 100.0;
    inline constexpr double MaxRiserHeightMm = 250.0;
    inline constexpr std::size_t MaxStepsPerFlight = 60;
    inline constexpr std::size_t CirculationOverlapSamples = 8;
    inline constexpr double MinCirculationOverlapFraction = 0.4;

    inline constexpr std::size_t MaxWallCount = 500'000;
    inline constexpr std::size_t MaxRoomCount = 100'000;
    inline constexpr std::size_t MaxOpeningCount = 500'000;

    inline constexpr double IdentityQuantumMm = 1.0 / 256.0;
    inline constexpr std::uint64_t IdentityHashOffsetBasis = 14695981039346656037ull;
    inline constexpr std::uint64_t IdentityHashPrime = 1099511628211ull;

    inline constexpr double DefaultDoorHeadHeightMm = 2100.0;
    inline constexpr double DefaultWindowSillHeightMm = 900.0;
    inline constexpr double DefaultWindowHeadHeightMm = 2100.0;
    inline constexpr double DefaultWallHeightMm = 2700.0;
    inline constexpr double FloorSlabThicknessMm = 200.0;
}
