#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"

/// The circle a curved wall was swept around, in Unreal units. Left default for straight walls.
struct FFloorPlanSweptArc
{
    FVector2D Centre = FVector2D::ZeroVector;
    double Radius = 0.0;
    double StartAngle = 0.0;
    double Sweep = 0.0;
    double Handedness = 1.0;
    bool bCurved = false;
};

/// Per-face planar UVs, projected in the frame the wall was swept along so curves do not seam.
class FFloorPlanMeshUVs
{
public:
    /// One UV unit per metre, so a tiling material holds its real-world scale on every element.
    static constexpr double UvUnitsPerUnrealUnit = 0.01;

    static void Project(UE::Geometry::FDynamicMesh3& Mesh, const FFloorPlanSweptArc& Sweep);
};
