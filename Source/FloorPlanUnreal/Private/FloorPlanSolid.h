#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "FloorPlanMeshBuilder.h"

/// Shared primitives for building sealed solids out of millimetre dimensions.
class FFloorPlanSolid
{
public:
    /// Millimetres are scaled to Unreal units here, so callers stay in drawing dimensions.
    static void AppendBox(UE::Geometry::FDynamicMesh3& Mesh, double AlongMin, double AlongMax,
                          double AcrossMin, double AcrossMax, double UpMin, double UpMax,
                          FFloorPlanMeshReport& Report);

    /// Callers pass right-handed order, where (B-A)x(C-A) is the outward normal. Unreal's
    /// rasterizer treats the reverse as front-facing, so the swap happens here and only here.
    static void AppendTriangle(UE::Geometry::FDynamicMesh3& Mesh, int32 A, int32 B, int32 C,
                               FFloorPlanMeshReport& Report);

    static int32 CountOpenBoundaryEdges(const UE::Geometry::FDynamicMesh3& Mesh);
};
