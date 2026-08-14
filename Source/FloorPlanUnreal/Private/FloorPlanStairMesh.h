#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "FloorPlanMeshBuilder.h"

/// A straight flight, measured in millimetres along the run it fills.
struct FFloorPlanStairShape
{
    FVector2D StartMm = FVector2D::ZeroVector;
    FVector2D EndMm = FVector2D::ZeroVector;
    double WidthMm = 0.0;
    double TreadDepthMm = 0.0;
    double RiserHeightMm = 0.0;
    int32 StepCount = 0;

    /// Underside of the flight, matching the slab the bottom step stands on.
    double BaseMm = 0.0;
};

/// Builds a flight as one solid block per step, so every step is watertight on its own.
class FFloorPlanStairMesh
{
public:
    /// Mesh is local to the run's midpoint with +X climbing; OutTransform places it.
    static bool Build(const FFloorPlanStairShape& Shape, UE::Geometry::FDynamicMesh3& Mesh,
                      FTransform& OutTransform, FFloorPlanMeshReport& Report);
};
