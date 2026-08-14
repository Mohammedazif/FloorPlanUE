#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"

class UDynamicMesh;

/// One rectangular hole in a wall's elevation, measured along the wall and up from the floor.
struct FFloorPlanOpeningCut
{
    double AlongStartMm = 0.0;
    double AlongEndMm = 0.0;
    double SillMm = 0.0;
    double HeadMm = 0.0;
};

struct FFloorPlanWallShape
{
    FVector2D StartMm = FVector2D::ZeroVector;
    FVector2D EndMm = FVector2D::ZeroVector;

    /// DXF bulge across the centreline: zero is straight, otherwise tan of a quarter sweep.
    double Bulge = 0.0;
    double ThicknessMm = 0.0;
    double HeightMm = 0.0;

    /// Underside of the wall, below the finished floor, so it overlaps the slab it stands on.
    double BaseMm = 0.0;

    TArray<FFloorPlanOpeningCut> Openings;
};

struct FFloorPlanMeshReport
{
    int32 Boxes = 0;
    int32 Triangles = 0;
    int32 OpeningsApplied = 0;
    bool bDegenerate = false;

    /// Distance along the wall's centreline, which for a curved wall exceeds its chord.
    double LengthMm = 0.0;

    /// Non-zero means the solid has a hole in it; a sealed wall must report zero.
    int32 OpenBoundaryEdges = 0;
};

/// Builds watertight wall prisms and room floors without relying on mesh booleans.
class FLOORPLANUNREAL_API FFloorPlanMeshBuilder
{
public:
    /// Millimetres to Unreal centimetres.
    static constexpr double MillimetreToUnreal = 0.1;

    /// Mesh is local to the chord's midpoint with +X along it; OutTransform places it.
    static bool BuildWall(const FFloorPlanWallShape& Shape, UE::Geometry::FDynamicMesh3& Mesh,
                          FTransform& OutTransform, FFloorPlanMeshReport& Report);

    /// Mesh is local to the boundary's centre; contained voids are not cut.
    static bool BuildFloor(const TArray<FVector2D>& BoundaryMm, double ThicknessMm,
                           UE::Geometry::FDynamicMesh3& Mesh, FTransform& OutTransform,
                           FFloorPlanMeshReport& Report);

    /// Straight extrusion of a closed boundary between two heights, local to its centre.
    static bool BuildPrism(const TArray<FVector2D>& BoundaryMm, double BaseMm, double TopMm,
                           UE::Geometry::FDynamicMesh3& Mesh, FTransform& OutTransform,
                           FFloorPlanMeshReport& Report);

    static void CopyToDynamicMesh(const UE::Geometry::FDynamicMesh3& Source,
                                  UDynamicMesh* Target);
};
