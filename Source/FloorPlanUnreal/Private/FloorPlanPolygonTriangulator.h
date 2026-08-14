#pragma once

#include "CoreMinimal.h"
#include "IndexTypes.h"

/// Ear-clipping triangulation of one simple closed boundary.
class FFloorPlanPolygonTriangulator
{
public:
    /// Emits counter-clockwise triangles indexing Boundary; false when no ear could be cut.
    static bool Triangulate(const TArray<FVector2D>& Boundary,
                            TArray<UE::Geometry::FIndex3i>& OutTriangles);
};
